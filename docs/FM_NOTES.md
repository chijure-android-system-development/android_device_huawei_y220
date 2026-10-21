# Radio FM

Chip MT6627 dentro del combo WMT. Funciona con la app de stock: sintonía,
búsqueda, volumen, auriculares y altavoz (2026-10-21). Sin RDS: la app dice
`isRDSSupported: false` con este driver.

## Piezas

| Pieza | Qué hace |
|---|---|
| `init.d/07wmt` | `insmod mtk_fm_drv.ko` después de WMT; `/dev/fm` `c 193 0`, `system:media` 0660 (como el `init.rc` de stock) |
| `FMRadio/FMRadio.apk` | app de stock rehecha por `FMRadio/build-fmradio.sh` (sin firmar; el build la firma con la clave platform) |
| `FMRadio/com.mediatek.fmradio.xml` | `ACCESS_MTK_MMHW` → gid `media`, como el `platform.xml` de stock |
| `libaudio` | ruta FM digital, volumen y salida |
| blobs | `libfmjni.so`, `libfmmt6627.so`, `libfmcust.so`, `etc/fmr/mt6627_fm_cust.cfg`, firmware `mt6627` |

## La app

La app de stock está odexada contra el framework de MediaTek y reproduce con
un tipo de player propio (`MEDIATEK://MEDIAPLAYER_PLAYERTYPE_FM`) que CM7 no
tiene. `build-fmradio.sh <system de stock>`:

1. la deodexa con el bootclasspath de stock (baksmali, API 10);
2. agrega `src/`: `FmPlayer` y stubs de `Xlog` y `PluginManager`;
3. hace que `FMRadioService` cree `FmPlayer` en vez de `MediaPlayer`;
4. pasa `setVolumeControlStream(10)` (STREAM_FM de MTK) a 3 (STREAM_MUSIC)
   en las tres actividades;
5. cambia `AudioSystem.setForceUse(4, …)` (FOR_PROPRIETARY de MTK) por
   `FmPlayer.setForceUse`;
6. declara `ACCESS_MTK_MMHW` en el manifiesto. CM7 no la define y un permiso
   que solo está en `platform.xml` no se concede.

`FmPlayer` manda a `libaudio` por `AudioSystem.setParameters`:

- `AudioSetFmDigitalEnable=1|0` (start/stop/pause/reset/release);
- `AudioSetFmVolume=<i>;AudioSetFmVolumeMax=<max>` (índice de STREAM_MUSIC,
  escucha `VOLUME_CHANGED_ACTION`);
- `AudioSetFmSpeaker=1|0` (botón de altavoz).

El script verifica que el resultado no cree ningún `MediaPlayer` en
`FMRadioService`. Regenerado y comparado con el APK hecho a mano: mismo
smali y mismos recursos.

## Ruta de audio

Sale del HAL de stock (`AudioMTKHardware`, dentro de `libaudioflinger.so`):
`SetFmChip` + `SetFmDirectConnection`. El MT6627 entra por la merge
interface al 2.º I2S-in (I00/I01) a 32 kHz, el ASRC lo lleva a 44100, GAIN2
da el volumen y la interconexión `AFE_GAIN2_CONN` (0x438) lo manda a
O03/O04 (DAC). Se mezcla en hardware con DL1, así que la música no lo corta.

- `AUDDRV_SET_FM_I2S_GPIO` (0xc0044362) pasa el combo a AIF_2 (FM digital).
  Al apagar, `AUDDRV_RESET_BT_FM_GPIO` (0xc0044360). Sin esto no suena.
- `FM_IOCTL_I2S_SETTING` no hace falta: en MT6627 `i2s_set` está en `#if 0`.
- Volumen: tabla `kFmGain2` de 17 pasos de stock (`0x80000` = 0 dB) en
  `AFE_GAIN2_CON1` (0x42c).
- Stock hace la ruta directa solo para auriculares; para altavoz captura y
  reproduce por software. Aquí se usa la directa también para altavoz y suena
  bien.

Secuencia completa con offsets en `libaudio/AudioHardware.cpp`
(`EacAudio::setFm`). El volcado de Ghidra de stock quedó en el scratchpad de
la sesión 20dad9c5 (`fmrev/fm_stock_decomp_full.txt`), no en el árbol.

## Probar

```bash
adb shell 'lsmod | grep mtk_fm_drv; ls -l /dev/fm'
adb shell dumpsys package com.mediatek.FMRadio | grep gids   # incluye 1013
adb logcat | grep -E "FmPlayer|AudioEac|FMLIB"
```

Esperado al encender: `FMLIB_CORE ... chipid=0x6627`, `powerup ... ret=0`,
`AudioSetFmDigitalEnable=1`, `fm digital path on` y en dmesg
`cmb_stub aif_ctrl_cb state(0->2)`. Sin auriculares la app pide la antena.
