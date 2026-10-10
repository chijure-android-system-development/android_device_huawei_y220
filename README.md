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
leyendo ese dump incompleto. El vendor del árbol no trae estos HAL.

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

Entran GPU (`libMali.so`, `libUMP.so`), RIL (`mtk-ril.so`, `librilmtk.so`,
`rild`, `ccci_fsd`, `ccci_mdinit`), cámara, OMX, audio MTK, sensores, NVRam,
`etc/firmware` (incluido `modem.img`) y los `.ko` de stock. No entran las
apps de Google ni el odex de Settings/Phone.

Esos HAL y los EGL de Mali sí están en `system.ubifs`. Este vendor no
los copió. Los jar
`com.mediatek.framework.jar` y `mediatek-common.jar` son stubs de 4 KB; el
código está en el `.odex`, atado al framework de stock, y no carga en CM7.

`lib/modules/wlan_mt6582.ko` de este vendor pesa 359972 bytes. El que está
en el teléfono es el recompilado (708120 bytes) que asocia a WPA2. No
reemplazar el del teléfono con este.

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

## Fuera de este paso

Gralloc, lights y los EGL de Mali están en `system.ubifs`, no en el
vendor que arma el zip. El módulo de Wi-Fi del zip es el de stock, no el
que ya asocia a WPA2.
