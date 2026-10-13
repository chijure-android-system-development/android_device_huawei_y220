#!/usr/bin/env python
# Copy Y220-U05 stock vendors into vendor/huawei/y220/proprietary.
# Source is the C482B206SP05 system dump, not the live phone.
# Skips AOSP files, Google apps, and odex of the stock framework.

import os
import re
import subprocess
import sys

CM7 = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
STOCK = os.environ.get(
    "STOCK_SYSTEM", "/home/chijure/y220/stock-C482B206SP05/system")
AOSP = os.environ.get(
    "AOSP_SYSTEM", os.path.join(CM7, "out/target/product/y210/system"))
OUT = os.path.join(CM7, "vendor/huawei/y220/proprietary")
LIST = os.path.join(os.path.dirname(__file__), "proprietary-files.txt")
BLOBS = os.path.join(CM7, "vendor/huawei/y220/y220-vendor-blobs.mk")

# Present in a full CM7 image. Not device vendors.
AOSP_EXTRA = set([
    "libharfbuzz.so", "libRS.so", "librs_jni.so", "libsqlite_jni.so",
    "libdrmframework.so", "libdrmframework_jni.so", "libcorkscrew.so",
    "libext4_utils.so", "libxml2.so", "libtinyxml.so", "libgccdemangle.so",
    "libdvm_debug.so", "libsparse.so", "libvo_aacenc.so", "libpjsipjni.so",
    "libpj_opensl_dev.so", "libvoicesearch.so", "libnativecheck-jni.so",
])

# Names that also exist in AOSP, but the stock file is the MTK one.
OVERRIDE_LIBS = set(["libstagefrighthw.so"])
OVERRIDE_BINS = set(["rild"])

# Toolbox applets and generic tools that the Y210 image simply does not ship.
BIN_DENY = set([
    "chmod", "chown", "df", "dmesg", "ln", "ls", "mkdir", "mount", "mv",
    "rm", "umount", "busybox", "atrace", "dhcp6c", "dhcp6ctl", "dhcp6s",
    "drmioserver", "drmserver", "ext4_resize", "ip", "make_ext4fs", "radvd",
    "rtt", "screencap", "shutdown", "tiny_mkswap", "tiny_swapoff",
    "tiny_swapon",
])

MODULE_DENY = set([
    "scsi_tgt.ko", "scsi_wait_scan.ko", "hid-logitech-dj.ko",
])

# Modules built with the device kernel ship from device/huawei/y220/prebuilt/modules
# (kernel-modules.mk, refreshed by update-kernel.sh). The stock ones do not match
# the kernel: the stock ccci_plat needs get_md_gpio_val and the modem never starts.
DEVICE_MODULES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "prebuilt/modules")

ETC_FILES = [
    "etc/mtk_omx_core.cfg",
    "etc/media_codecs.xml",
    "etc/fmr/mt6627_fm_cust.cfg",
    "etc/ril/oper.lis",
    "etc/wifi/p2p.conf",
    "etc/wifi/wpa_supplicant.conf",
    "etc/mtklog-config.prop",
    "etc/vold.fstab.nand",
    "etc/vold.fstab.fat.nand",
]

# The stock framework jars (com.mediatek.framework, mediatek-common, mediatek-op,
# mtklockscreen, DMAgent, wbxml) are 313-byte stubs whose code is in .odex built
# against the stock framework. On CM7 PackageManager throws StaleDexCacheError on
# them and system_server dies in a loop, so none are copied.
FRAMEWORK = []

NEEDED_RE = re.compile(r"\[([^\]]+)\]")


def aosp_names(subdir):
    root = os.path.join(AOSP, subdir)
    names = set()
    if not os.path.isdir(root):
        return names
    for dirpath, _, files in os.walk(root):
        for name in files:
            names.add(name)
    return names


def needed(path):
    try:
        out = subprocess.check_output(["readelf", "-d", path], stderr=subprocess.STDOUT)
    except (subprocess.CalledProcessError, OSError):
        return []
    if not isinstance(out, str):
        out = out.decode("utf-8", "replace")
    libs = []
    for line in out.splitlines():
        if "(NEEDED)" not in line:
            continue
        m = NEEDED_RE.search(line)
        if m:
            libs.append(m.group(1))
    return libs


def stock_map(subdir):
    root = os.path.join(STOCK, subdir)
    found = {}
    if not os.path.isdir(root):
        return found
    for dirpath, _, files in os.walk(root):
        for name in files:
            found.setdefault(name, os.path.join(dirpath, name))
    return found


def rel_from_stock(path):
    return os.path.relpath(path, STOCK)


def main():
    if not os.path.isdir(STOCK):
        sys.exit("missing stock system: %s" % STOCK)
    aosp_lib = aosp_names("lib")
    aosp_bin = aosp_names("bin")
    libs = stock_map("lib")
    bins = stock_map("bin")

    chosen = {}  # relpath -> abs

    def add(path):
        rel = rel_from_stock(path)
        chosen[rel] = path

    seeds = []
    for name, path in sorted(libs.items()):
        if not name.endswith(".so"):
            continue
        if "/modules/" in path.replace("\\", "/"):
            continue
        if name in AOSP_EXTRA:
            continue
        if name not in aosp_lib or name in OVERRIDE_LIBS:
            seeds.append(path)
            add(path)

    for name, path in sorted(bins.items()):
        if name in BIN_DENY:
            continue
        if name not in aosp_bin or name in OVERRIDE_BINS:
            seeds.append(path)
            add(path)

    # DT_NEEDED closure. AOSP libraries stay in the CM build.
    seen = set(seeds)
    queue = list(seeds)
    while queue:
        path = queue.pop()
        for soname in needed(path):
            if soname in aosp_lib or soname in AOSP_EXTRA:
                continue
            dep = libs.get(soname)
            if dep and dep not in seen:
                seen.add(dep)
                add(dep)
                queue.append(dep)

    for rel in ETC_FILES + FRAMEWORK:
        path = os.path.join(STOCK, rel)
        if os.path.isfile(path) and os.path.getsize(path) > 0:
            add(path)

    fw = os.path.join(STOCK, "etc/firmware")
    for dirpath, _, files in os.walk(fw):
        for name in files:
            path = os.path.join(dirpath, name)
            if os.path.getsize(path) > 0:
                add(path)

    mddb = os.path.join(STOCK, "etc/mddb")
    if os.path.isdir(mddb):
        for dirpath, _, files in os.walk(mddb):
            for name in files:
                add(os.path.join(dirpath, name))

    mod = os.path.join(STOCK, "lib/modules")
    if os.path.isdir(mod):
        for name in sorted(os.listdir(mod)):
            if name in MODULE_DENY:
                continue
            if os.path.isfile(os.path.join(DEVICE_MODULES, name)):
                continue
            path = os.path.join(mod, name)
            if os.path.isfile(path):
                add(path)

    if os.path.isdir(OUT):
        for dirpath, _, files in os.walk(OUT, topdown=False):
            for name in files:
                os.remove(os.path.join(dirpath, name))
            if dirpath != OUT:
                os.rmdir(dirpath)

    rels = sorted(chosen)
    for rel in rels:
        dest = os.path.join(OUT, rel)
        if not os.path.isdir(os.path.dirname(dest)):
            os.makedirs(os.path.dirname(dest))
        src = chosen[rel]
        data = open(src, "rb").read()
        open(dest, "wb").write(data)
        os.chmod(dest, 0o644)

    lines = []
    lines.append("# Y220-U05 C482B206SP05. Extracted from the stock system dump.")
    lines.append("# Google apps and AOSP odex are not in this list.")
    lines.append("# lib/modules keeps only stock modules the kernel tree does not build;")
    lines.append("# the rest come from device/huawei/y220/prebuilt/modules.")
    section = None
    for rel in rels:
        top = rel.split("/")[0]
        if top != section:
            lines.append("")
            lines.append("# %s" % top)
            section = top
        lines.append(rel)
    open(LIST, "w").write("\n".join(lines) + "\n")

    mk = ["# Generated by device/huawei/y220/extract-files.py",
          "PRODUCT_COPY_FILES += \\"]
    copies = []
    for rel in rels:
        copies.append("    vendor/huawei/y220/proprietary/%s:system/%s" % (rel, rel))
    mk.append(" \\\n".join(copies))
    open(BLOBS, "w").write("\n".join(mk) + "\n")

    total = sum(os.path.getsize(os.path.join(OUT, r)) for r in rels)
    print("files %d" % len(rels))
    print("bytes %d" % total)
    print("list %s" % LIST)
    print("blobs %s" % BLOBS)


if __name__ == "__main__":
    main()
