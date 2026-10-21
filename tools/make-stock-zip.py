#!/usr/bin/env python3
"""make-stock-zip.py: TWRP-flashable zip from a TWRP backup of the stock ROM.

usage: make-stock-zip.py <backup dir> <update-binary> <wipe script> <out.zip>

<backup dir> holds system.ubifs.win (a tar of /system) and boot.mtd.win (raw
boot partition). Files go to system/ in the zip; owners, modes (setuid too)
and symlinks come from the tar, so nothing is guessed. The updater keeps the
UBIFS volume, empties it (same as the CM7 zip), copies, sets permissions and
writes boot with TWRP's flash_image. Data, cache, recovery, nvram and the
modem partitions are not touched.
"""
import hashlib, os, sys, tarfile, time, zipfile

DEVICES = ('y220', 'HWY220-U', 'Y220-U05')


def quote(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    bk, binary, wipe, out = sys.argv[1:5]
    system = tarfile.open(os.path.join(bk, 'system.ubifs.win'))
    boot = open(os.path.join(bk, 'boot.mtd.win'), 'rb').read()
    build_prop = None

    files, links, perms = [], [], []
    for m in system:
        rel = m.name.lstrip('/').rstrip('/')
        if not rel:
            continue
        path = '/system/' + rel
        if m.isreg():
            files.append((rel, m))
            if rel == 'build.prop':
                build_prop = system.extractfile(m).read().decode('utf-8', 'replace')
        elif m.issym():
            links.append((m.linkname, path))
            continue
        elif not m.isdir():
            raise SystemExit('unsupported tar entry: %s' % m.name)
        perms.append((m.uid, m.gid, m.mode & 0o7777, path))

    display = 'stock'
    for line in (build_prop or '').splitlines():
        if line.startswith('ro.build.display.id='):
            display = line.split('=', 1)[1]

    s = []
    s.append('assert(' + ' || '.join(
        'getprop("ro.product.device") == "%s" || getprop("ro.build.product") == "%s"' % (d, d)
        for d in DEVICES) + ');')
    s.append('ui_print("Huawei Y220-U05 stock ROM: %s");' % display)
    s.append('ui_print("Wipe Data and Cache before booting it.");')
    s.append('show_progress(0.700000, 0);')
    s.append('unmount("/system");')
    s.append('assert(mount("ubifs", "UBI", "/dev/ubi0_0", "/system"));')
    s.append('package_extract_file("y220-wipe-system.sh", "/tmp/y220-wipe-system.sh");')
    s.append('set_perm(0, 0, 0755, "/tmp/y220-wipe-system.sh");')
    s.append('assert(run_program("/tmp/y220-wipe-system.sh") == "0");')
    s.append('ui_print("Copying /system...");')
    s.append('package_extract_dir("system", "/system");')
    for target, path in sorted(links, key=lambda l: l[1]):
        s.append('symlink(%s, %s);' % (quote(target), quote(path)))
    s.append('ui_print("Setting permissions...");')
    s.append('show_progress(0.100000, 0);')
    for uid, gid, mode, path in perms:
        s.append('set_perm(%d, %d, 0%o, %s);' % (uid, gid, mode, quote(path)))
    s.append('ui_print("Writing boot...");')
    s.append('show_progress(0.200000, 10);')
    s.append('assert(package_extract_file("boot.img", "/tmp/boot.img"),')
    s.append('       run_program("/sbin/flash_image", "boot", "/tmp/boot.img") == "0",')
    s.append('       delete("/tmp/boot.img"));')
    s.append('unmount("/system");')
    s.append('ui_print("Done.");')
    script = '\n'.join(s) + '\n'

    stamp = time.localtime()[:6]
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        def add(name, data, mode=0o644):
            zi = zipfile.ZipInfo(name, stamp)
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = (0o100000 | mode) << 16
            z.writestr(zi, data)
        add('META-INF/com/google/android/update-binary', open(binary, 'rb').read(), 0o755)
        add('META-INF/com/google/android/updater-script', script)
        add('y220-wipe-system.sh', open(wipe, 'rb').read(), 0o755)
        add('boot.img', boot)
        for rel, m in files:
            add('system/' + rel, system.extractfile(m).read(), m.mode & 0o777)

    print('build', display)
    print('files %d, symlinks %d, set_perm %d' % (len(files), len(links), len(perms)))
    print('boot md5', hashlib.md5(boot).hexdigest())


if __name__ == '__main__':
    main()
