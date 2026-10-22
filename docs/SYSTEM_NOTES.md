# Sistema: comparación con stock (2026-10-21)

En vez de buscar bugs uno por uno se comparó el `/system` del backup TWRP
de stock (C482B206), el ramdisk del boot de stock y el CM7 instalado:
archivos, servicios de init, bibliotecas y propiedades.

## blobcheck

`tools/blobcheck/blobcheck.sh` repite la comparación con el teléfono
conectado:

```
bash device/huawei/y220/tools/blobcheck/blobcheck.sh
bash device/huawei/y220/tools/blobcheck/blobcheck.sh --stock ~/y220/system.ubifs.win
```

- `symcheck.py`: NEEDED que faltan y símbolos sin definir, con
  `libmtk_shim` contado como precargado (como en mediaserver).
- `dltest`: `dlopen` en el teléfono de cada `.so`, cada uno en su
  proceso. Es lo único que muestra lo que rechaza el linker de GB.
  Se compila con `mmm device/huawei/y220/tools/blobcheck`.
- `known.txt`: fallos conocidos por causa raíz. Lo demás sale como NUEVO
  y el script termina con 1.

`readelf` en español escribe "Biblioteca compartida"; los scripts fuerzan
`LC_ALL=C`.

## test_y220 antes y después de flashear

```
bash device/huawei/y220/tools/test_y220.sh --section zip --zip out/target/product/y220/cm-7-*.zip
bash device/huawei/y220/tools/test_y220.sh --fast
```

`--zip` prueba que cada apk/jar del zip sea un zip válido (no hace falta
el teléfono). En el teléfono, la sección `bugs` falla si un apk de
`/system/app` no quedó registrado en PackageManager o si no hay teclado
predeterminado instalado. El 2026-10-21 una build cortada por OOM dejó
`LatinIME.apk` truncado con fecha nueva; la build siguiente lo empaquetó y
tras el wipe no hubo teclado. `RomManager.apk` (stub de CM7) se ignora.

## Arreglado

- Política térmica: sin `thermal_manager` las zonas de `/proc/mtktz` no
  tenían puntos de disparo. `init.d/09thermal` carga
  `/etc/.tp/thermal.conf` de stock (CPU 67/69/85 °C, apagado 105/115 °C,
  batería 60 °C), arranca `thermald` (apagado ordenado que pide el
  kernel) y corre `pq`. El daemon `thermal` de stock (WiFi) necesita
  `ifc_set_throttle` del libnetutils de MTK y no se usa.
- `dalvik.vm.heapsize=24m`: no estaba y Dalvik daba 16m a cada app.
- Decoders de video MTK: ver `VIDEO_NOTES.md`.

## No son bugs

- GPS: el `gps.default.so` de stock es un stub vacío de 5 KB; el
  Y220-U05 no tiene GPS.
- Sensores: solo acelerómetro, igual que stock. No hay proximidad, luz
  ni brújula; `msensord` no hace falta.
- NVRAM: dirección BT y MAC WiFi correctas.
- Servicios de stock sin uso en CM7: DMAgent, NvRAMAgent, hald,
  mdlogger, mobile_log_d, agpsd, drmserver.
- Firmware FM v2–v5: falta, la radio funciona con v1.
- 3G (2026-10-22): con "Solo WCDMA" el RIL manda `AT+ERAT=1,0`, el módem
  busca 3G y la red responde `+CREG: 2,3` (registro denegado). El teléfono
  queda en EDGE con Claro PE en modo automático: es cosa de la red, el plan o
  las bandas, no de la ROM.
- Almacenamiento USB (2026-10-22): compartir la SD funciona (lunfile
  `/sys/devices/platform/usb_mass_storage/lun0/file`, el PC la ve como
  "File-CD Gadget"). Si el PC la tiene montada, desactivar falla con
  `Device or resource busy` y la SD queda sin montar en el teléfono hasta
  expulsarla en el PC; es el comportamiento de GB.

## Siguen sin cargar (sin uso)

`libextpbap_jni` (PBAP), `libmtkplayer`, `libMtkOmxAacEnc`
(`libvo_aacenc`), `libvtmal` (videollamada), DRM de MTK, `factory` y
`hqftm` (modo fábrica), `libagpssupl` (A-GPS). El detalle está en
`tools/blobcheck/known.txt`.
