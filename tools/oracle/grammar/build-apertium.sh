#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Builds an Apertium language's generator (analysis to surface form) as
# its Makefile does, for the grammar oracle. GPL data and tools,
# run here only; nothing of them is shipped.
# usage: build-apertium.sh LANG (tur, aze, kaz, kir, uzb, tuk, hun, fin)
# needs: git, hfst (Ubuntu's 3.16: hfst-twolc, hfst-lexc,
# hfst-compose-intersect, hfst-fst2fst, hfst-lookup)
set -e
L=$1
ROOT=${ROOT:-$PWD/apertium}
mkdir -p "$ROOT" && cd "$ROOT"
[ -d apertium-$L ] || git clone -q --depth 1 https://github.com/apertium/apertium-$L.git
mkdir -p build-$L && cd build-$L
A=../apertium-$L
grep -v 'Dir/LR' $A/apertium-$L.$L.lexc > $L.RL.lexc
hfst-twolc -q $A/apertium-$L.$L.twol -o twol.hfst
hfst-lexc -q $L.RL.lexc -o RL.lexc.hfst
hfst-compose-intersect -1 RL.lexc.hfst -2 twol.hfst -o RL.hfst
hfst-fst2fst -O RL.hfst -o $L.gen.hfst
echo "$ROOT/build-$L/$L.gen.hfst"
# Then: printf 'Ayşe<np><ant><f><dat>\n' | hfst-lookup -q $L.gen.hfst
