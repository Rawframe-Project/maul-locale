# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The kind of a row t262cal.py recorded, by its origin's top-level
# marker, for the emitters reading the rows of every group at once.

MARKERS = ("ixdtf", "str", "zdt", "dur", "inst", "time", "zone", "dt")


def kind(o):
    return next((m for m in MARKERS if m in o), "calendar")
