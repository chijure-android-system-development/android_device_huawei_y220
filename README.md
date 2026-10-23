# CyanogenMod 10.1 para Huawei Ascend Y220-U05

Device tree de CyanogenMod 10.1 / Android 4.2.2 para el Huawei Ascend
Y220-U05 (MediaTek MT6572). No es el Y210. Viene del port CM7 (rama
`gingerbread`); la plataforma es CyanogenMod `cm-10.1` sin parches: lo propio
del Y220 vive aquí. `docs/` son las notas del port CM7 (hardware, llamadas,
cámara, BT) y siguen valiendo como mapa del hardware, no del software.

El Y220 salió con Android 2.3.6, pero el MT6572 es un SoC de la época 4.2:
el kernel (`3.4.5`, fuente en `~/y220/kernel-mt6572`, rama de MTK
`lcsh72_we_jb3`) es el de 4.2 de MediaTek. Por eso el userspace MTK de 4.2
de otro MT6572 de Huawei, el **Y330-U01**, encaja con el kernel del Y220 sin
tocarlo.

## Hardware

| Campo | Valor |
| --- | --- |
| SoC | MediaTek MT6572, 2x Cortex-A7 |
| GPU | Mali-400 MP, driver del kernel r3p2 (API 19) |
| RAM | 256 MB (219 MB para Linux) |
| Pantalla | HVGA 320x480, mdpi |
| Almacenamiento | NAND 256 KB de bloque; `system`, `userdata`, `cache` en UBIFS |
| Módem | MT6572 integrado, MOLY WR8 W1315 (ccci) |
| WiFi/BT/FM | combo MT6572 (WMT), `wlan_mt6582.ko` |
| Kernel | `#95`, prebuilt en `kernel`, módulos en `prebuilt/modules` |

## De dónde sale cada blob

`proprietary-files.txt` lista cada archivo con su origen y
`extract-files.py` los copia a `vendor/huawei/y220`:

- `y330:` stock Y330-U01 V100R001C00B121 (4.2.2, ALPS.JB3.MP.V1.37),
  extraído en `/mnt/fuentes/references/y330-stock-B121`. Gráfica (Mali
  r3p2-01rel0, `gralloc.mt6572`), RIL (`rild`, `librilmtk`, `mtk-ril`),
  NVRAM del lado AP, BT (`libbluetoothdrv`), audio, sensores y cámara.
- `y220:` stock GB del propio Y220 (vendor de CM7). Módem (`modem.img`,
  `ccci_fsd`, `ccci_mdinit`), firmware del combo, `6620_launcher`, tablas
  térmicas y `pq`.

Mismo SoC, mismo Mali (`r3p2-01rel0` en los dos stocks) y módulos con el
mismo vermagic que el kernel `#95`.

**NVRAM:** las tablas de LID de `libcustom_nvram` no coinciden. El Y330
tiene dos LID comunes más (`Audio_AudEnh_Control_Opt`, `Dfo`), así que todos
los LID de audio quedan corridos en 2. Los blobs de audio y BT del Y330
llevan sus LID compilados, por eso el lado AP de la NVRAM (`nvram_daemon`,
`libnvram`, `libcustom_nvram`) es el del Y330. El lado módem, donde está el
IMEI, sigue con los binarios del Y220. `BT_Addr` es el LID 1 en los dos.

## Sin parches de plataforma

Lo que CM7 resolvía parcheando `system/core`, `frameworks` y `build` aquí
está en el device tree:

- **UBI:** `ubiattach/ubiattach_y220` (estático, en `/sbin`) conecta
  `system`, `userdata` y `cache` como `ubi0`, `ubi1` y `ubi2` (la numeración
  del fstab de TWRP). `init.mt6572.rc` lo corre con `exec` en `on fs` y monta
  `/dev/ubiN_0`.
- **`init.rc` propio** (`TARGET_PROVIDES_INIT_RC`): el genérico de CM monta
  `mtd@system`, `mtd@cache` y `mtd@userdata` como yaffs2 en `on fs`. En esta
  NAND esos nombres existen; YAFFS2 toma los bloques UBI por basura y los
  borra. Así se perdieron `system` y `cache` en el primer arranque
  (2026-10-23). También define `ril-daemon` con los sockets del `rild` MTK
  (`rild`..`rild4`, `rild-mtk-*`, `rild-atci`).
- **OTA para UBIFS:** `releasetools/y220_ota_from_target_files`
  (`TARGET_RELEASETOOL_OTA_FROM_TARGET_SCRIPT`) monta los volúmenes UBI en
  TWRP, vacía `/system` sin formatear y escribe el boot con `flash_image`.
  El boot lleva las cabeceras MTK (`bootimg.mk`, `mtkbootimg/`).
- **Símbolos MTK:** los blobs esperan funciones que MTK agregó a `liblog`,
  `libcutils`, `libmedia`, `libui` y `libcamera_client`. Están en
  `libmtk_symbols` (xlog, pmem, `MtkCameraParameters` con los valores del
  stock, `AudioSystem` voice unlock, `GraphicBufferMapper::getIonFd` vía el
  device `extra` de gralloc) y `init.mt6572.rc` la pone en `LD_PRELOAD`. El
  linker de 4.2 busca ahí los símbolos de todas las librerías.
- **Nodos MTK:** el init de CM no tiene `mknod` y estos drivers no avisan a
  ueventd. `rootdir/init.mtk-nodes.sh` crea `sec`, `devmap`, `Vcodec`,
  `stpwmt`, `stpbt`, `stpgps`, `wmtWifi` y `fm`, y pone los dueños ccci
  (9996) y nvram (9997) con el `chown` de busybox (el de toolbox solo acepta
  nombres).

## Subsistemas

- **RIL:** `MediaTekRIL` (`ril/`, en telephony-common por
  `BOARD_RIL_CLASS`). Numeración de URC del `librilmtk` del Y330 (tabla
  3000..3029; el GB usaba 3020 para el estado PS, este 3022). Acepta las
  llamadas entrantes con `SET_CALL_INDICATION` (sin eso no suenan), pide
  GPRS "always" al encender la radio y manda el cid en `SETUP_DATA_CALL`.
  La señal viene en el formato de MTK (ASU, BER y después RSSI/RSCP/Ec/N0 en
  cuartos de dBm); AOSP tomaba el RSSI por el dBm de CDMA y mostraba sin
  señal, así que `responseSignalStrength` la traduce a GSM.
- **APN:** `apns-conf.xml` en formato `version="7"` (el de CM7 era 6 y
  TelephonyProvider descartaba el archivo entero: cero APN) con Claro PE y
  Movistar PE. Si la base quedó vacía, borrar
  `/data/data/com.android.providers.telephony/databases/telephony.db`.
- **WiFi:** `wpa_supplicant_8` sobre nl80211 con
  `wpa_supplicant_8_lib` (START/STOP/MACADDR). El HAL carga
  `wlan_mt6582.ko` y `wmt_wifi_loader` (`wmt-wifi-on.sh`) enciende WMT WiFi.
  El `wlan_mt6582.ko` de `prebuilt/modules` lleva un arreglo del kernel:
  `mtk_cfg80211_set_default_key()` devolvía `-EINVAL` ("not implemented") y
  `wpa_supplicant_8` lo llama después de cada clave por pares, así que todo
  WPA/WPA2 fallaba con "Failed to set PTK" aunque el handshake se completara.
  Ahora devuelve 0 (`add_key` ya marca la clave de transmisión). Solo cambia
  el módulo: se arma con `KMOD_PATH=mediatek/kernel/drivers/conn_soc/drv_wlan/mt6582/wlan ./build.sh y220`
  y el boot sigue con el kernel `#95`.
- **Bluetooth:** Bluedroid con `libbt-vendor` sobre `libbluetoothdrv.so`.
  `com.android.bluetooth` no tiene el gid de nvram (9997; bionic no resuelve
  gids numéricos en `platform.xml`), así que no lee `BT_Addr` y Bluedroid
  usa su propia dirección (`persist.service.bdroid.bdaddr`).
- **Audio:** HAL primario del stock (`libaudio.primary.default.so`, con su
  nombre original) cargado por `libaudio_wrapper` (`audio.primary.mt6572`):
  el módulo MTK se registra con id `libaudio` y el libhardware de CM pide
  `audio`. La política es `audio_policy.default` de CM: la del stock usa una
  `audio_policy_service_ops` con un campo más antes de `load_hw_module`
  (offset 64 contra 60) y no abría ningún módulo. `QCOM_FM_ENABLED`
  alinearía la tabla pero cambia los enums de `system/audio.h`.
  `mediaserver` lleva el grupo 9997 para leer la calibración de `/data/nvram`;
  `/dev/ebc` (CVSD, audio SCO) es `system:media`.
  Grabación: el `AudioMTKStreamIn` del stock solo abre el micrófono en
  estéreo a 48 kHz (`Set`/`CheckSampleRate`); la conversión la hacía el
  AudioFlinger de MTK. El de CM solo reintenta una entrada rechazada si la
  frecuencia sugerida es como mucho el doble de la pedida, así que 8 y
  16 kHz fallaban. `libaudio_wrapper` abre la entrada con la configuración
  que el HAL acepta y en `read()` convierte canales y frecuencia (resampler
  speex de `libaudioutils`).
- **Cámara:** el Y220 tiene una sola cámara, `gc0329_yuv` (VGA trasera, sin
  frontal ni flash). HAL del stock del Y330 (`camera.mtk.so`) con:
  - el `libcameracustom` del propio Y220 (el del Y330 solo conoce los
    sensores del Y330); las dos llamadas que le faltan
    (`custom_GetYuvPreflashAF`, `custom_GetYuvAfLampSupport`, sin lámpara AF)
    devuelven 0 desde `libmtk_symbols`, igual que en el Y330;
  - `jpeg_mem_dest` y `pmem_*` en `libmtk_symbols`;
  - `libcamera_wrapper` (`camera.mt6572`), que reporta solo la cámara 0 (el
    HAL agrega siempre la TV analógica como segunda trasera y la app la
    abría: sin cuadros), saca `hdr` de `scene-mode-values` y pasa el JPEG
    que llega como `MTK_CAMERA_MSG_EXT_DATA` (el CameraClient de CM lo
    descartaba) como `CAMERA_MSG_COMPRESSED_IMAGE`. También deja solo los
    tamaños de foto que entran en 640x480: el HAL ofrecía hasta 2560x1920
    interpolados desde el sensor VGA.
  Gallery2 deshabilita su `CameraLauncher` si en el primer arranque no hay
  cámara; si quedó así: `pm enable com.android.gallery3d/com.android.camera.CameraLauncher`.
- **Gráfica:** `gralloc.mt6572` + EGL Mali del Y330. Sin hwcomposer: el del
  stock necesita el SurfaceFlinger de MTK.
- **Media:** solo codecs de software (`configs/media_codecs.xml`). Los OMX de
  MTK necesitan el stagefright de MTK.
- **CPU:** `TARGET_ARCH_VARIANT_CPU := cortex-a9`; el GCC 4.6 de 4.2 no
  conoce `cortex-a7`.

## Memoria

219 MB para Linux. El zram (swap comprimido) lo arma el servicio
`compcache` de CM con `ro.zram.default=85` (`system.prop`): 85 % de la RAM,
unos 190 MB como el stock (192 MB). Sin esa propiedad `compcache` corría
`stop` al arrancar y apagaba el swap. Los niveles del lowmemorykiller los
pone ActivityManager.

## Entrada, USB y almacenamiento

- **Teclas:** atrás, home y menú son táctiles de hardware: `mtk-tpd-kpd`
  manda los scancodes 158, 102 y 139. Sin barra virtual. Cada dispositivo
  necesita su `.kl` (`keylayout/`): en `Generic.kl` el 102 es `MOVE_HOME`
  (la tecla Inicio de un teclado, mueve el cursor), no `HOME`.
  `mtk-tpd-kpd.kl` y `mtk-tpd.kl` lo mapean a `HOME`; `mtk-kpd.kl`
  (encendido y volumen) es el del stock.
- **Pantalla táctil:** `idc/mtk-tpd.idc` (`touch.deviceType = touchScreen`).
  El driver no pone `INPUT_PROP_DIRECT` y 4.2 la tomaba por touchpad, con
  puntero de mouse.
- **USB:** `rootdir/init.mt6572.usb.rc` agrega `mass_storage` y
  `mass_storage,adb` (VID 12D1, PID del stock). Sin esas reglas
  UsbDeviceManager dejaba `persist.sys.usb.config=none` y el gadget apagado.
- **microSD:** `configs/vold.fstab` (`/storage/sdcard0`) y el overlay
  `xml/storage_list.xml` con ese mismo punto de montaje.

## Compilar

En el contenedor `cm10.1-builder`:

    export CM_BUILD=y220
    source build/envsetup.sh
    lunch cm_y220-userdebug
    make -j3 bacon

`rild.libargs` lleva un espacio y va en `system.prop`:
`PRODUCT_PROPERTY_OVERRIDES` lo parte en dos líneas y el OTA no lee el
`build.prop`.

## Instalar (TWRP 3.1.1)

1. Respaldar `/data/nvram`, `protect_f`, `protect_s` y `nvram` a la PC
   (`dump_image`). Respaldo del 2026-10-23 en
   `/mnt/fuentes/builds/cm10.1/y220/backup-before-cm101-20261023/`.
2. Desde GB: vaciar `/data` salvo `/data/nvram`, y `/cache`.
3. `adb push` del zip a `/data` y `twrp install`.
4. Antes de reiniciar, comparar `dump_image boot` (los primeros bytes, del
   tamaño del `boot.img`) con el md5 del build.

`ro.adb.secure=1`: la clave de la PC va en `/data/misc/adb/adb_keys`.

### Reparar `system` o `cache` si se pierde el volumen UBI

TWRP no trae `ubimkvol`; `make ubimkvol_y220` lo compila estático:

    erase_image system
    ubiattach 14                       # mtd14 = system, queda vacío
    /tmp/ubimkvol_y220 /dev/ubiN system
    reboot recovery

Igual para `cache` (`mtd15`). UBIFS formatea el volumen nuevo al montarlo.

## Estado (2026-10-23)

| Subsistema | Estado |
| --- | --- |
| Arranque, UBI, Mali-400 (GLES 2.0) | ✅ |
| Pantalla táctil y teclas atrás/home/menú | ✅ |
| microSD, USB (adb, almacenamiento) | ✅ |
| WiFi (WPA2, DHCP, internet) | ✅ con el `wlan_mt6582.ko` arreglado |
| SIM, red, datos móviles (Claro, EDGE) y SMS | ✅ |
| Bluetooth | ✅ enciende y descubre; dirección de NVRAM no |
| Audio | ✅ parlante y micrófono; llamadas sin confirmar |
| Acelerómetro, vibración | ✅ |
| Cámara (gc0329) | ✅ vista previa y foto; video sin probar |
| Llamadas | ❓ fallaron en una prueba sin logs; pendiente |
| Video por hardware (OMX MTK) | ❌ solo software |

Pruebas con QApp (`~/projects/qapp`): los reportes quedan en
`/data/data/com.qapp.romtester/files/reports/` (`run-as com.qapp.romtester`).
