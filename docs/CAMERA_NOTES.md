# Cámara (2026-10-21)

La base (pila de stock, YV12 → RGB565, I420 para video) está en el
`README.md`. Esto es lo que salió probando foto y video.

## Vista previa que se corría después de la foto

Síntoma: tras tomar una foto la vista previa se desplazaba de derecha a
izquierda sin parar hasta cerrar y abrir la cámara.

Causa: después de `takePicture`, el HAL destruye y recrea su cliente de
pantalla y el `CameraClient` de stock vuelve a llamar `registerPreviewBuffers`
con un heap nuevo sin desregistrar el anterior. `LayerBuffer::registerBuffers`
de CM7 rechaza eso (`if (mSource != 0) return INVALID_OPERATION`):

```
CameraClient: [registerPreviewBuffers] strideW:480, strideH:368, w:480, h:368
E/CameraClient: registerBuffers failed with status -38
```

Los `postBuffer(offset)` del heap nuevo se aplicaban sobre el viejo.

Arreglo: `frameworks_base_surfaceflinger_reregister.patch` llama
`unregisterBuffers()` al principio de `registerBuffers()` cuando el board pone
`BOARD_SURFACEFLINGER_REREGISTER_BUFFERS := true` (solo el Y220). El Y210 y el
C660 conservan el comportamiento de AOSP. Verificado en el log: el registro
después de la foto ya no falla.

## Resolución de foto

Las fotos salían de 320×240 porque la preferencia de la app quedó en
`320x240`. El HAL ofrece `picture-size-values=320x240,480x320,640x480,1280x960`
y por defecto 640×480 (sensor GC0329, VGA). Se cambia en Ajustes de la cámara.

## Video

Graba H.263 352×288 a 10 fps con audio AMR-NB. El audio se cortaba por un bug
de `libaudio` (ver `AUDIO_NOTES.md`, "Registros compartidos con el micrófono").

## Ver la pantalla desde el PC

```bash
bash device/huawei/y210/tools/screenshot.sh salida.png -s Y220-U05
```

Usa `/system/bin/screenshot` de CM (pasa por SurfaceFlinger); GB no trae
`screencap`. El framebuffer es 320×480 a 32 bpp (`virtual_size 320,960`).

## Log

`MtkCam` escribe varias líneas por cuadro y llena el búfer del logcat de GB en
segundos (no hay `logcat -G`). Para capturar una prueba completa:

```bash
adb logcat -v time | grep --line-buffered -vE "queryImgBufferSize|Msync|UMP" > camara.txt
```
