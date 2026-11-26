#!/usr/bin/env python3
"""Embed ats-mini/web/remote.html into ats-mini/PageRemote.h.

Run after editing the page: python tools/embed_web.py
"""
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(ROOT, "ats-mini", "web", "remote.html")
DST = os.path.join(ROOT, "ats-mini", "PageRemote.h")

with open(SRC, encoding="utf-8") as f:
    page = f.read()

if ')HTML"' in page:
    raise SystemExit("remote.html must not contain the raw string terminator")

with open(DST, "w", encoding="utf-8", newline="\n") as f:
    f.write("#ifndef PAGE_REMOTE_H\n#define PAGE_REMOTE_H\n\n")
    f.write("// Generated from web/remote.html by tools/embed_web.py, do not edit.\n")
    f.write("// The same page is served by the PC companion, which adds the audio.\n")
    f.write('static const char pageRemote[] PROGMEM = R"HTML(')
    f.write(page)
    f.write(')HTML";\n\n#endif // PAGE_REMOTE_H\n')

print(f"{DST}: {len(page)} bytes")
