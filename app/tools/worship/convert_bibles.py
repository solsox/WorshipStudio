#!/usr/bin/env python3
"""Converts free-licensed Bibles (USFX, OSIS, USFM) into WorshipStudio JSON.

Usage: convert_bibles.py <source-dir> <output-dir>

Sources (see README "Biblias incluidas"):
  https://github.com/seven1m/open-bibles  (spa-*.usfx.xml, eng-kjv.osis.xml)
  https://github.com/MC1171611/valera1865  (USFM/*.usfm, copied to <source-dir>/rv1865)
"""
import glob
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

CODES = ("GEN EXO LEV NUM DEU JOS JDG RUT 1SA 2SA 1KI 2KI 1CH 2CH EZR NEH EST JOB PSA PRO ECC SNG ISA JER LAM "
         "EZK DAN HOS JOL AMO OBA JON MIC NAM HAB ZEP HAG ZEC MAL MAT MRK LUK JHN ACT ROM 1CO 2CO GAL EPH PHP COL "
         "1TH 2TH 1TI 2TI TIT PHM HEB JAS 1PE 2PE 1JN 2JN 3JN JUD REV").split()
OSIS = ("Gen Exod Lev Num Deut Josh Judg Ruth 1Sam 2Sam 1Kgs 2Kgs 1Chr 2Chr Ezra Neh Esth Job Ps Prov Eccl Song "
        "Isa Jer Lam Ezek Dan Hos Joel Amos Obad Jonah Mic Nah Hab Zeph Hag Zech Mal Matt Mark Luke John Acts Rom "
        "1Cor 2Cor Gal Eph Phil Col 1Thess 2Thess 1Tim 2Tim Titus Phlm Heb Jas 1Pet 2Pet 1John 2John 3John Jude "
        "Rev").split()
NAMES_ES = ["Génesis", "Éxodo", "Levítico", "Números", "Deuteronomio", "Josué", "Jueces", "Rut", "1 Samuel",
            "2 Samuel", "1 Reyes", "2 Reyes", "1 Crónicas", "2 Crónicas", "Esdras", "Nehemías", "Ester", "Job",
            "Salmos", "Proverbios", "Eclesiastés", "Cantares", "Isaías", "Jeremías", "Lamentaciones", "Ezequiel",
            "Daniel", "Oseas", "Joel", "Amós", "Abdías", "Jonás", "Miqueas", "Nahúm", "Habacuc", "Sofonías",
            "Hageo", "Zacarías", "Malaquías", "Mateo", "Marcos", "Lucas", "Juan", "Hechos", "Romanos",
            "1 Corintios", "2 Corintios", "Gálatas", "Efesios", "Filipenses", "Colosenses", "1 Tesalonicenses",
            "2 Tesalonicenses", "1 Timoteo", "2 Timoteo", "Tito", "Filemón", "Hebreos", "Santiago", "1 Pedro",
            "2 Pedro", "1 Juan", "2 Juan", "3 Juan", "Judas", "Apocalipsis"]
NAMES_EN = ["Genesis", "Exodus", "Leviticus", "Numbers", "Deuteronomy", "Joshua", "Judges", "Ruth", "1 Samuel",
            "2 Samuel", "1 Kings", "2 Kings", "1 Chronicles", "2 Chronicles", "Ezra", "Nehemiah", "Esther", "Job",
            "Psalms", "Proverbs", "Ecclesiastes", "Song of Solomon", "Isaiah", "Jeremiah", "Lamentations",
            "Ezekiel", "Daniel", "Hosea", "Joel", "Amos", "Obadiah", "Jonah", "Micah", "Nahum", "Habakkuk",
            "Zephaniah", "Haggai", "Zechariah", "Malachi", "Matthew", "Mark", "Luke", "John", "Acts", "Romans",
            "1 Corinthians", "2 Corinthians", "Galatians", "Ephesians", "Philippians", "Colossians",
            "1 Thessalonians", "2 Thessalonians", "1 Timothy", "2 Timothy", "Titus", "Philemon", "Hebrews", "James",
            "1 Peter", "2 Peter", "1 John", "2 John", "3 John", "Jude", "Revelation"]

# Words that keep an initial capital when an all-caps word is normalized.
PROPER = {"jehová", "dios", "señor", "jesús", "jesus", "cristo", "israel", "jerusalén", "david", "moisés", "pablo",
          "pedro", "juan", "santiago", "judas", "salomón", "abraham", "josué", "noé", "yavé", "jah", "babilonia",
          "nazareno", "lord", "god", "jehovah", "jews", "israel"}


def fix_caps(text):
    """Normalizes words printed in capitals (drop caps, LORD/SEÑOR convention, inscriptions):
    'Y ACONTECIÓ después' -> 'Y aconteció después', 'JEHOVÁ es' -> 'Jehová es',
    'REY DE LOS JUDÍOS' -> 'Rey de los judíos'."""
    def word(match):
        w = match.group(0)
        if len(w) < 2 or not w.isupper():
            return w
        low = w.lower()
        before = text[:match.start()].rstrip(" \"'“‘«¿¡(")
        sentence_start = before == "" or before[-1] in ".?!:;"
        if low in PROPER or sentence_start:
            return low[0].upper() + low[1:]
        return low
    return re.sub(r"[A-Za-zÁÉÍÓÚÑÜáéíóúñü]+", word, text)


def clean(text):
    text = re.sub(r"\s+", " ", text).strip()
    text = re.sub(r"\s+([,.;:?!»)])", r"\1", text)
    return text


def finish(books, names, meta):
    out = []
    for i, code in enumerate(CODES):
        chapters = books.get(code)
        if not chapters:
            raise SystemExit("%s: missing book %s" % (meta["abbrev"], code))
        chapter_list = []
        for c in range(1, max(chapters) + 1):
            verses = chapters.get(c, {})
            count = max(verses) if verses else 0
            chapter_list.append([fix_caps(clean(verses.get(v, ""))) for v in range(1, count + 1)])
        out.append({"name": names[i], "chapters": chapter_list})
    total = sum(len(c) for b in out for c in b["chapters"])
    empty = sum(1 for b in out for c in b["chapters"] for v in c if not v)
    print("%-8s %d libros, %d capitulos, %d versiculos (%d vacios)" % (
        meta["abbrev"], len(out), sum(len(b["chapters"]) for b in out), total, empty))
    return dict(meta, books=out)


def parse_usfx(path):
    skip = {"f", "x", "fe", "s", "h", "toc", "id", "ide", "rem", "d", "ms", "mt", "cl", "ca", "va", "vp", "fig",
            "periph", "toc1", "toc2", "toc3", "sts", "restore", "ref"}
    books = {}
    state = {"book": None, "c": 0, "v": 0}

    def add(text):
        if text and state["book"] in books and state["v"] > 0:
            ch = books[state["book"]].setdefault(state["c"], {})
            ch[state["v"]] = ch.get(state["v"], "") + text

    def walk(el):
        tag = el.tag.split("}")[-1]
        if tag == "book":
            code = el.get("id")
            state.update(book=code, c=0, v=0)
            if code in CODES:
                books[code] = {}
        elif tag == "c":
            state.update(c=int(re.sub(r"\D", "", el.get("id"))), v=0)
        elif tag == "v":
            state["v"] = int(re.match(r"\d+", el.get("id")).group(0))
        elif tag == "ve":
            pass
        if tag not in skip:
            add(el.text)
            for child in el:
                walk(child)
                add(child.tail)
        if tag == "ve":
            state["v"] = 0
        if tag in ("p", "q") and state["v"]:
            add(" ")

    walk(ET.parse(path).getroot())
    return books


def parse_osis(path):
    books = {}
    current = {"ref": None}
    skip = {"note", "title", "rdg"}

    def add(text):
        if text and current["ref"]:
            b, c, v = current["ref"]
            ch = books.setdefault(b, {}).setdefault(c, {})
            ch[v] = ch.get(v, "") + text

    def walk(el):
        tag = el.tag.split("}")[-1]
        if tag == "verse":
            if el.get("sID") or (el.get("osisID") and not el.get("eID")):
                book, c, v = el.get("osisID").split(".")[:3]
                current["ref"] = (CODES[OSIS.index(book)], int(c), int(v)) if book in OSIS else None
            elif el.get("eID"):
                current["ref"] = None
        if tag in skip:
            return
        add(el.text)
        for child in el:
            walk(child)
            add(child.tail)

    walk(ET.parse(path).getroot())
    return books


def parse_usfm(files):
    books = {}
    for path in files:
        text = open(path, encoding="utf-8").read()
        code = re.search(r"\\id\s+(\w+)", text).group(1)
        if code not in CODES:
            continue
        chapters = books.setdefault(code, {})
        text = re.sub(r"\\f\s.*?\\f\*", "", text, flags=re.S)
        text = re.sub(r"\\x\s.*?\\x\*", "", text, flags=re.S)
        text = re.sub(r"\\va\s.*?\\va\*", "", text, flags=re.S)
        c = v = 0
        for line in text.splitlines():
            m = re.match(r"\\(\w+)\s*(.*)", line)
            if not m:
                if c and v:
                    chapters[c][v] += " " + line
                continue
            marker, rest = m.groups()
            if marker == "c":
                c, v = int(rest.split()[0]), 0
                chapters[c] = {}
            elif marker == "v":
                num, _, body = rest.partition(" ")
                v = int(re.match(r"\d+", num).group(0))
                chapters[c][v] = body
            elif marker in ("p", "q", "q1", "q2", "m", "pi", "li") and c and v:
                chapters[c][v] += " " + rest
            elif marker in ("d", "s", "s1", "qa", "mt", "cl", "toc1", "toc2", "toc3", "rem", "id", "ide", "h"):
                continue
        for ch in chapters.values():
            for key in ch:
                ch[key] = re.sub(r"\\\+?\w+\*?", "", ch[key])
    return books


def main():
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    jobs = [
        ("rv1909", parse_usfx, os.path.join(src, "spa-rv1909.usfx.xml"),
         {"name": "Reina Valera 1909", "abbrev": "RV1909", "language": "es", "license": "Dominio público"}, NAMES_ES),
        ("rv1865", parse_usfm, sorted(glob.glob(os.path.join(src, "rv1865", "*.usfm"))),
         {"name": "Reina Valera 1865", "abbrev": "RV1865", "language": "es",
          "license": "Dominio público (texto digital 2018 CC0, Ministerios Valera 1865)"}, NAMES_ES),
        ("bes", parse_usfx, os.path.join(src, "spa-bes.usfx.xml"),
         {"name": "Biblia en Español Sencillo", "abbrev": "BES", "language": "es", "license": "CC BY 4.0"}, NAMES_ES),
        ("pdt", parse_usfx, os.path.join(src, "spa-pddpt.usfx.xml"),
         {"name": "Palabra de Dios para ti", "abbrev": "PDT", "language": "es", "license": "CC BY-SA 4.0"}, NAMES_ES),
        ("vbl", parse_usfx, os.path.join(src, "spa-vbl.usfx.xml"),
         {"name": "Versión Biblia Libre", "abbrev": "VBL", "language": "es", "license": "CC BY-SA 4.0"}, NAMES_ES),
        ("kjv", parse_osis, os.path.join(src, "eng-kjv.osis.xml"),
         {"name": "King James Version", "abbrev": "KJV", "language": "en", "license": "Public Domain"}, NAMES_EN),
    ]
    for key, parser, source, meta, names in jobs:
        data = finish(parser(source), names, meta)
        with open(os.path.join(dst, key + ".json"), "w", encoding="utf-8") as fh:
            json.dump(data, fh, ensure_ascii=False, separators=(",", ":"))


if __name__ == "__main__":
    main()
