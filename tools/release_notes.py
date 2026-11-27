#!/usr/bin/env python3
"""Print the CHANGELOG section of a release, for its GitHub release notes.

Usage: release_notes.py CHANGELOG.md v1.02-2.46[-beta1]

A beta gets the section of the release it leads to, which collects the
changes while that release is being prepared.
"""
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")

text = open(sys.argv[1], encoding="utf-8").read()
version = re.sub(r"-beta\d+$", "", sys.argv[2].lstrip("v"))
match = re.search(rf"(^## {re.escape(version)} .*?)(?=^## \d|\Z)", text, re.MULTILINE | re.DOTALL)
print(match.group(1).strip() if match else "")
