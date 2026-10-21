#!/usr/bin/env python3
"""Repack an MTK recovery image with a new ramdisk, keeping its kernel bytes.

usage: repack.py <original.img> <ramdisk root dir> <out.img> [modes_from.cpio]

The cpio is newc with uid/gid 0 and mtime 0, like mkbootfs. Modes come from
the original cpio listing (modes_from), since an unprivileged extraction
keeps them but ownership is lost.
"""
import gzip, os, struct, sys

PART_SIZE = 6291456
MTK_MAGIC = 0x58881688


def read_cpio_modes(raw):
    modes, off = {}, 0
    while off + 110 <= len(raw):
        h = raw[off:off + 110]
        assert h[:6] == b'070701', 'bad cpio'
        f = [int(h[6 + 8 * i:14 + 8 * i], 16) for i in range(13)]
        mode, namesize, filesize = f[1], f[11], f[6]
        name = raw[off + 110:off + 110 + namesize - 1].decode()
        off = (off + 110 + namesize + 3) & ~3
        off = (off + filesize + 3) & ~3
        if name == 'TRAILER!!!':
            break
        modes[name] = mode
    return modes


def cpio_newc(root, modes):
    out, ino = bytearray(), 300000
    entries = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        rel = os.path.relpath(dirpath, root)
        if rel != '.':
            entries.append(rel)
        for f in sorted(filenames):
            entries.append(os.path.normpath(os.path.join(rel, f)))
    for name in sorted(entries):
        path = os.path.join(root, name)
        st = os.lstat(path)
        mode = modes.get(name, st.st_mode)
        if os.path.islink(path):
            data = os.readlink(path).encode()
        elif os.path.isdir(path):
            data = b''
        else:
            data = open(path, 'rb').read()
        nb = name.encode() + b'\0'
        hdr = '070701' + ''.join('%08x' % v for v in (
            ino, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(nb), 0))
        out += hdr.encode() + nb
        out += b'\0' * ((-len(out)) % 4)
        out += data
        out += b'\0' * ((-len(out)) % 4)
        ino += 1
    nb = b'TRAILER!!!\0'
    hdr = '070701' + ''.join('%08x' % v for v in (0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, len(nb), 0))
    out += hdr.encode() + nb
    out += b'\0' * ((-len(out)) % 4)
    return bytes(out), sorted(entries)


def main():
    orig, root, dst = sys.argv[1:4]
    d = open(orig, 'rb').read()
    magic, ksize, kaddr, rsize, raddr, ssize, saddr, tags, page = struct.unpack('<8sIIIIIIII', d[:40])
    assert magic == b'ANDROID!' and ssize == 0
    pages = lambda n: (n + page - 1) // page
    kernel = d[page:page + ksize]
    roff = page + pages(ksize) * page
    old_rd = d[roff:roff + rsize]
    assert struct.unpack('<I', old_rd[:4])[0] == MTK_MAGIC
    old_cpio = gzip.decompress(old_rd[512:])
    modes = read_cpio_modes(old_cpio)

    cpio, names = cpio_newc(root, modes)
    missing = sorted(set(modes) - set(names))
    extra = sorted(set(names) - set(modes))
    print('entries', len(names), 'missing', missing, 'extra', extra)
    gz = gzip.compress(cpio, 9, mtime=0)
    mtk = struct.pack('<II32s', MTK_MAGIC, len(gz), b'RECOVERY') + b'\xff' * (512 - 40)
    rd = mtk + gz

    hdr = bytearray(d[:page])
    struct.pack_into('<I', hdr, 16, len(rd))
    img = bytes(hdr) + kernel + b'\0' * (pages(ksize) * page - ksize)
    img += rd + b'\0' * (pages(len(rd)) * page - len(rd))
    assert len(img) <= PART_SIZE, 'image too big: %d' % len(img)
    img += b'\xff' * (PART_SIZE - len(img))
    open(dst, 'wb').write(img)
    print('kernel', ksize, 'ramdisk', len(rd), 'image', len(img))


if __name__ == '__main__':
    main()
