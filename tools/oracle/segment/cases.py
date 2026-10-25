#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the segmentation oracle's cases, "locale<TAB>kind<TAB>text" (a
# line feed written \n), from CLDR's common/segments:
# - each suppression of each locale in a sentence before a capital, with
#   -u-ss-standard and without, and each partial's text through its first
#   full stop alone;
# - a break Maul Unicode holds back past a segmenter's kept bytes, after a
#   match, a partial and no entry;
# - the class overrides (Greek's semicolons, POSIX English's full stop);
# - the line keywords over CJ, quotation marks among Chinese and among
#   Latin letters, the wave dash and Latin text, for the root and the
#   locales that tailor lines;
# - Japanese words over Hiragana, ideographs and the marks CLDR adds to
#   them, and plain text in every locale and kind.
#
#   python3 tools/oracle/segment/cases.py CLDR_COMMON > cases.txt

import os
import sys
import xml.etree.ElementTree as ET


def tag(name):
    # CLDR's file name as a BCP 47 tag the library and ICU4C both read.
    return name.replace("_POSIX", "-posix").replace("_", "-")


def suppressions(path):
    root = ET.parse(path).getroot()
    return sorted({e.text for e in root.iter("suppression")})


def partial(text):
    stop = text.find(".")
    return stop >= 0 and stop + 1 < len(text)


NUMBERS = "(" + ", ".join(str(i) for i in range(1, 26)) + ")"
KANA = "あぁあーい"
QUOTES = "中“文”中。彼は“はい”と言った。"
# U+201C and U+201D between Latin letters, where QU and OP or CL differ.
LATIN_QUOTES = "say “abc”def and “x” y"
WAVE = "中〜中゠中"
LATIN = "Hello, world (a test) - done; ok? Yes!"
PLAIN = {
    "word": "The quick (\"brown\") fox's 3.14 jumps, o'clock. 12,345.67 e-mail",
    "sentence": "It is 3.5 p.m. now. Mr. Jones said \"Go!\" She left... Then 'yes.' Done",
    "line": "A line with spaces, hyphen-ated words, (parens) and 12.34%.\nNext line.",
}


def cases(common):
    out = []
    segments = os.path.join(common, "segments")
    names = sorted(f[:-4] for f in os.listdir(segments) if f.endswith(".xml"))
    for name in names:
        locale = tag(name)
        for text in suppressions(os.path.join(segments, name + ".xml")):
            sentence = f"Ab {text} Cd ef. Gh."
            out.append((locale + "-u-ss-standard", "sentence", sentence))
            out.append((locale, "sentence", sentence))
            if partial(text):
                prefix = text[: text.find(".") + 1]
                out.append((locale + "-u-ss-standard", "sentence", f"Ab {prefix} Cd ef."))
        for kind, text in PLAIN.items():
            out.append((locale, kind, text))
    for locale, word in (("en", "Mr."), ("en", "Xy."), ("de", "z."), ("de", "z.B.")):
        out.append((locale + "-u-ss-standard", "sentence", f"Ab {word} {NUMBERS} Cd ef. Gh."))
    out.append(("el", "sentence", "Ναι; Όχι; Τέλος; Αρχή. Ναι"))
    out.append(("en-US-posix", "word", "3.14 a.b c.d. e.f"))
    # POSIX English as BCP 47 writes it, and its sentences from English.
    out.append(("en-US-u-va-posix", "word", "3.14 a.b c.d. e.f"))
    out.append(("en-US-u-ss-standard-va-posix", "sentence", "Ab Mr. Cd ef. Gh."))
    out.append(("en-US", "word", "3.14 a.b c.d. e.f"))
    for locale in ("und", "en", "ja", "zh", "zh-Hant", "ko", "de"):
        for key in ("", "-u-lb-strict", "-u-lb-normal", "-u-lb-loose"):
            for text in (KANA, QUOTES, LATIN_QUOTES, WAVE, LATIN):
                out.append((locale + key, "line", text))
    for text in ("ひらがなとかんじ", "これはひらがなです", "ゝゞのほか゛とゕゖ", "あ゙い",
                 "漢字仮名交じり文", "人々は日本語を話す", "〇〇年に〻", "⺀⺁漢\U00017000\U0001B170"):
        out.append(("ja", "word", text))
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: cases.py CLDR_COMMON")
    seen = set()
    for locale, kind, text in cases(sys.argv[1]):
        if (locale, kind, text) not in seen:
            seen.add((locale, kind, text))
            print(f"{locale}\t{kind}\t{text.replace(chr(10), chr(92) + 'n')}")


if __name__ == "__main__":
    main()
