#!/usr/bin/env python3
"""symcheck.py: missing libraries and unresolved symbols in a /system copy.

usage: symcheck.py <system dir>

For every ELF under <system dir> it prints the NEEDED libraries that are
not in lib/, lib/hw or lib/egl, and the undefined (non-weak) dynamic
symbols that no library of its NEEDED closure defines. Lines are
"<path>: NEEDED <lib>" and "<path>: UNDEF <symbol>".
"""
import os, re, subprocess, sys

root = sys.argv[1]
LIBDIRS = [os.path.join(root, d) for d in ('lib', 'lib/hw', 'lib/egl')]
ENV = dict(os.environ, LC_ALL='C')
cache = {}


def info(path):
    if path not in cache:
        out = subprocess.run(['readelf', '-dW', '--dyn-syms', path],
                             capture_output=True, text=True, env=ENV).stdout
        needed = re.findall(r'\(NEEDED\)\s+Shared library: \[(.*?)\]', out)
        defs, undefs = set(), set()
        for line in out.splitlines():
            f = line.split()
            if len(f) < 8 or not f[0].endswith(':') or not f[0][:-1].isdigit():
                continue
            name = f[7].split('@')[0]
            if f[6] == 'UND':
                if f[4] != 'WEAK' and name:
                    undefs.add(name)
            elif f[4] in ('GLOBAL', 'WEAK', 'UNIQUE'):
                defs.add(name)
        cache[path] = (needed, defs, undefs)
    return cache[path]


def find(name):
    for d in LIBDIRS:
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    return None


def is_elf(path):
    try:
        with open(path, 'rb') as f:
            return f.read(4) == b'\x7fELF'
    except OSError:
        return False


def main():
    for dirpath, _, files in os.walk(root):
        for name in sorted(files):
            path = os.path.join(dirpath, name)
            if os.path.islink(path) or not is_elf(path):
                continue
            rel = os.path.relpath(path, root)
            seen, todo, missing = set(), [path], []
            while todo:
                p = todo.pop()
                if p in seen:
                    continue
                seen.add(p)
                for n in info(p)[0]:
                    q = find(n)
                    if q:
                        todo.append(q)
                    elif p == path:
                        missing.append(n)
            for n in missing:
                print('%s: NEEDED %s' % (rel, n))
            # A missing NEEDED already explains its symbols.
            if missing:
                continue
            defs = set()
            for p in seen:
                defs |= info(p)[1]
            for s in sorted(info(path)[2] - defs):
                print('%s: UNDEF %s' % (rel, s))


if __name__ == '__main__':
    main()
