# Video por hardware (2026-10-21)

La reproducción de H.264, MPEG4 y H.263 usa los decoders de MTK
(`OMX.MTK.VIDEO.DECODER.*`, `libMtkOmxVdec.so`). La grabación sigue con el
encoder de software de CM7 (H.263 352×288, ver `CAMERA_NOTES.md`).

## Por qué no cargaban

`blobcheck` (ver `SYSTEM_NOTES.md`) mostraba toda la cadena caída:

- `libsched.so` → `libvcodec_utility.so` → `libvcodecdrv.so` →
  `libMtkOmxVdec.so`/`libMtkOmxVenc.so` y los `libh264*`/`libmp4*`:
  `sched_getaffinity`, `sched_setaffinity` y `__pthread_gettid` del bionic
  de MTK.
- `libstagefrighthw.so` (el plugin OMX): constructor de
  `MemoryHeapBaseMTK` del libbinder de MTK.
- `libMtkOmxVdec.so`: `AwesomePlayer::mtk_omx_get_current_time` del
  libstagefright de MTK.

## libmtk_shim

Se precarga solo en mediaserver (`setenv LD_PRELOAD libmtk_shim.so` en el
servicio `media` de `init.rc`). El linker de GB busca primero en las
precargas, así que no hace falta tocar bionic ni los blobs.

- `sched_*`: syscalls 241/242. `sched_getaffinity` devuelve 0 y limpia el
  resto de la máscara, como glibc.
- `__pthread_gettid`: `kernel_id` de `pthread_internal_t`.
- `MemoryHeapBaseMTK(fd, base, size, flags, device)`: en stock es
  `MemoryHeapBase()` más `init(dup(fd), base, size, flags, device)`, sin
  miembros propios (desensamblado de `libbinder.so` de stock). `init` deja
  `mNeedUnmap` en false, así que el heap no desmapea `base`.
- `mtk_omx_get_current_time`: devuelve -1, lo mismo que stock mientras
  hace seek. El `AwesomePlayer` de CM7 nunca le da su puntero al decoder,
  así que el decoder no descarta cuadros atrasados.

## Driver

`init.d/10vcodec` carga `vcodec_kernel_driver.ko` y crea `/dev/Vcodec`
(`c 160 0`). Stock lo deja `system:media`, pero ahí mediaserver es root.
En CM7 corre con uid `media` y gid `system` (primer `group` del servicio),
así que el nodo es `media:system 0660`. Con el dueño de stock el decoder
fallaba con `/dev/Vcodec open failed` y `OMXCodec` esperaba sin fin.

## OMXCodec y el renderer

`frameworks_base_stagefright_mtk_omx.patch`, con
`BOARD_USES_MTK_OMX := true`:

- `kDecoderInfo` pone `OMX.MTK.VIDEO.DECODER.AVC`, `MPEG4` y `H263`
  primero. Las miniaturas piden software primero y no cambian.
- El decoder entrega YUV420 planar (`0x13`) con las líneas de Y rellenas a
  32 píxeles y las de UV a 16. Lo reporta en `nStride`/`nSliceHeight`
  (176×144 sale 192×144; 320×180 sale 320×192). Para `OMX.MTK.*`,
  `initOutputFormat` usa esos valores como tamaño decodificado.
- `SoftwareRenderer` registraba un RGB565 de 192 de stride con recorte a
  176, y el `LayerBuffer` del Y220 ignora `hor_stride`: la imagen salía en
  rayas diagonales. Con el flag, si el tamaño decodificado difiere del
  visible, copia el área visible de los tres planos a un espacio extra del
  mismo heap y convierte y registra solo el tamaño visible.

Los quirks propios de MTK en el `getComponentQuirks` de stock (`0x2000`
decoder, `0x4000` encoder) solo agregan metadata a los buffers y guardan
la referencia del buffer de entrada; no cambian cómo se asignan.

## Medido

Con `stagefright` en el teléfono:

| Video | Hardware | Software |
|---|---|---|
| H.264 640×480 1,5 Mbps, por cuadro | 13–15 ms | 26,8 ms |
| CPU total del sistema, mismo video | 362 ticks | 680 ticks |

Imagen revisada con capturas del framebuffer en 176×144, 192×144,
320×180, 320×240 y H.263 352×288. 20 aperturas y cierres seguidos del
decoder dejaron igual la memoria de mediaserver.

## Probar

```
adb push video.mp4 /data/local/tmp/
adb shell stagefright /data/local/tmp/video.mp4      # hardware
adb shell stagefright -s /data/local/tmp/video.mp4   # software
adb logcat -s OMXCodec:* MtkOmxVdec:* VDO_LOG:*
```

`[OMX.MTK.VIDEO.DECODER.AVC] allocating ... on output port` confirma el
decoder de hardware.

## Pendiente

- Probar sincronía de audio y video y seek en videos largos y pesados.
- Encoders MTK (`OMX.MTK.VIDEO.ENCODER.*`): cargan, pero no están en
  `kEncoderInfo`; la cámara manda I420 por `CameraSource`.
