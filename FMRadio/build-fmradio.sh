#!/bin/bash
# build-fmradio.sh — rebuilds FMRadio.apk from the stock Y220 app.
#
# Stock FMRadio is odexed against the MediaTek framework and plays audio with a
# MediaTek-only player type ("MEDIATEK://MEDIAPLAYER_PLAYERTYPE_FM"). This:
#   1. deodexes it with the stock bootclasspath;
#   2. adds src/ (FmPlayer, which switches libaudio's digital FM path through
#      AudioSetFmDigitalEnable, and stubs for Xlog / PluginManager);
#   3. makes the service create FmPlayer instead of MediaPlayer;
#   4. declares ACCESS_MTK_MMHW in the manifest (CM7 does not define it, so the
#      platform.xml gid mapping is ignored otherwise).
#   5. volume follows STREAM_MUSIC (GB has no STREAM_FM) and the speaker
#      switch goes through FmPlayer.setForceUse (GB has no FOR_PROPRIETARY).
# The result is unsigned; the build signs it with the platform key.
#
# Host tools: baksmali, smali, apktool, zip. javac/dx run in cm7-builder.
#
# Usage: build-fmradio.sh <stock /system dir>

set -e

STOCK=${1:?usage: $0 <stock /system dir>}
HERE=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
BCP=core.odex:bouncycastle.odex:ext.odex:framework.odex:android.policy.odex:services.odex:core-junit.odex:mtklockscreen.odex:mediatek-common.odex:mediatek-op.odex

baksmali x -a 10 -d "$STOCK/framework" -b "$BCP" -o "$W/smali" "$STOCK/app/FMRadio.odex"

# src/ -> dex -> smali (JDK 6 and dx from the CM7 tree).
mkdir -p "$W/src"
cp "$HERE"/src/*.java "$W/src/"
docker exec cm7-builder rm -rf /tmp/fmradio-src
docker cp "$W/src" cm7-builder:/tmp/fmradio-src
docker exec cm7-builder bash -c 'cd /tmp/fmradio-src && mkdir -p classes && \
    O=~/cm7/out/target/common/obj/JAVA_LIBRARIES && \
    javac -bootclasspath $O/framework_intermediates/classes.jar:$O/core_intermediates/classes.jar \
        -source 1.5 -target 1.5 -d classes *.java && \
    ~/cm7/out/host/linux-x86/bin/dx --dex --output=src.dex classes'
docker cp cm7-builder:/tmp/fmradio-src/src.dex "$W/src.dex"
baksmali d -o "$W/smali" "$W/src.dex"

python3 - "$W/smali/com/mediatek/FMRadio" <<'EOF'
import sys
d = sys.argv[1]
for name in ('FMRadioService.smali', 'FMRadioService$3.smali'):
    p = d + '/' + name
    s = open(p).read()
    for reg in ('v1', 'v2'):
        old = ('    new-instance %s, Landroid/media/MediaPlayer;\n\n'
               '    invoke-direct {%s}, Landroid/media/MediaPlayer;-><init>()V\n') % (reg, reg)
        new = old.replace('Landroid/media/MediaPlayer;', 'Lcom/mediatek/FMRadio/FmPlayer;')
        s = s.replace(old, new)
    assert 'FmPlayer' in s, name
    if name == 'FMRadioService.smali':
        old = 'invoke-static {v0, v1}, Landroid/media/AudioSystem;->setForceUse(II)I'
        assert s.count(old) == 1
        s = s.replace(old, old.replace('Landroid/media/AudioSystem;', 'Lcom/mediatek/FMRadio/FmPlayer;'))
    open(p, 'w').write(s)

# MediaTek STREAM_FM (10) does not exist on GB; FmPlayer follows STREAM_MUSIC.
import re
for name in ('FMRadioActivity.smali', 'FMRadioEMActivity.smali', 'FMRadioFavorite.smali'):
    p = d + '/' + name
    s, n = re.subn(r'const/16 (v\d+), 0xa\n\n(    invoke-virtual \{p0, \1\}, Landroid/app/Activity;->setVolumeControlStream)',
                   r'const/4 \1, 0x3\n\n\2', open(p).read())
    assert n == 1, name
    open(p, 'w').write(s)
EOF
grep -rq "new-instance v.*, Landroid/media/MediaPlayer;" "$W/smali/com/mediatek/FMRadio/FMRadioService.smali" \
    && { echo "FMRadioService still creates a MediaPlayer"; exit 1; }

smali a -a 10 -o "$W/classes.dex" "$W/smali"

apktool d -s -f -o "$W/dec" "$STOCK/app/FMRadio.apk" >/dev/null
sed -i 's|    <uses-permission android:name="android.permission.ACCESS_MTK_MMHW"/>|    <permission android:name="android.permission.ACCESS_MTK_MMHW" android:protectionLevel="signatureOrSystem"/>\n&|' \
    "$W/dec/AndroidManifest.xml"
apktool b -o "$W/rebuilt.apk" "$W/dec" >/dev/null

mkdir "$W/apk"
(cd "$W/apk" && unzip -q "$STOCK/app/FMRadio.apk" -x 'META-INF/*' 'lib/*' && \
    unzip -o -q "$W/rebuilt.apk" AndroidManifest.xml && cp "$W/classes.dex" . && \
    zip -q -r "$W/FMRadio.apk" .)
cp "$W/FMRadio.apk" "$HERE/FMRadio.apk"
echo "OK: $HERE/FMRadio.apk"
