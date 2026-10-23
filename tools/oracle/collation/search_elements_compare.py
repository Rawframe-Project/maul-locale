# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

import sys
d=sys.argv[1]
a=open(d+'/in.txt').read().splitlines(); b=open(d+'/icu.txt').read().splitlines(); c=open(d+'/ours.txt').read().splitlines()
# Tags ICU 79 does not ship (root fallback) and de's provisional eor.
skip=tuple(l.split('-u-')[0]+'-u-' for l in open(d+'/rootfall.txt').read().split())+('de-u-co-eor',)
bad=[i for i in range(len(a)) if b[i]!=c[i] and not a[i].startswith(skip)]
print(len(a),'differ',len(bad))
for i in bad[:10]: print(a[i]); print(' icu ',b[i]); print(' ours',c[i])
