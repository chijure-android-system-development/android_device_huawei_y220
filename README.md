# Huawei Y220-U05, CM7

MediaTek MT6572, panel ILI9488 320x480, NAND UBIFS. No es el Y210.

El teléfono sigue con el kernel `#57` (`boot-y220-ums4.img`) y el recovery
TWRP. Nada de este directorio se ha flasheado.

## Qué es este árbol

`lunch cyanogen_y220-eng` selecciona el producto. `breakfast y220` elige
`eng` aunque `timed_build.sh` pida `userdebug`. El `kernel` prebuilt es el
zImage de `boot-y220-ums4.img` (`#57`, 3727144 bytes).
`prebuilt/ramdisk-stock.img` es el ramdisk de boot de esa misma imagen
(612053 bytes, gzip cpio, sin cabecera MTK). Sigue trayendo `init.charging.rc`.

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
- `libhardware_legacy_wifi_wmt.patch`: si existe `/dev/wmtWifi`, el HAL
  espera hasta 4 s a que aparezca `wlan0` después de marcar el driver
  como cargado. El `insmod` de `wlan_mt6582.ko` no crea la interfaz.
- `external_wpa_supplicant_6_scan_idle.patch`: solo los puntos de
  llamada. La política está en `libwpa_mtk_wext`.
- `frameworks_base_wifi_dormant.patch`: un estado DORMANT sin asociación
  no baja `wlan0`.
- `frameworks_base_bt_mtk_gap.patch`: el enable de Ajustes llama a
  `libbt_mtk_gap` en vez de `bluetoothd`. Ver la nota de Bluetooth. No
  prenderlo.

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

## Bluetooth

No prenderlo desde Ajustes. El toggle deja `system_server` en un bucle.

La dirección de fábrica ya está en `mtd2` (registro `BT_Addr`, 30 bytes,
no es la de fábrica `00:00:46:65:72:01`). Se copió, sin escribir la
partición, a los dos archivos que el stack lee:

- `/data/BT_Addr`, `0777`, `root:root`. Lo abre `BT_open` en
  `stp_chrdev_bt.c`.
- `/data/nvram/APCFG/APRDEB/BT_Addr`, `0666`, `root:bluetooth`. Lo lee
  `libbluetooth_mtk`.

`nvram_daemon` no está corriendo. No arrancarlo: si el archivo no está,
puede volver a grabar el valor por defecto en la partición.

`libbt_mtk_gap` manda `MSG_ID_BT_POWERON_REQ` (100) al socket
`/dev/socket/bt.int.adp` y espera `MSG_ID_BT_POWERON_CNF` (145). Ajustes,
si la respuesta es 0 o 1, salta el event loop de BlueZ y marca el estado
encendido. En el teléfono la prueba fue esta:

- `mtkbt`: `Read Nvram data fails` y `mtk_bt_enable: ret=-1`.
- Igual contestó `POWERON_CNF` con `result=0`. El cliente lo tomó como
  éxito (`BTBM_ADP_SUCCESS` también es 0).
- `initProfileState` pidió las propiedades del adaptador. No hay path de
  BlueZ. `dbus_message_new_method_call` aborta el proceso (`SIGSEGV`,
  `deadbaad`) y Zygote vuelve a levantar la interfaz. En el arranque,
  `SystemServer` lee `bluetooth_on` y llama a `enable()` otra vez.
- dmesg no tiene panic. El cierre es `BT_close`, `WMT turn off BT OK` y
  el PMIC de BT abajo.

`bluetooth_on` se dejó en 0. El binario que está en el teléfono es el que
hace ese abort. El parche ya trae la guarda del path nulo, pero esa
revisión no se instaló: el rebuild no llegó a linkar. Aunque se instale,
`result=0` no se puede aceptar mientras `mtk_bt_enable` devuelva -1.

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
