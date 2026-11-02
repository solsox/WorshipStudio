#!/usr/bin/env python3
"""Extracts WorshipStudio's own texts (tr() and translate("WorshipStudio", ...))
and updates app/frontend/data/worship/idiomas/: plantilla.json lists every text;
each <idioma>.json keeps its translations and gets the missing keys empty.

Usage: python3 herramientas/textos.py          (update)
       python3 herramientas/textos.py --faltan (list missing per language)
"""
import glob, json, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'app', 'frontend')
OUT = os.path.join(ROOT, 'data', 'worship', 'idiomas')
SOURCES = glob.glob(os.path.join(ROOT, 'docks', '*.cpp')) + glob.glob(os.path.join(ROOT, 'widgets', 'OBSBasic*.cpp'))
SKIP = {'WorshipSelfTest.cpp'}

CALL = re.compile(r'(?:\btr|(?:QCoreApplication::translate|QT_TRANSLATE_NOOP)\(\s*"WorshipStudio"\s*,|QDialog::tr|QFileDialog::tr)\(?\s*((?:"(?:[^"\\]|\\.)*"\s*)+)')
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')

# Texts that reach tr() through a table (colors, backgrounds), kept after the
# extracted ones so numbering of existing texts does not move.
EXTRA = ['Ciruela', 'Negro mate', 'Negro profundo', 'Negro brillante', 'Blanco', 'Vainilla', 'Hueso',
         'Brasa', 'Indigo', 'Esmeralda', 'Azul', 'Violeta', 'Rosa', 'Dorado', 'Turquesa']

def unescape(text):
    return re.sub(r'\\(.)', lambda m: {'n': '\n', 't': '\t'}.get(m.group(1), m.group(1)), text)

def extract():
    texts = set()
    for path in SOURCES:
        if os.path.basename(path) in SKIP:
            continue
        source = open(path, encoding='utf-8').read()
        for match in CALL.finditer(source):
            text = unescape(''.join(LIT.findall(match.group(1))))
            if text.strip():
                texts.add(text)
    return sorted(texts) + [t for t in EXTRA if t not in texts]

def main():
    texts = extract()
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'plantilla.json'), 'w', encoding='utf-8') as f:
        json.dump({t: '' for t in texts}, f, ensure_ascii=False, indent=1)
    for path in sorted(glob.glob(os.path.join(OUT, '*.json'))):
        if path.endswith('plantilla.json'):
            continue
        current = json.load(open(path, encoding='utf-8'))
        merged = {t: current.get(t, '') for t in texts}
        missing = [t for t, v in merged.items() if not v]
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(merged, f, ensure_ascii=False, indent=1)
        print(f'{os.path.basename(path)}: {len(texts) - len(missing)}/{len(texts)}')
        if '--faltan' in sys.argv:
            for t in missing:
                print('   ', repr(t))
    print(f'{len(texts)} textos')

main()
