# Bluetooth

El stack es `mtkbt` de stock (BlueAngel de MediaTek sobre STP), no BlueZ.
El kernel tiene `# CONFIG_BT is not set`: no hay sockets `AF_BLUETOOTH`
(RFCOMM, L2CAP, SCO), así que todo lo de CM7 que abre un socket BlueZ falla
con `EAFNOSUPPORT`. La regla del árbol es mantener el stack de stock y poner
puentes chicos entre las APIs de CM7 y `mtkbt`.

## Estado (2026-10-21)

| Función | Estado | Pieza |
|---|---|---|
| Encender/apagar, nombre, visible, clase | OK | `libbt_mtk_gap` + `frameworks_base_bt_mtk.patch` |
| Buscar dispositivos | OK | ídem |
| Emparejar (confirmación numérica) | OK, en los dos sentidos | ídem |
| Nombre del remoto tras emparejar entrante | OK | msg 194 → `onMtkRemoteName` |
| Enviar archivos (OPP cliente) | OK | `libbt_mtk_opp` + `packages_apps_Bluetooth_opp_mtk.patch` |
| Recibir archivos (OPP servidor) | OK | `libbt_mtk_opp` + `BluetoothOppMtkServerSession` |
| Audio A2DP, manos libres (SCO), PAN | Falta | sin socket BlueZ |
| Remoto emparejado entrante en la lista de envío | Falta | falta su `Class` |

## Sockets y mensajes de `mtkbt`

Todos son `SOCK_DGRAM` con `ilm_struct` (`msg_id` + `ilm_data`):

- `/dev/socket/bt.int.adp` (`bluetooth:net_bt` 0660): pedidos a `mtkbt`.
- `@bt.app.gap`: respuestas (CNF) a los pedidos GAP de `libbt_mtk_gap`.
- `@bt.ext.adp`: indicaciones GAP.
- `@bt.ext.adp.opp`: respuestas e indicaciones de OPP (lo crea `libbt_mtk_opp`).

Los ids de este `mtkbt` no coinciden con los headers MTK de JB que hay en
GitHub. Los que usamos (verificados en el teléfono o en el
`libandroid_runtime.so` de stock):

| id | mensaje | datos (`ilm_data`) |
|---|---|---|
| 100 / 145 | POWERON_REQ / CNF | CNF `result` 0 o 1 = ok |
| 103 | DISCOVERY_REQ | 24 bytes: cod `0xffffffff` en 4, inquiry length en 8, max results en 9 |
| 105 / 151 | WRITE_LOCAL_NAME REQ / CNF | len en 4, nombre en 5 (máx. 59) |
| 109 / 155 | WRITE_SCANENABLE_MODE REQ / CNF | modo (word) en 4: bit 0 inquiry (visible), bit 1 page |
| 113 / 159 | WRITE_LOCAL_COD REQ / CNF | cod (word) en 4, `write_type` 2 en 8 |
| 107 / 153 | READ_REMOTE_NAME REQ / CNF | dirección en 4..11; CNF: result 4, cod 16, len 20, nombre 21 (no usado aún) |
| 147 / 148 / 149 | DISCOVERY_RESULT / UPDATE / CNF | ver `parse_result()` |
| 164 | SECURITY_USER_CONFIRM_IND | dirección 4..11, número ASCII 12..17 |
| 172 | BONDING_RESULT_IND | result en 4, dirección en 8 |
| 186 | LINK_STATE_IND | `ilm_data[5]`: 1 conectado, 0 desconectado |
| 194 | REMOTE_NAME_IND | dirección 4..11, len en 12, nombre desde 13 |

La dirección (`bt_addr_struct`) es `lap` (u32), `uap` (u8), `nap` (u16 en 6).

## Decisiones y detalles

- **Clase del dispositivo.** Stock la escribe desde Java (`setProperty("Class")`);
  CM7 nunca lo hacía y el Y220 anunciaba clase 0. `btmtk_gap_power_on()`
  escribe `0x5a020c` (teléfono, como el `main.conf` de BlueZ).
- **Inquiry de 0x08 × 1,28 s** en vez de los 0x30 de stock: la búsqueda
  propia tapaba la mitad de la ventana de visibilidad.
- **Avisos antes de `boot_completed`.** Con `mtkbt` ya encendido, POWERON
  responde en ms y `EnableThread` mandaba el broadcast de cambio de nombre
  antes del arranque: bootloop. `mtkPropertyChanged` atrapa la excepción.
- **Guardas D-Bus.** Sin BlueZ no hay `ObjectPath`; cualquier llamada JNI con
  ruta nula aborta `system_server` dentro de libdbus (`deadbaad`). Todas las
  funciones de `android_server_BluetoothService.cpp` que llaman D-Bus tienen
  la guarda `get_adapter_path() == NULL` (las últimas cuatro: device
  properties, set device property, discover services, service channel).
- **Nombre del remoto en emparejamiento entrante.** Un dispositivo que se
  empareja sin haber salido en la búsqueda no tiene `Name`; `mtkbt` lo manda
  justo después (msg 194). `BluetoothEventLoop.onMtkRemoteName` lo guarda y
  emite `ACTION_NAME_CHANGED`, como el `PropertyChanged` de BlueZ.

## Envío de archivos (OPP)

`libextopp.so` de stock es C puro (solo depende de `liblog`, `libcutils`,
`libutils`, `libbinder`) y habla con `mtkbt`, que hace SDP, RFCOMM y OBEX.
`libbt_mtk_opp` la carga con `dlopen` (es un blob de vendor) y repite lo que
hace `libextopp_jni` de stock:

1. socket conectado a `/dev/socket/bt.int.adp` → `g_bt_opp_cntx` + 0x560;
2. `socket_local_server("bt.ext.adp.opp")` → `g_bt_opp_cntx` + 0x564;
3. `btmtk_opp_set_jni_callback()`; `startListenMessage()` en su hilo;
4. `btmtk_opp_client_enable()`, `_connect(&addr)`, `_push(mime, nombre, ruta)`.

Eventos de la callback (`bt_opp_ui_event`): 1/2 enable ok/falla, 5 conectado,
6 progreso [enviado, total], 7 push start [total], 8 push ok, 9 push falla
[código OBEX, p. ej. 195 = 0xC3 Forbidden], 16 desconectado.

En la app (`BluetoothOppMtkClientSession`): si `libbt_mtk_opp` carga,
`BluetoothOppTransfer` se salta SDP y RFCOMM. Progreso, estados y el mapeo de
códigos OBEX son los de `BluetoothOppObexClientSession`. `mtkbt` abre el
archivo por ruta: `file://` directo, `content://` por `_data`, y si no hay,
copia temporal en `sdcard/bluetooth/.send`.

`tools/opptest` hace un envío sin la app (como root, con BT encendido):

```bash
adb shell /data/local/tmp/opptest 00:A0:C6:F2:E5:FB image/jpeg foto.jpg /mnt/sdcard/foto.jpg
```

Referencias: headers de BlueAngel para MT6572 en
[rex-xxx/mt6572_x201](https://github.com/rex-xxx/mt6572_x201/tree/master/frameworks/bluetooth/blueangel)
(`btadp_ext/include/bt_opp_comm.h`, `bt_oppc_*.h`). La firma de
`btmtk_opp_client_*` y el arranque salen de descompilar `libextopp_jni.so`
de stock.

## Recepción de archivos (OPP servidor)

Al encender BT, `BluetoothOppService` habilita el servidor de `mtkbt`
(`btmtk_opp_server_enable`, responde 101/102) en lugar del listener RFCOMM de
BlueZ, que fallaba con "Address family not supported". `mtkbt` registra el
servicio OPP en SDP y hace OBEX; `libextopp` escribe el archivo dentro del
proceso de la app.

Eventos del servidor (0x65..0x71): 101/102 enable ok/falla, 106 inicio,
105 progreso [bytes recibidos], 107 recibido [total], 108 falla [código],
112 desconectado, 113 petición [dirección, nombre, MIME, tamaño]. Salen de
descompilar `my_opps_ui_cb` y `opp_server_*` de `libextopp.so`.

`BluetoothOppMtkServerSession` repite `BluetoothOppObexServerSession.onPut()`:
valida nombre y tipo, inserta el share entrante, avisa con la notificación y
espera la respuesta del usuario (o el timeout de AOSP). Al aceptar llama
`btmtk_opp_server_access_response(0, {tamaño, ruta})` con la ruta completa que
da `BluetoothOppReceiveFileInfo` (`/sdcard/bluetooth/<nombre>`); al rechazar,
código 0x43, como la app de stock (`OppServiceNative.oppsAccessResponse`).
Los objetos siguientes de la misma conexión se aceptan solos, como en AOSP.

Probado el 2026-10-21: un JPEG de 205599 bytes desde otro teléfono llegó
completo a `/mnt/sdcard/bluetooth/`, estado 200 y escaneado por el media
scanner.

## Pendientes

- **Clase del remoto en emparejamiento entrante.** `BluetoothSettings`
  (filtro `TRANSFER`) solo lista dispositivos con UUID de OPP o con una clase
  compatible, y el remoto entrante no tiene ninguna: hay que ponerlo visible
  para enviarle. La clase viene en los eventos de emparejamiento (o en
  `READ_REMOTE_NAME_CNF`); hay un volcado temporal de 164/172 en
  `libbt_mtk_gap` sin commitear para ver el offset.
- **A2DP**: `mtkbt` lleva AVDTP; stock manda SBC por `/dev/socket/bt.a2dp.stream`
  desde `libmtka2dp`.
- **Manos libres**: la voz del MT6572 va por BTCVSD, no por SCO sobre HCI.
- Alternativa descartada: BlueZ con el driver `hci_stp.c` de MediaTek
  (Ubuntu Touch krillin/vegetahd, AsteroidOS MT6580). Funciona en esos
  equipos pero no es lo que usa stock y reemplazaría `mtkbt` entero.
