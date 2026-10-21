# Audio

`libaudio` es propio (`libaudio/AudioHardware.cpp`): escribe registros del AFE
(`afe()`, ioctl `SET_AUDSYS_REG`) y del PMIC MT6323 (`ana()`,
`SET_ANAAFE_REG`) en `/dev/eac`. El HAL de stock está dentro de su
`libaudioflinger.so` (`AudioMTKHardware`) y sirve de referencia, no se usa.
La base (parlante, micrófono, tasas) está en el `README.md`; aquí va lo de
2026-10-21.

## Salidas

`applyOutput()` elige la salida:

- música: el `routing=` que manda la política al stream (`setOutDevices`);
- FM: el botón de altavoz (`AudioSetFmSpeaker`); si no es altavoz, auriculares.

Cada salida se abre y se cierra con las secuencias *machine* de
`AnalogOpen`/`AnalogClose` de stock (`outputOpen()`/`outputClose()`):

| Salida | AUDTOP | amplificador |
|---|---|---|
| auriculares | `0x70c=0xf7f2`, `0x700`, `0x70a=0x14`, `0x708=0x7c`, 10 ms, `0x70c=0xf5ba`, `0x70a=0x2214` | `SET_HEADPHONE_ON` (0xa4) |
| altavoz | `0x70a=0x5514`, `0x70e=0x2400`, `0x70c=0xb7f6`, `0x700`, `0x708=0x14`, 10 ms | class-D (`0x70e=0x35b0` … `0x06a`), `SET_SPEAKER_ON` |
| ambos | la de auriculares + class-D | los dos |

Antes los auriculares nunca sonaron: no se mandaba `SET_HEADPHONE_ON` y las
escrituras del altavoz (`0x708=0x14`, `0x70c=0xb7f6`) apagaban el driver de
auriculares. `0x70a=0x5514` en altavoz no está en stock; se deja porque el
altavoz siempre funcionó con él.

## Registros compartidos con el micrófono

El micrófono (`EacStreamIn`) y la salida comparten:

- `AFE_DAC_CON0` (0x10) bit 0, AFE encendido (DL1 es el bit 1, VUL el 3);
- `AFE_IRQ_CON` (0x3a0): IRQ1 (bit 0, tasa en bits 4–7) es DL1, IRQ2 (bit 1,
  tasa en bits 8–11) es el micrófono (`AudDrv_Afe.h`);
- AUDTOP 0x708 (el micrófono no lo escribe).

Bugs que salieron de esto (los dos corregidos):

- `outputClose()` como el `AnalogClose` de stock ponía `0x708=0`: con la
  salida en standby, la grabadora grababa ceros (−91 dB). El cierre ya no
  toca `0x708`/`0x70c`; `outputOpen()` los escribe siempre (`cfd7282`).
- `enableDl()` escribía `AFE_IRQ_CON` con máscara completa y `standbyOut()`
  borraba su byte bajo y apagaba el AFE: cualquier sonido que empezara o se
  apagara durante una grabación apagaba IRQ2 y `AudioRecord` se quedaba sin
  datos. La cámara de video toca un sonido al empezar: la primera grabación
  salía sin pista de audio, `stop()` daba `ERROR_MALFORMED` (−1007) y ANR.
  La salida ahora solo toca IRQ1 y no apaga el AFE con el micrófono activo
  (`03a3d50`).

## FM

Ver `FM_NOTES.md`. `setFm()` deja DL1 apagado si no hay PCM (si corriera sin
datos repetiría el buffer) y `standbyOut()` con FM activo solo para DL1.

## Pruebas útiles

Grabación (nivel con ffmpeg en el PC):

```bash
CLASSPATH=/data/local/tmp/rectest.jar app_process /system/bin RecTest /data/local/tmp/t.3gp 8000
ffmpeg -i t.3gp -af volumedetect -f null -    # ruido ambiente: ~ -60..-70 dB; ceros: -91
```

Grabar con un sonido de por medio (la falla del micrófono): lanzar
`ToneTest` (un `ToneGenerator`) al segundo de empezar `RecTest`. Antes del
arreglo el archivo duraba 0,78 s de 8 s y había `obtainBuffer timed out`.
Los dos programas están en el scratchpad de la sesión (`rectest/`); son
`MediaRecorder` + AMR y `ToneGenerator` compilados con el `framework.jar` de
CM7.

Registros en vivo: el driver imprime cada `SET_AUDSYS_REG`/`SET_ANAAFE_REG`
en dmesg (`AudDrv`).
