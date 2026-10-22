# Llamadas de voz (2026-10-21)

Funcionan en los dos sentidos, con auricular, altavoz y audífono. El audio
lo arma `libaudio` (`EacAudio` en `libaudio/AudioHardware.cpp`), que sigue
a `AudioMTKHardware` de stock (dentro de `libaudioflinger.so`, leído con
Ghidra). El estado de la llamada y los botones dependen de `MediaTekRIL`.

## Control del módem por CCCI

`libaudio` habla con el DSP de voz del módem por dos nodos:

- `/dev/ccci_pcm_tx`: escribe mensajes de 16 bytes
  `{0xffffffff, id << 16 | p16, 5, p32}`.
- `/dev/ccci_pcm_rx`: lee las respuestas (`id | 0x8000`). Con ioctl
  `0x80044301` se obtiene el estado del módem y con `0x80044303` el tamaño
  del buffer compartido, que se mapea con `mmap` sobre este mismo fd.

`init.d/06modem` deja los dos nodos como `9996:audio` (mediaserver corre
como `media` en el grupo `audio`; en stock es root).

| id | Uso |
|---|---|
| `0x2f00` | ganancia de bajada (DL) |
| `0x2f01` | ganancia de subida (UL) |
| `0x2f02` | silenciar UL (botón Silenciar) |
| `0x2f03` | silenciar DL |
| `0x2f04` | sidetone |
| `0x2f10` | frecuencia, 16000 |
| `0x2f20` / `0x2f21` | voz encendida / apagada |
| `0x2f22` | modo: 0 auricular, 1 audífono, 2 altavoz |
| `0x2f24` | máscara de mejoras (`0xffbf`, `0xffffffff`) |
| `0x2f71` / `0x2f73` | parámetros NB (0x5a4 bytes) / WB (0x970 bytes) |

Los parámetros van en el buffer compartido, offset `0x68a`, con la
cabecera `{0xa2a2, 7, len}` y los datos en `0x690`. Son los de fábrica de
stock (`libaudio/speech_param_default.h`). Sin ellos el módem no pasa voz.
Si el módem manda `0xAF70` (pide parámetros), `libaudio` los reenvía en
la siguiente llamada.

## Secuencia

`setMode(IN_CALL)` guarda el modo; el `setParameters(routing)` que llega
después arranca la llamada (`callStart`) y los siguientes cambian de
salida (`callRoute`). Al salir de `IN_CALL` se llama `callStop`.

`callStart`, como `OpenModemSpeechControlFlow` de stock:

1. abre CCCI y manda los parámetros si hace falta;
2. pone la reproducción en standby (la llamada se queda con el AFE);
3. `callDigital`: conexiones del AFE ADC → módem y módem → DAC, PCM2;
4. `callMic`: micrófono principal o de audífono;
5. `callOutput`: auricular (`SET_EARPIECE_ON` 0xa6), altavoz o audífono;
6. `callVolume`, frecuencia, modo y `0x2f20`.

Mientras dura la llamada, `writePcm` descarta el audio de las apps al
ritmo del stream, como stock.

Volumen: `deg = (-34.74 * ln(v) + 0.5) >> 2`, en pasos de 0,25 dB. Primero
baja el buffer analógico y el resto va al módem con `0x2f00`.

## Botones Silenciar y Teclado

Estaban desactivados porque la llamada se quedaba en DIALING. `mtk-ril`
avisa el progreso solo con su URC `+ECPI` (`UNSOL_CALL_PROGRESS_INFO`,
3004), que RILJ no conoce ("Unrecognized unsol response: 3004"), y nunca
manda `CALL_STATE_CHANGED`. `MediaTekRIL.processUnsolicited` lo convierte
en `RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED`; `GsmCallTracker` vuelve a
pedir `GET_CURRENT_CALLS` y la llamada pasa a ACTIVE.

`mtk-ril` responde `GENERIC_FAILURE` a `RIL_REQUEST_SET_MUTE`. El overlay
de Phone pone `send_mic_mute_to_AudioManager=true`: Silenciar llega a
`setMicMute` de `libaudio`, que manda `0x2f02` al módem.

## Log útil

```
adb logcat -s AudioEac:* RILJ:* GsmCallTracker:*
```

`speech parameters sent`, `call on, device N` y `call volume ...` salen de
`libaudio`. `speech msg 0x.... : no ack` indica que el módem no respondió.
