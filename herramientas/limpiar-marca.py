#!/usr/bin/env python3
"""Replaces the original program's name with WorshipStudio in every locale
.ini under the given folders (texts only: keys, links and identifiers stay).
Used on the sources and, in CI, on the installed app so third-party plugins
(submodules) show WorshipStudio too.

Usage: python3 herramientas/limpiar-marca.py <carpeta> [<carpeta>...]
"""
import os, re, sys

URL = re.compile(r"(https?://[^\s'\"<>]+|com\.obsproject\.\w+|obs-[a-z0-9-]+\.[a-z]+)")
RULES = [
    (re.compile(r'OBS ?Studio'), 'WorshipStudio'),
    (re.compile(r'OBSProject|OBS Project'), 'WorshipStudio'),
    (re.compile(r'obs-websocket', re.I), 'WebSocket'),
    (re.compile(r"(?:(?<=\\n)|(?<![A-Za-z0-9_./\\]))OBS(?![A-Z0-9_])"), 'WorshipStudio'),
]


def clean_value(value):
    parts = URL.split(value)
    for i in range(0, len(parts), 2):
        for pattern, replacement in RULES:
            parts[i] = pattern.sub(replacement, parts[i])
    return ''.join(parts)


def clean_file(path):
    with open(path, encoding='utf-8', errors='surrogateescape') as f:
        lines = f.read().split('\n')
    changed = 0
    for i, line in enumerate(lines):
        if '=' not in line or line.lstrip().startswith((';', '#')):
            continue
        key, value = line.split('=', 1)
        new = clean_value(value)
        if new != value:
            lines[i] = key + '=' + new
            changed += 1
    if changed:
        with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
            f.write('\n'.join(lines))
    return changed


def main():
    total = 0
    for root_dir in sys.argv[1:]:
        for root, _, files in os.walk(root_dir):
            if os.sep + 'locale' not in root + os.sep and not root.endswith('locale'):
                continue
            for name in files:
                if name.endswith('.ini'):
                    total += clean_file(os.path.join(root, name))
    print(f'{total} textos con la marca anterior reemplazados')


main()
