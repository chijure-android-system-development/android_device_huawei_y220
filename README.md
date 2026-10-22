# Huawei Y220-U05, CM7

MediaTek MT6572, panel ILI9488 320x480, NAND UBIFS. No es el Y210.

El teléfono corre CM7 instalado desde el zip de este árbol
(`cm-7-20261022`) con el kernel `#95` (boot flasheado aparte el 2026-10-22) y el recovery TWRP 3.1.1-0.1. Algunos
cambios posteriores se probaron con actualización puntual sobre `/system` y
quedan documentados abajo.

## Qué es este árbol

`lunch cyanogen_y220-eng` selecciona el producto. `breakfast y220` elige
`eng` aunque `timed_build.sh` pida `userdebug`. El `kernel` prebuilt es el
`#95` (3848496 bytes, netfilter, sin interfaz de firmware táctil), con sus módulos en `prebuilt/modules`.
`prebuilt/ramdisk-stock.img` es el ramdisk de boot de stock (612053 bytes,
gzip cpio, sin cabecera MTK); solo lo usa `pack-bootimg.sh`. El boot del zip
lleva el ramdisk de CM7 (`init.rc` de este árbol).

`pack-bootimg.sh` arma un boot de 6291456 bytes con `mtkbootimg`. No escribe
el teléfono. El `mkbootimg` de CM7 no agrega las cabeceras MTK `KERNEL` /
`ROOTFS`, así que `make bootimage` no es una imagen de boot flasheable.

`system`, `data` y `cache` son UBIFS (`ubi0_0`, `ubi1_0`, `ubi2_0`). El
`system.img` de este build es yaffs y no se escribe en `mtd14`. No flashear
`recovery.img`: el recovery del teléfono es TWRP. `recovery.fstab` existe
para que el target `bacon` esté definido. Gingerbread no monta `ubifs`.

`BoardConfig.mk` deja dos salidas que el árbol de CM7 no tiene para MT6572:

- `BOARD_USES_ALSA_AUDIO`: `libaudio` de `hardware/msm7k` solo se compila
  en plataformas Qualcomm. Sin esto `libaudioflinger` no enlaza.
- `USE_CAMERA_STUB`: no hay `libcamera.so` para este equipo. El servicio
  enlaza `libcamerastub`. La cámara real no está porteada.

## Parches

`build/` y `vendor/cyanogen` son checkouts aparte. Lo que este port cambia
ahí vive en [`patches/`](patches/). En este árbol ya están aplicados. No
correr `apply-patches.sh` otra vez.

```bash
cd /path/to/cm7
bash device/huawei/y220/patches/apply-patches.sh
bash device/huawei/y220/patches/revert-patches.sh
```

- `vendor_cyanogen_products.patch` y `cyanogen_y220.mk`: `lunch` solo importa
  `vendor/cyanogen/products/cyanogen_$CM_BUILD.mk`. Ese makefile hereda
  `device/huawei/y220/cyanogen_y220.mk`.
- `build_releasetools_ubifs.patch`: agrega `"ubifs": "MTD"` a
  `PARTITION_TYPES` en `build/tools/releasetools/common.py`. Sin eso
  `ota_from_target_files` muere en `FormatPartition` con `KeyError: 'ubifs'`
  y `bacon` no sale. El script edify queda `format("ubifs", "MTD", ...)`,
  que el recovery de Gingerbread no sabe ejecutar. No convierte el
  `system.img` a UBIFS ni formatea el NAND.
- `frameworks_base_libutils_tracer.patch`: `libutils` compila
  `BOARD_LIBUTILS_EXTRA_SRC`. El cuerpo es `libutils/Tracer.cpp` de este
  árbol. `libMali.so` necesita `android::Tracer::init` y CM7 no lo tiene.
  El stub deja el tag en 0, así que no escribe atrace.
- `system_core_liblog_extra.patch`: el `liblog` compartido hace
  whole-static de `BOARD_LIBLOG_EXTRA_STATIC` (`libxlog`).
  `libpalwlan_mtk.so` resuelve `__xlog_buf_printf` ahí.
- `WIFI_FIRMWARE_LOADER=wmt_wifi_loader`: el servicio del device enciende
  WMT, espera hasta 4 s a que aparezca `wlan0` y publica `ok` o `failed` al
  HAL Wi-Fi genérico. El `insmod` de `wlan_mt6582.ko` no crea la interfaz.
- `external_wpa_supplicant_6_mtk_wext.patch`: puente único con
  `libwpa_mtk_wext`; la política de scan, filtrado, RSSI y velocidad vive en
  el device tree.
- `frameworks_base_wifi_dormant.patch`: un estado DORMANT sin asociación
  no baja `wlan0`.
- `frameworks_base_bt_mtk.patch`: puente único desde el framework a la
  librería compartida `libbt_mtk_gap` del device tree. Implementa encendido,
  discovery y pairing sin depender del adaptador BlueZ.
- `frameworks_base_phonefactory_mediatek.patch` y
  `system_core_property_ccmni.patch`: datos móviles. Ver Radio.
- `frameworks_base_libutils_ics_compat.patch`: agrega únicamente
  `String8::format`, `String8::formatV` y `Thread::join`, de ICS.
  `PropertyMap` (sin `load()`) vive en `libutils/PropertyMap.cpp` del device
  y entra en `libutils.so` mediante `BOARD_LIBUTILS_EXTRA_SRC`. Los piden
  `camera.default.so` y las `libcam.*` de stock.
- `external_jpeg_mtk_compat.patch`: `jpeg_compress_struct` lleva al final
  los dos arrays de MTK (`for_huff_opt`, `last_coffs_zero`) y mide 432
  bytes como en stock; sin eso `jpeg_CreateCompress` de `libJpgEncPipe`
  falla por tamaño. Agrega `jpeg_mem_dest` (de libjpeg 8). Toda
  `libjpeg` y sus usuarios (Skia) se recompilan con ese struct.
- `system_core_libcutils_mtk_compat.patch`: permite agregar fuentes mediante
  `BOARD_LIBCUTILS_EXTRA_SRC`. Las implementaciones `pmem_*` sobre
  `/dev/pmem_multimedia` y el stub `dump_backtrace_to_file` viven en
  `libcutils/` dentro del device tree.
  El 2026-10-13 se instaló este `libcutils.so`: Android completó el arranque,
  quedaron activos system_server, mediaserver, RIL, Wi-Fi y mtkbt; la cámara
  tomó `IMG_20261013_133755.jpg`, Wi-Fi siguió asociado y no hubo tombstones
  nuevos. El anterior está en
  `/data/local/tmp/libcutils.so.before-device-sources`.
- `frameworks_base_camera_mtk_client.patch`: deja un hook JNI pequeño. El
  adaptador `libcamera_compat/camera_compat.cpp` resuelve el
  `Camera::takePicture(int)` del blob sin cambiar la clase `Camera` de CM7.
  El ejecutable `mediaserver` propio del device reserva el tamaño ABI del
  `CameraService` de stock.
- `frameworks_base_surfaceflinger_yv12.patch`: `LayerBuffer` detecta YV12 y
  llama al conversor RGB565 de `surfaceflinger/Yv12Converter.cpp` antes de
  subir la textura. La conversión y su buffer viven en el device tree.

## Compilación

Dentro del contenedor `cm7-builder` (el árbol del host está en
`/home/builder/cm7`):

```bash
docker exec cm7-builder bash -c 'cd /home/builder/cm7 && DEVICE=y220 ./timed_build.sh'
```

La primera pasada que terminó (2026-10-10, log
`out/timed_build_20261010-002724.log`, 156 s sobre el árbol ya compilado)
dejó:

`out/target/product/y220/cm-7-20261009-UNOFFICIAL-y220.zip`

54850231 bytes, md5 `812970171412c3f31fe4f9a7e7b68927`.

El zip que sí empaqueta el script de UBIFS y el boot con cabecera MTK es
`out/target/product/y220/cm-7-20261010-UNOFFICIAL-y220.zip`
(98241612 bytes, md5 `d2f754e1b8b2e537ef80970fd3f57dc2`, log
`out/timed_build_20261010-013702.log`). `boot.img` dentro del zip pesa
3731456 bytes: magia `ANDROID!`, y en la página 2048 la cabecera MTK
`88168858` seguida del zImage (`0000a0e1`). `system.img` suelto sigue
siendo yaffs. `recovery.img` es el de CM7, no TWRP, y el script no lo
escribe. Ninguno se flasheó. El rollback sigue siendo el backup de TWRP.

Después de mover las compatibilidades de cámara, `libutils` y `libcutils`
al device tree, un `make bacon` completo terminó el 2026-10-13. Generó
`out/target/product/y220/cm-7-20261013-UNOFFICIAL-y220.zip`, 96 MB, MD5
`83ef8df718dcaae1e7c204d4795822de`. Se comprobó que el ZIP contiene el
mediaserver propio, `libbt_mtk_gap.so`, las bibliotecas reconstruidas y el
loader Wi-Fi; su updater conserva UBIFS y no instala el recovery. Este ZIP
se instaló completo desde TWRP 3.1.1-0.1 el 2026-10-13. El updater terminó
con `RC=0`, escribió `boot`, Android completó el arranque y `/system` quedó
montado como UBIFS de solo lectura. Se verificaron los hashes de mediaserver,
libcutils, libutils, libstagefright, libsurfaceflinger y libbt_mtk_gap contra
el contenido del ZIP. system_server, mediaserver, RIL, wpa_supplicant y mtkbt
permanecieron activos; Wi-Fi se asoció a DEPA2 a 65 Mb/s, la app Cámara abrió
y no aparecieron errores fatales ni tombstones nuevos.

## System de stock completo

`/home/chijure/y220/system.ubifs` es el `/system` de C482B206SP05 ya extraído
(256 MB, `ro.build.version.incremental=C482B206SP05`, 12 ene 2014). El árbol
`stock-C482B206SP05/system` es el mismo release pero incompleto: no tiene
`lib/hw` ni `lib/egl` (281 `.so` contra 295). `extract-files.py` sigue
leyendo ese dump incompleto. `gralloc.mt6572.so` y los EGL de Mali se
copiaron después desde `system.ubifs`, no desde ese dump.

`lib/hw`:

| archivo | bytes |
| --- | --- |
| `lights.default.so` | 9772 |
| `gralloc.default.so` | 14272 |
| `gralloc.mt6572.so` | 13856 |
| `sensors.default.so` | 9788 |
| `gps.default.so` | 5208 |
| `camera.default.so` | 48052 |

`lights.default.so` escribe `/sys/class/leds/lcd-backlight/brightness` y
también button, keyboard, jogball y los led red/green/blue. Es el HAL de
brillo de stock. `ro.hardware` es `mt6572`, así que el loader también
acepta `lights.default.so`.

`lib/egl/egl.cfg` tiene `0 0 android` y `0 1 mali`. Al lado están
`libGLES_android.so` y `libEGL_mali.so`, `libGLESv1_CM_mali.so`,
`libGLESv2_mali.so`. `libMali.so` y `libUMP.so` están en `lib/`.

`lib/modules/wlan.ko` sigue siendo un enlace a
`wlan_mtk_connsys_mt6572.ko`, que este árbol tampoco tiene.
`wlan_mt6582.ko` pesa 359972 bytes.

## Vendors

Salen del dump `stock-C482B206SP05/system`, no de `system.ubifs` ni del
teléfono en vivo:

```bash
python3 device/huawei/y220/extract-files.py
```

Deja 292 archivos (46 MB) en `vendor/huawei/y220/proprietary` y la lista en
`proprietary-files.txt`. `device_y220.mk` hereda `y220-vendor.mk`, así que el
próximo `bacon` los copia a `system/`. No se flasheó nada.

Entran GPU (`libMali.so`, `libUMP.so`, `lib/egl/libEGL_mali.so`,
`libGLESv1_CM_mali.so`, `libGLESv2_mali.so`, `lib/hw/gralloc.mt6572.so`),
RIL (`mtk-ril.so`, `librilmtk.so`, `rild`, `ccci_fsd`, `ccci_mdinit`),
cámara, OMX, audio MTK, sensores, NVRam, `etc/firmware` (incluido
`modem.img`) y los `.ko` reconstruidos contra el kernel del teléfono.
No entran las apps de Google ni el odex de Settings/Phone.

El dump no tiene `lib/egl` ni `lib/hw`. Esos salen de `system.ubifs` (TWRP,
mismo build SP05), listados en `FULL_FILES` de `extract-files.py`:
`libEGL_mali.so`, `libGLESv1_CM_mali.so`, `libGLESv2_mali.so`,
`gralloc.mt6572.so`, `camera.default.so` y `sensors.default.so`.
`libcamera_client.so` y `libcameraservice.so` de stock están en
`OVERRIDE_LIBS`: se copian encima de los de CM7 (el build avisa
`overriding commands for target`, es esperado).

`egl.cfg` no es un blob: vive en este device tree (`0 0 android`,
`0 1 mali`) y se copia a `/system/lib/egl/egl.cfg`. Sin ese archivo el
loader de Gingerbread se queda en `libGLES_android.so`.

Los jar `com.mediatek.framework.jar` y `mediatek-common.jar` son stubs de
4 KB; el código está en el `.odex`, atado al framework de stock, y no
carga en CM7.

`lib/modules/wlan_mt6582.ko` de este vendor pesa 838824 bytes y
`cfg80211.ko` 195536. Son los reconstruidos que cargan en el kernel `#57`.
Los `.ko.stock` y `mtk_stp_bt.ko.prev` que a veces quedan al lado no entran
al makefile: el `insmod` de esos de stock panic.

`lib/modules/wlan.ko` es un enlace roto a `wlan_mtk_connsys_mt6572.ko`, que
el dump no tiene. No se copió.

## Zip instalable desde TWRP

El script no formatea el volumen. Monta el UBIFS que ya existe
(`/dev/ubi0_0`), borra el árbol de `/system` y copia los archivos. El
`boot.img` lo escribe `/sbin/flash_image` de TWRP, con cabeceras MTK
`KERNEL` y `ROOTFS`. El `recovery.img` del zip no se escribe: el teléfono
sigue con TWRP.

`init.rc` monta `ubifs ubi@system`, `ubi@userdata` y `ubi@cache`. El
`init` de CM7 aprende ese prefijo con
`patches/system_core_init_ubi.patch`.

Eso no convierte el `system.img` suelto en UBIFS. Flashear ese img a
mtd14 sigue rompiendo la partición. El camino es el zip, desde TWRP, y
el rollback sigue siendo el backup.

## Qué está corriendo en el teléfono

Kernel `#57`, sin cambios de GPU. `/dev/mali` (10,48) y `/dev/ump` (244,0)
ya vienen en esa imagen. Nacen `root:root 600`. `init.rc` y `07wmt` los
dejan `666 system:graphics` antes de `class_start`. El `chmod` de `/dev/ump`
está comentado en el `init.rc` de stock; sin él `libUMP` no abre el nodo.

`ro.hardware` es `mt6572`, así que el HAL carga `gralloc.mt6572.so` y no
`gralloc.default.so`. SurfaceFlinger reporta renderer `Mali-400 MP`,
OpenGL ES-CM 1.1, superficie 8-8-8-8, panel `mtkfb` 320x480 con el azul
en el bit 0. `libGLES_android.so` sigue cargado porque `egl.cfg` lo deja
como implementación 0. La que pinta es la 1, Mali.

`libMali.so` no enlaza contra el `libutils` de CM7 hasta que el parche de
Tracer está aplicado. Sin ese símbolo `gralloc.mt6572.so` no carga y
SurfaceFlinger se cae al abrir el framebuffer.

La Mali no implementa `eglSetSwapRectangleANDROID`. El wrapper de CM7
(`frameworks/base/opengl/libs/EGL/egl.cpp`) responde `EGL_BAD_DISPLAY`
cuando el puntero es nulo. Sale una vez en el log, en el primer frame, y
el compose sigue. `ro.opengles.version=131072` está en `system.prop`
(OpenGL ES 2.0, igual que el stock). El `build.prop` del teléfono no se
regeneró con ese valor.

El combo no pasó por un cambio de kernel. `07wmt` carga `mtk_stp_wmt`,
`mtk_stp_bt`, `mtk_wmt_wifi`, `cfg80211`, `wlan_mt6582` y deja
`6620_launcher -m 3` vivo. `mtkbt` arranca en `sys.boot_completed=1`.

## Wi-Fi

En `gl_wext.c` de este driver, `SIOCSIWPRIV` escribe el código de país.
El combo scan de Android manda ese ioctl con la cabecera `CSCAN`, así que
el scan real (`SIOCSIWSCAN`) no sale. `BOARD_WEXT_NO_COMBO_SCAN` lo evita.
`libwpa_mtk_wext` sube `IFF_UP` en `DRIVER START`, saca la UI de
`WPA_IDLE` y filtra el BSS falso `NVRAM WARNING`. El país `EU` es de este
equipo (canales 1-13); otro puerto cambia las dos letras del `Android.mk`.

`/data/misc/dhcp` no existía y `dhcpcd` no arrancaba. El directorio ya
está en el teléfono (`0770 dhcp:dhcp`). `init.rc` lo crea, pero eso solo
entra con una imagen de boot nueva. No se ha flasheado.

Ajustes lista y asocia. No escribir la MAC ni el país en NVRAM: la MAC de
Wi-Fi sigue saliendo distinta en cada boot.

## Wi-Fi: wpa_supplicant.conf

`extract-files.py` no copia el `etc/wifi/wpa_supplicant.conf` de stock.
Es para el supplicant de JB (`ctrl_interface=DIR=/data/misc/wpa_supplicant`,
`config_methods`, P2P) y el de GB muere con `Failed to initialize control
interface`. Va el de este árbol (`ctrl_interface=wlan0`). Si el teléfono ya
arrancó con el de stock, quedó copiado en `/data/misc/wifi/` y hay que
reemplazarlo (`system:wifi 0660`).

## Bluetooth

Stack `mtkbt` de stock, sin BlueZ (el kernel no tiene `CONFIG_BT`). Encender,
buscar, emparejar en los dos sentidos, nombre/visible y enviar archivos
funcionan; recibir archivos, audio y manos libres no. Mensajes, offsets y
pendientes en [`docs/BLUETOOTH_NOTES.md`](docs/BLUETOOTH_NOTES.md).

La dirección de fábrica ya está en `mtd2` (registro `BT_Addr`, 30 bytes,
no es la de fábrica `00:00:46:65:72:01`). Se copió, sin escribir la
partición, a los dos archivos que el stack lee:

- `/data/BT_Addr`, `0777`, `root:root`. Lo abre `BT_open` en
  `stp_chrdev_bt.c`.
- `/data/nvram/APCFG/APRDEB/BT_Addr`, `0666`, `root:bluetooth`. Lo lee
  `libbluetooth_mtk`.

`nvram_daemon` no está corriendo. No arrancarlo: si el archivo no está,
puede volver a grabar el valor por defecto en la partición.

- `libbt_mtk_gap`: GAP por `bt.int.adp` (encendido, búsqueda, nombre,
  visible, clase `0x5a020c`, emparejamiento, nombre del remoto).
- `frameworks_base_bt_mtk.patch`: conecta esos eventos con
  `BluetoothService`/`BluetoothEventLoop` y protege todas las llamadas D-Bus
  sin adaptador BlueZ (con ruta nula libdbus aborta `system_server`).
- `libbt_mtk_opp` + `packages_apps_Bluetooth_opp_mtk.patch`: la app de CM7
  envía archivos por `mtkbt` con la `libextopp.so` de stock.
- `tools/opptest`: envío por OPP sin la app, para probar `mtkbt`.

## Radio FM

Funciona con la app FMRadio de stock rehecha por `FMRadio/build-fmradio.sh`
y la ruta FM digital en `libaudio`: sintonía, búsqueda, volumen, auriculares
y altavoz. Ver [`docs/FM_NOTES.md`](docs/FM_NOTES.md).

## Hora por red

Stock no trae `/etc/gps.conf`. Sin él, `GpsLocationProvider` pide la hora NTP
a un host nulo (loopback IPv6, sin IPv6 en este kernel) y reintenta cada
5 min. `gps.conf` del árbol solo pone `NTP_SERVER=pool.ntp.org`.

## Radio

Con el kernel `#92` el módem sube solo en frío: SIM `READY`, registrado
en Claro (71610), IMEI leído. La cadena es la de stock:

- `init.d/06modem` carga `sec`, `devapc`, `devinfo`, `ccci_plat` y
  `ccci`, crea `/dev/sec` y `/dev/devmap`, y pone dueños ccci (9996) y
  nvram (9997). El `ueventd` de CM7 solo acepta nombres; con números
  deja `root:root` sin avisar, por eso va en el script.
- `init.rc` monta `protect_f` y `protect_s` (yaffs2). `ccci_fsd` guarda
  ahí el NVRAM del módem (`/protect_f/md`, `/protect_s/md`).
- `ccci_mdinit` espera `nvram_init=Ready` y luego arranca
  `gsm0710muxd`, que crea `/dev/radio/ptty*` y arranca `ril-daemon`.
- `nvram_daemon` corre como root: CM7 solo deja a root poner
  `nvram_init`. Con `/data` limpio restaura desde la BinRegion (lee).
- `rild` necesita `rild.libpath` y `rild.libargs` de `system.prop`.
  `build.prop` no depende de `system.prop` en GB: si cambia, borrar
  `out/target/product/y220/system/build.prop` antes del build.

Copia de mtd1–4 y de `protect_f`/`protect_s` antes de la primera prueba:
`~/y220/backup-nvram-20261013/`.

### Datos móviles

Funcionan en EDGE con Claro (`claro.pe`). El framework AOSP no hace lo que
hace el de MediaTek, así que hay cuatro piezas:

- `apns-conf.xml`: el de CM7 no trae mcc 716. Se agregó Claro PE.
- `ril/.../MediaTekRIL.java` (`ro.telephony.ril_class=mediatek`):
  - El módem arranca con `AT+EGTYPE=0` (attach cuando haga falta) y
    `+CGREG` se queda en `2, 255`. La clase manda
    `RIL_REQUEST_SET_GPRS_CONNECT_TYPE` (2016) con 1 al encender la radio.
  - `+CGREG` llega como URC MTK 3020 (en el `librilmtk` de stock; el
    `ril.h` de MT6572 en GitHub dice 3022). Se reenvía como
    `NETWORK_STATE_CHANGED` para que el framework vuelva a leer GPRS.
  - `SETUP_DATA_CALL` lleva un 8.º string, el cid. Sin él `mtk-ril`
    falla sin mandar ningún AT.
- `mtk-ril` escribe `net.ccmni0.dns1/dns2/gw` (gw = la IP local).
  `system_core_property_ccmni.patch` deja a radio escribir `net.ccmni*`.
- `mobiledata.interfaces` en `system.prop` incluye `ccmni0`; sin eso
  `net.dns1` queda vacío y no resuelve nombres.

`PhoneFactory` de CM7 no carga clases por nombre:
`frameworks_base_phonefactory_mediatek.patch` agrega la rama `mediatek`.
En cada arranque sale `Unrecognized solicited response: 2016`; RIL.java
lo captura y no pasa nada.

Para probar a mano: `/dev/socket/rild-atci` acepta comandos AT, pero al
cerrar el cliente `librilmtk` se queda leyendo 0 bytes en bucle (CPU y
log). Hay que reiniciar después.

Falta: APN de MMS y comprobar 3G.

Lo que sigue es la historia con el `#57`.

El kernel `#57` no sube el módem. `CONFIG_MTK_CCCI_DRIVER` no está
puesto, así que este árbol no genera `ccci.ko` ni `ccci_plat.ko`. Los
`.ko` de stock en `/system/lib/modules` piden `get_md_gpio_val` y
`get_md_adc_val`. El `#57` exporta `get_md_gpio_info` y
`get_md_adc_info`. El `insmod` sale con `Unknown symbol` y no crea
`/dev/ttyC0` ni `/dev/ccci*`.

Se probó el zImage del `boot.img` de stock (`Linux 3.4.5 #1`, 12 ene
2014) con el ramdisk de CM7. `mtkbootimg --mtk 1`, base `0x10000000`,
page 2048, cabeceras `KERNEL` y `ROOTFS`. mtd7 es NAND de 6 MB,
writesize 4096, erase 256 KB. `flashcp` rechaza el último trozo si no
cierra en 4096 bytes; la imagen se rellenó con `0xff` hasta el erase
block. Recovery (mtd8) y `mtd2` no se escribieron.

Ese kernel sí exporta `get_md_gpio_val`. `ccci_plat.ko` y `ccci.ko`
cargan y aparecen `/dev/ttyC0` y `/dev/ccci_ioctl1`. El vermagic de los
dos kernels es `3.4.5 SMP preempt mod_unload ARMv7`, así que el `#57`
no rechaza un `.ko` de stock por versión: si el símbolo existe, entra.
Antes del reboot, `07wmt` se renombró a `07wmt.k57` para que no hiciera
`insmod` de los módulos del `#57`. `mtkbt` tampoco arrancó solo.

`mtk-ril.so` no usa `/dev/ttyC0` como puerto AT. Abre
`/dev/radio/pttycmd*`. En el `init.modem.rc` de stock eso lo crea
`gsm0710muxd -s /dev/ttyC0 -f 512 -n 8 -m basic`, después de
`ccci_fsd` y `ccci_mdinit`. El mux arrancó y se quedó en `ttyC0`;
`/dev/radio` siguió vacío. `ccci_mdinit` espera `nvram restore ready`.
No se arrancó, ni `nvram_daemon`: no está claro si escriben `mtd2`.

`system_server` murió con `SIGSEGV` en `libsurfaceflinger.so` y zygote
lo relanzaba. Se volvió a escribir mtd7 con la copia tomada antes del
cambio (6 MB, kernel `0x38e128`, `boot-k57-20261011.img` junto al dump
de stock). El teléfono arranca otra vez en `#57`. `07wmt` y el
`start mtkbt` de `init.local.rc` quedaron como estaban.

`system.prop` de este árbol tiene `rild.libpath=/system/lib/mtk-ril.so`
y `rild.libargs=-d /dev/ttyC0`. En el `build.prop` del teléfono se
quitaron: sin `/dev/ttyC0`, `rild` no deja `/dev/socket/rild`. Un
system nuevo los metería y el `#57` se quedaría sin socket de radio.

## Audio

`libaudio` es propio (`AudioHardware.cpp`): escribe registros AFE y del
PMIC MT6323 por ioctl en `/dev/eac`. Stock no tiene `libaudio.so`; su HAL
está dentro de su `libaudioflinger.so`, que no encaja con el de CM7.

El parlante es el Class-D del MT6323. `amp_yad` no toca GPIO en MT6572.

Para cada ruta se copian los valores de stock: con algo sonando en un
Y220 stock, `cat /proc/audio` (sin root) y comparar con CM7. Con el
parlante, el DMA (`AFE_DL1_CUR`) no avanzaba hasta igualar:
`AFE_DAC_CON1=9` (DL1 a 44100), `AFE_DAC_CON0` bit 10, `I2S_CON3`
apagado, `ABB_AFE_CON0=1`, `TOP_CKPDN1` bit 15 limpio,
`AUDTOP_CON0/1/5`, `SPK_CON1=0x8055`, `SPK_CON9=0x600`.

Micrófono: `EacStreamIn` abre su propio `/dev/eac` (el memif VUL queda
atado al fd que lo reservó), pone los registros de stock grabando
(`CONN3=9`, `UL_SRC_CON0=0x1e0001`, IRQ2 cada 800, `ABB_AFE_CON0` bit 1,
`ABB_AFE_CON1=0x19`, `AUDTOP_CON0..8`) y captura a 48000 estéreo. `read()`
promedia a la tasa pedida (8000, 16000, 24000, 48000); para otras propone
una y AudioFlinger remuestrea. El `read()` del driver no bloquea.
La tabla de tasas del AFE es 8k=0, 16k=4, 32k=8, 44.1k=9, 48k=10; el enum
de `AudDrv_Afe.h` (44100=7) no corresponde al hardware.

Auriculares, selección de salida, grabar mientras suena algo y la ruta FM:
ver [`docs/AUDIO_NOTES.md`](docs/AUDIO_NOTES.md). Llamadas de voz (CCCI con
el módem, rutas, volumen y los botones Silenciar/Teclado):
[`docs/CALL_NOTES.md`](docs/CALL_NOTES.md). Falta: volumen de música por
hardware.

Cámara (vista previa tras la foto, resolución, cómo capturar log y
pantalla): [`docs/CAMERA_NOTES.md`](docs/CAMERA_NOTES.md).

## Sensores

Solo hay acelerómetro (KXTJ2-1009, `/dev/gsensor`). El kernel no tiene
`alsps`: no hay sensor de proximidad ni de luz.

- HAL: `sensors.default.so` de stock. Lee eventos `EV_REL` del input
  `hwmdata` (solo avisan que hay datos) y pide los valores con
  `HWM_IO_GET_SENSORS_DATA` en `/dev/hwmsensor`. El tamaño del ioctl
  (0x218) coincide con el kernel.
- `ueventd.mt6572.rc` deja `/dev/hwmsensor` y `/dev/gsensor` en
  `system:system 0660`, como el init.rc de stock.
- `dumpsys sensorservice` muestra `last=<0,0,0>` aunque funcione: el HAL a
  veces devuelve 0 eventos en `poll()` y el `recordLastValue` de GB lee
  `buffer[-1]` en ese caso. Los eventos sí llegan a las apps.

## Cámara

Preview y foto funcionan con la pila de MTK de stock: `camera.default.so`,
las `libcam.*`, `libcamera_client.so` y `libcameraservice.so` de stock.
`USE_CAMERA_STUB` sigue en true; el stub de CM7 queda tapado al instalar.

- mediaserver: el `CameraService` de stock mide 0x68 bytes y el header de
  CM7 dice 0x48. Con `new CameraService()` de CM7 se pisa el heap y
  mediaserver muere (SIGBUS en `writeStrongBinder`). El entry point propio
  vive en `device/huawei/y220/mediaserver`.
- Símbolos de ICS que faltaban: ver los parches de `libutils`, `libjpeg`
  y `libcutils`.
- Permisos (`ueventd.mt6572.rc`): `/dev/M4U_device` 0444, nodos
  `kd_camera_*`/`camera-*` en `system:camera`, y `MTK_SMI`/`mt-mdp` en
  `system:camera`. El mediaserver de stock corre como root; el de CM7
  como `media` con grupo `camera` y sin grupo `media`. Sin M4U la HAL no
  reserva buffers (`m4u_alloc_mva fail`) y el ISP queda en
  `deque no buff`. `init.rc` pone `mem_bw_ctrl/concurrency_scenario` en
  `system:camera`.
- Preview: el `CameraClient` de stock usa la API de GB
  (`registerPreviewBuffers`/`postBuffer`) sobre la `SurfaceView` de push
  buffers. El buffer viene en YV12; `GGLSurface.format` es de 1 byte y
  YV12 llegaba como 89, que `TextureManager` no conoce: preview blanca.
  El hook de `LayerBuffer` llama al conversor RGB565 del device tree. Con
  `SURFACE_TYPE_NORMAL` falla `registerBuffers` (-38), así que la app de
  Cámara no se toca.

El 2026-10-13 se instaló puntualmente el `libsurfaceflinger.so` que compila
ese conversor. Después del reinicio, la cámara negoció YV12 480x368, mostró
preview real en el framebuffer, tomó `IMG_20261013_131308.jpg` y sobrevivió
tres ciclos de cerrar/abrir. `mediaserver` conservó el PID 212 y no hubo
señales fatales ni tombstones nuevos. La biblioteca anterior quedó en
`/data/local/tmp/libsurfaceflinger.so.before-yv12-device`.

## Video

Graba (cámara de video de CM7, 3gp). El HAL de MTK manda los cuadros como
`yuv420i-yyuvyy-3plane`, que es I420; `CameraSource` de CM7 no conocía el
nombre y un `CHECK` abortaba mediaserver (se caían audio, cámara y
reproductor). El hook de `frameworks_base_stagefright_mtk_i420.patch`
delega el nombre a `stagefright/CameraCompat.cpp`, que lo mapea a
`OMX_COLOR_FormatYUV420Planar`; luego codifica el encoder por software.

El 2026-10-13 se instaló puntualmente el `libstagefright.so` con este hook.
Grabó `VID_20261013_132009.3gp`: 23.752 s, 482251 bytes, vídeo H.263
352x288 y audio AMR-NB. `mediaserver` mantuvo el PID 212, no apareció el
`CHECK` de formato y no hubo tombstones nuevos. La biblioteca anterior
quedó en `/data/local/tmp/libstagefright.so.before-camera-compat`.

La reproducción usa los decoders de hardware de MTK
(`OMX.MTK.VIDEO.DECODER.AVC`, `MPEG4` y `H263`):

- `libmtk_shim` (precargado en mediaserver por `init.rc`) da lo que los
  blobs toman del bionic y del libbinder/libstagefright de MTK:
  `sched_getaffinity`, `sched_setaffinity`, `__pthread_gettid`,
  `MemoryHeapBaseMTK` y `AwesomePlayer::mtk_omx_get_current_time`.
- `init.d/10vcodec` carga `vcodec_kernel_driver.ko` y crea `/dev/Vcodec`
  como `media:system` (mediaserver corre con uid media y gid system).
- `frameworks_base_stagefright_mtk_omx.patch` (`BOARD_USES_MTK_OMX`) pone
  los decoders MTK primero en OMXCodec y usa `nStride`/`nSliceHeight` como
  tamaño decodificado. El decoder rellena el stride a 32 píxeles (176 de
  ancho sale con 192) y el `LayerBuffer` del Y220 ignora `hor_stride`, así
  que `SoftwareRenderer` recorta los planos al tamaño visible.

Medido el 2026-10-21 con `stagefright` en un H.264 640x480 de 1,5 Mbps:
14,9 ms por cuadro contra 26,8 ms del decoder de software, y la mitad de
CPU del sistema (362 contra 680 ticks). Se verificó la imagen en pantalla
con 176x144, 192x144, 320x180, 320x240 y H.263 352x288. Las miniaturas
siguen usando software primero. La grabación sigue con el encoder de
software (los encoders MTK no están en la tabla). Detalle:
[`docs/VIDEO_NOTES.md`](docs/VIDEO_NOTES.md).

## Revisión de blobs

`tools/blobcheck/blobcheck.sh` revisa el `/system` del teléfono conectado:
`symcheck.py` busca bibliotecas NEEDED que faltan y símbolos sin definir, y
`dltest` hace `dlopen` en el teléfono de cada `.so` de `/system/lib`, que es
lo único que muestra lo que rechaza el linker de GB. Con
`--stock ~/y220/system.ubifs.win` lista además los archivos de stock que
CM7 no tiene. `known.txt` guarda los fallos conocidos con su causa; lo
demás sale como NUEVO y el script termina con 1. `dltest` se compila con
`mmm device/huawei/y220/tools/blobcheck`.

La comparación del 2026-10-21 contra stock encontró que faltaban la
política térmica (`09thermal`) y `dalvik.vm.heapsize` (Dalvik usaba 16m).
El `gps.default.so` de stock es un stub vacío: el Y220-U05 no tiene GPS. De
sensores solo hay acelerómetro, como en stock. Detalle:
[`docs/SYSTEM_NOTES.md`](docs/SYSTEM_NOTES.md).

## Carga con el teléfono apagado

Con el teléfono apagado y el cargador conectado, el LK arranca la partición
boot con `boot_mode` 8 (`/sys/class/BOOT/BOOT/boot/boot_mode`, ver el README
del kernel). El `init` de stock carga entonces `init.charging.rc` y corre
`ipod`. El de CM7 no sabía de esto: arrancaba el `init.rc` normal y se quedaba
en el logo de Movistar. No es cosa de TWRP: el recovery no interviene.

`system_core_init_charging.patch` (`BOARD_CHARGING_MODE_FILE`): si el modo es
8 o 9, `init` dispara `charger` en vez de `early-boot`/`boot` y deja
`ro.bootmode=charger`. `init.rc` arranca ahí el `ipod` de stock, que dibuja
la batería desde la partición logo; módem, `init.d` y Android no arrancan.
El servicio `shutdown` (`reboot -p`) es el que `ipod` pide al desconectar el
cargador; stock usa un `/system/bin/shutdown` que CM7 no trae.

Para entrar: apagar, desconectar, esperar la pantalla negra y conectar. Si
se apaga con el cargador ya conectado, el LK reinicia en modo normal
(`boot_reason=4`) y arranca Android.

Probado el 2026-10-22 con el kernel `#95`: aparece la animación, el modo
carga se queda estable (se suspende con la pantalla apagada) y mantener el
botón de encendido hace que `ipod` reinicie a Android. Hasta `#94` el kernel
se colgaba a los ~10 s por un `panic()` de diagnóstico que había quedado en
`battery_common.c` (ver el README del kernel), y había que sacar la batería.
Desconectar el cargador (servicio `shutdown`) no se probó.

Para depurar: `touch /data/misc/ipod_save_log` hace que `ipod` escriba
`/data/misc/ipod_log`; en modo carga no hay adb y UBIFS pierde lo que no se
sincronizó al sacar la batería, así que hay que volcar con `sync`.

## Anclaje USB

Desde el kernel `#93`. El kernel del árbol se compilaba sin netfilter
(`# CONFIG_NETFILTER is not set` en el kconfig del proyecto), así que no había
tabla `nat`. El kernel de stock sí la tiene (`ip_tables`, `iptable_nat`,
`nf_conntrack` y `MASQUERADE`). `#93` usa el netfilter de la config de
plataforma MT6572. `XT_MATCH_QTAGUID` queda apagado: es de ICS, no compila sin
IPv6 y GB no lo usa. Como cambia `sk_buff`, todos los módulos se recompilaron
con `#93`.

El netd de GB (también el de stock) activa RNDIS en
`/sys/class/usb_composite/rndis/enable`, pero el kernel 3.4 usa el gadget
`android_usb`:

- `netd/UsbController.cpp` (vía `BOARD_CUSTOM_USB_CONTROLLER`, sin parche):
  si existe `android0/functions`, netd pone `rndis,adb` y al terminar
  restaura la lista anterior (`mass_storage,adb`). adb sigue conectado.
- `packages_apps_Settings_rndis_on_demand.patch`: `rndis0` solo existe
  mientras la función está activa, y Settings solo dejaba anclar una
  interfaz que ya existiera. Con el cable conectado, la casilla primero
  enciende RNDIS y ancla `rndis0` cuando aparece. Si no aparece en 15 s,
  lo apaga.
- Overlay: `config_tether_usb_regexs` = `rndis\d`;
  `config_tether_upstream_regexs` = `wlan\d` y `ccmni\d`.

Probado el 2026-10-22: el PC (Linux, `rndis_host`) recibe 192.168.42.x por
dnsmasq, navega por `ccmni0` con NAT y resuelve DNS. Al desmarcar vuelve
`mass_storage,adb`. El ping a internet falla también desde el teléfono: Claro
filtra ICMP.

## Zona Wi-Fi

Probado el 2026-10-22: WPA2, el cliente recibe 192.168.43.x por dnsmasq y
navega por `ccmni0` con NAT, también en el primer encendido tras arrancar.

El driver `wlan_mt6582` crea su interfaz AP con el ioctl privado
`set_p2p_mode 1 1` sobre `wlan0` (sub-ioctl 28 de `SIOCIWFIRSTPRIV`): aparece
`ap0`, solo cfg80211, sin handlers wext. El `p2p_supplicant` wext de stock
iba con el `p2p.ko` del kernel de 2014 y no sirve; el hostapd 0.6 de GB
tampoco (necesita interfaz monitor y libnl).

- hostapd 0.8 (nl80211) de CM ics: dos repos extra fuera del manifiesto,
  `external/wpa_supplicant_8`
  (github.com/CyanogenMod/android_external_wpa_supplicant_8, ics, 84507b8) y
  `external/libnl-headers`
  (github.com/CyanogenMod/android_external_libnl-headers, ics, 6ccf734).
  `hostapd/Android.mk` lo incluye con `BOARD_HOSTAPD_DRIVER := NL80211`
  (el `Android.mk` raíz de wpa_supplicant_8 solo entra con VER_0_8_X).
  `libnl_2/` es el `system/core/libnl_2` de ICS. `include/linux/` trae
  `genetlink.h` e `in_route.h`, que bionic GB no tiene
  (`TARGET_SPECIFIC_HEADER_PATH`). `hostapd/hostapd_mtk.c` da el hook
  `wpa_driver_nl80211_driver_cmd` vacío y `get_rfc3526_prime_1536` (WPS),
  que falta en el OpenSSL 0.9.8 de GB.
- `ap0` no permite interfaz monitor (`mon.ap0: -95`). hostapd recibe los
  frames de gestión por nl80211, pero mandaba el EAPOL por el monitor y el
  4-way de WPA2 nunca terminaba (`i802_send_eapol ... Bad file number`).
  `external_wpa_supplicant_8_nl80211_eapol.patch` lo envía por PF_PACKET
  en `ap0`.
- hostapd ICS baja a uid `wifi` al iniciar (`os_program_init`): netd deja
  `/data/misc/wifi/hostapd.conf` y el directorio de control como `wifi`.
- `netd/SoftapController.cpp` (`BOARD_CUSTOM_SOFTAP_CONTROLLER`,
  `system_netd_custom_softap.patch`): `set` escribe la config (SSID,
  open/WPA/WPA2, canal 6), `startap` enciende `wlan0`, crea `ap0` y arranca
  el servicio `hostapd` de `init.local.rc`; `stopap` lo para y quita `ap0`.
  La interfaz que pasa el framework (`wl0.1`) se ignora.
- Overlay: `config_tether_wifi_regexs` = `ap\d`.
- `system_netd_logwrapper_waitpid.patch`: al encender BT, `PanController`
  lanza `pand` (BlueZ), que sale con 255 y queda zombi. El `wait()` del
  logwrapper de netd lo recogía en el primer iptables y netd abortaba el NAT
  sin `MASQUERADE` (el cliente conectaba sin internet). Ahora espera a su
  propio hijo.

## Pendientes conocidos

- Bluetooth: enciende, busca, empareja en los dos sentidos y envía
  recibe archivos y suena por audífonos A2DP (ver `docs/BLUETOOTH_NOTES.md`);
  falta manos libres. La pila es `mtkbt` (MTK, sobre STP), no
  BlueZ: no hay `bluetoothd` y el kernel no tiene sockets RFCOMM/SCO de
  BlueZ.
  - Para que `mtkbt` encienda el chip: `persist.mtk.wcn.combo.chipid=0x6572`,
    grupos `system` y nvram (9997) en el servicio, y `servicemanager` que
    deja registrar al uid bluetooth (`mtk.bt.profile.*.ashm`).
  - Mensajes (ids de este `mtkbt`): pedidos por `bt.int.adp`, respuestas a
    pedidos en `bt.app.gap` (POWERON_CNF 145), indicaciones en `bt.ext.adp`:
    DISCOVERY_RESULT_IND 147, UPDATE_IND 148 (nombre), DISCOVERY_CNF 149.
    La numeración no coincide con los headers MTK de JB en GitHub.
  - `libbt_mtk_gap` arma los pedidos GAP y lee `bt.ext.adp`;
    `frameworks_base_bt_mtk.patch` conecta sus eventos con
    `BluetoothEventLoop`, así Ajustes recibe discovery y bonding.
- Con BT encendido, `BluetoothHandsfree` reintentaba el accept de SCO sin
  pausa: `com.android.phone` al 43 % y ~640 líneas de log por segundo.
  `packages_apps_Phone_sco_retry.patch` lo reintenta a los 30 s.
- Al encender BT, `BluetoothNetworkService` pide `startPan` a `netd`, que
  falla sin red Bluez; la `IllegalStateException` reiniciaba system_server.
  `frameworks_base_bt_pan_no_crash.patch` la captura. MAP (`com.android.bluetooth`)
  moría con `NullPointerException` en `accept()` sin socket RFCOMM;
  `packages_apps_Bluetooth_map_no_socket.patch`.
- GPS: no hay HAL (`gps.default.so` de stock es un stub de 5 KB).
- APN de MMS de Claro.
