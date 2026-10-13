# TWRP device tree - Huawei Y220-U05

**Device**: Huawei Y220-U05  
**Codename**: y220  
**SoC**: MediaTek MT6572  
**CPU**: Dual-core ARM Cortex-A7  
**Android base**: 2.3.6 Gingerbread (`C482B206SP05`)  
**Stock product**: `HUAWEI/Y220-U05/HWY220-U`

Kernel: [android_kernel_huawei_y220-u05](https://github.com/chijure-android-system-development/android_kernel_huawei_y220-u05), build `#92`

## Partitions

Taken from `/proc/mtd` on the stock `C482B206SP05` firmware. The chip is
Toshiba NAND, 512 MB, page 4096, spare 128, erase size 256 KB.

| Partition | Device | Size |
|-----------|--------|------|
| preloader | `/dev/mtd/mtd0` | 512 KB |
| pro_info | `/dev/mtd/mtd1` | 1 MB |
| nvram | `/dev/mtd/mtd2` | 3 MB |
| protect_f | `/dev/mtd/mtd3` | 3 MB |
| protect_s | `/dev/mtd/mtd4` | 3 MB |
| seccnfg | `/dev/mtd/mtd5` | 256 KB |
| uboot | `/dev/mtd/mtd6` | 512 KB |
| boot | `/dev/mtd/mtd7` | 6 MB |
| recovery | `/dev/mtd/mtd8` | 6 MB |
| secstatic | `/dev/mtd/mtd9` | 256 KB |
| misc | `/dev/mtd/mtd10` | 768 KB |
| logo | `/dev/mtd/mtd11` | 1 MB |
| expdb | `/dev/mtd/mtd12` | 2 MB |
| fat | `/dev/mtd/mtd13` | 17 MB |
| system | `/dev/mtd/mtd14` | 219 MB |
| cache | `/dev/mtd/mtd15` | 66 MB |
| userdata | `/dev/mtd/mtd16` | 172 MB |
| external SD | `/dev/block/mmcblk0p1` | variable |

`system`, `data` and `cache` are UBIFS. `init.recovery.mt6572.rc` attaches them
in stock boot order (`mtd14`, `mtd16`, `mtd15`), which becomes `ubi0`, `ubi1`
and `ubi2`.

## Notes

- Display is 320x480, 32 bpp. Theme is `portrait_mdpi`.
- The prebuilt `kernel` is a raw ARM zImage. It must not already contain the
  512-byte MTK header.
- Recovery images are packed with the local `mtkbootimg` host tool.
  `mtkbootimg --mtk 1` adds a `KERNEL` header and a `RECOVERY` header.
- Boot image layout uses `base=0x10000000`, `pagesize=2048`,
  `ramdisk_offset=0x01000000`, and `tags_offset=0x00000100`. The second
  address used by the stock header is `0x10f00000`.
- The recovery partition is exactly 6291456 bytes. `assert-max-image-size`
  deletes the image if the build goes over that. The image that fits keeps
  only `en.xml` and `es.xml` in the ramdisk and leaves `sbin/sgdisk` out.
- Wipes of `system`, `data` and `cache` are disabled. This TWRP cannot format
  UBIFS, and a wipe would destroy the volume.
- Backups of those three partitions are file tars (`*.ubifs.win`). Boot is a
  raw MTD dump. Store the backup on the micro SD; `/data` is too small.
- `TW_NO_REBOOT_BOOTLOADER := true` because this phone does not expose
  standard fastboot. Unbrick is SP Flash Tool v5 on the preloader, never v6,
  and never Format All.
- USB vendor/product in Android and in this recovery is `12d1:2351`.

## Entering recovery mode

From a booted system:

```bash
adb reboot recovery
```

## Build

```bash
cd /home/chijure/twrp-4.4
source build/envsetup.sh
lunch omni_y220-eng
make recoveryimage
```

Output:

```text
out/target/product/y220/recovery.img
```

The checked-in `kernel` is the prebuilt `make recoveryimage` uses. It is the
`#92` zImage from `android_kernel_huawei_y220-u05`, the same kernel the phone
boots Android with. Device dumchar names are `seccfg` and `sec_ro`; the MTD
names above are the stock ones (`seccnfg`, `secstatic`), which `#92` also uses.

On the phone, `#92` runs with the installed TWRP ramdisk: UBI attaches
mtd14/16/15, `/system`, `/data`, `/cache` and the micro SD mount, display and
touch work. With that ramdisk the image is 6252544 of 6291456 bytes. If a
rebuilt ramdisk pushes it over the partition size, recompressing it with
`pigz -11` (still gzip) saves about 107 KB.

## Flash

Write the raw MTD device. `dd` to `mtdblock8` only erases the partition.
Do not write `mtd7` / boot.

From this recovery, which is already root:

```bash
adb push recovery.img /data/local/tmp/recovery-test.img
adb shell '/data/local/tmp/mtderase; nandwrite /dev/mtd/mtd8 /data/local/tmp/recovery-test.img'
adb reboot recovery
```

From stock Android:

```bash
adb push recovery.img /data/local/tmp/recovery-test.img
adb shell "su -c '/data/local/tmp/mtderase; busybox nandwrite /dev/mtd/mtd8 /data/local/tmp/recovery-test.img'"
adb reboot recovery
```

`mtderase` must erase `/dev/mtd/mtd8` for length `0x600000`. The image written
there is padded with `0xFF` to 6291456 bytes.
