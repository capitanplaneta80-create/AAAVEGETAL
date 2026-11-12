#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mauricio Vásquez Arias, Universidad EAFIT
"""Tablas de VEGETAL (eng_plant.c): espectros en 1/16 de semitono, normalización y anillos.
  python3 tools/gen_plant_tables.py pl_tables.h"""
import math, sys
out = sys.argv[1] if len(sys.argv) > 1 else "pl_tables.h"
L = ["/* generado por tools/gen_plant_tables.py: no editar */", "#ifndef PL_TABLES_H", "#define PL_TABLES_H"]
st16 = lambda r: int(round(16 * 12 * math.log2(r)))
phi = (1 + 5 ** 0.5) / 2
def fold(r, lo=1.0, hi=4.0):
    while r >= hi: r /= 4.0 if r >= hi * 2 else 2.0
    while r < lo: r *= 2.0
    return r
aure = [phi ** (k / 3) for k in range(16)]                     # temperamento áureo: 16 pasos de phi^(1/3) (278 cents)
fib = [1, 1, 2, 3, 5, 8, 13, 21, 34, 55]
fibo = sorted(set([1, 2, 3, 5, 8, 13, 21, 34, 55] + [4, 7, 11, 18, 29, 47, 76]))   # armónicos de Fibonacci y de Lucas
arm = list(range(1, 17))
pent = [2 ** ((12 * o + s) / 12) for o in range(4) for s in (0, 2, 4, 7, 9)][:16]
spec = {"AURE": aure, "FIBO": fibo, "ARM": arm, "PENT": pent}
L.append("#define PL_NSPEC %d" % len(spec))
L.append("static const char *const PL_SPEC_NAMES[] = {" + ", ".join('"%s"' % k for k in spec) + "};")
L.append("static const int16_t PL_SPEC[%d][16] = {" % len(spec))
for k, v in spec.items():
    assert len(v) == 16, (k, len(v))
    L.append("    {" + ", ".join(str(st16(r)) for r in sorted(v)) + "},   /* %s */" % k)
L.append("};")
L.append("static const int16_t PL_NORM[65] = {" + ", ".join(str(32767 if n == 0 else int(round(32767 / math.sqrt(n)))) for n in range(65)) + "};")
L.append("#endif")
open(out, "w").write("\n".join(L) + "\n")
print("escrito", out, {k: [round(x, 3) for x in v] for k, v in spec.items()})
