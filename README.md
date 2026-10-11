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

El combo Wi-Fi/BT no pasó por un cambio de kernel. `07wmt` carga
`mtk_stp_wmt`, `mtk_stp_bt`, `mtk_wmt_wifi`, `cfg80211`, `wlan_mt6582` y
deja `6620_launcher -m 3` vivo. `mtkbt` arranca en
`sys.boot_completed=1`. El toggle de Bluetooth en Ajustes sigue siendo el
camino BlueZ de CM7, no el de `mtkbt`.
