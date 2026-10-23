# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# conform_collation_keys --keys against icu4c_collation_keys.cpp's
# answers: only tags whose locale and type ICU's loader
# took itself count, and the Han probes (Han, and characters the root
# maps to a Han primary) are left out but
# for zh's tailorings, which map them (the root orders Han by code point,
# the collation design), and kept in the unihan types, which order Han by
# radical and stroke as ICU's root does. Given the builder's --queries too, a zh string of
# Han and CJK radicals its type's alt="short" rules do not name is left
# out: the root orders it. Prints the differing tags with two examples
# each.
#   python3 -I keys_compare.py icu.txt ours.txt queries.txt shown [builder.txt]
import sys,collections,re
a=open(sys.argv[1]).read().split('\n'); b=open(sys.argv[2]).read().split('\n'); q=open(sys.argv[3]).read().split('\n')
assert len(a)==len(b)
long={'phonebk':'phonebook','trad':'traditional','dict':'dictionary'}
defaults={'zh':'pinyin','zh_Hant':'stroke'}
def verified(tag,actual):
    m=re.match(r'(.*?)(?:-u-co-([a-z0-9-]+?))?(?:-k[a-z].*)?$',tag)
    base,co=m.group(1),m.group(2)
    f='root' if base=='und' else base.replace('-','_')
    ab,_,kw=actual.partition('@')
    ab=ab or 'root'
    t=kw.split('=')[1] if kw else defaults.get(ab,'standard')
    if ab!=f: return False
    return co is None or long.get(co,co)==t
han=[(0x2E80,0x2FDF),(0x3400,0x4DBF),(0x4E00,0x9FFF),(0xF900,0xFAFF),(0x20000,0x33479)]
short=collections.defaultdict(set)
if len(sys.argv)>5:
    cur=None
    for l in open(sys.argv[5]):
        if l.startswith('type '): cur=l.split()[1:]; continue
        if l.startswith('q ') and cur and cur[0]=='zh' and cur[2:]==['short']:
            short[cur[1]].update(int(h,16) for h in l.split()[1:])
def rootHan(tag,qline):
    if not short or not tag.startswith('zh'): return False
    t=next((n for n in ('stroke','zhuyin','unihan') if n in tag),'pinyin')
    cps=[int(h,16) for h in qline.split()[1:]]
    return all(any(lo<=c<=hi for lo,hi in han) and c not in short[t] for c in cps)
rootHanSkipped=0
HAN_PROBES={'q 4E00','q 963F','q 4E28','q 2A6D9','q 20000','q 9F52','q 5B57','q 3400',
            'q 9FA5','q 2F00','q 2E80','q 3220'}
def hasHan(qline):
    return qline.strip() in HAN_PROBES
hanSkipped=0
tag=None; ok=False; per=collections.Counter(); tot=collections.Counter(); ex={}; skipped=collections.Counter()
for i,(x,y) in enumerate(zip(a,b)):
    if x.startswith('tag '):
        tag,_,actual=x[4:].partition('\t'); assert y=='tag '+tag,(x,y); ok=verified(tag,actual); continue
    if not x: continue
    if not ok: skipped[tag]+=1; continue
    unihan='-co-unihan' in tag
    if hasHan(q[i]) and re.split('[-_]',tag)[0] not in ('zh','yue') and not unihan:
        hanSkipped+=1; continue
    if rootHan(tag,q[i]) and not unihan:
        rootHanSkipped+=1; continue
    tot[tag]+=1
    if x!=y:
        per[tag]+=1; ex.setdefault(tag,[]).append((q[i],x,y))
for t,c in sorted(per.items(),key=lambda kv:-kv[1])[:int(sys.argv[4]) if len(sys.argv)>4 else 60]:
    print(t,c,'of',tot[t])
    for e in ex[t][:2]: print('   ',*e)
print('verified tags',len(tot),'keys',sum(tot.values()),'differ',sum(per.values()),'in',len(per),'tags; unverified tags',len(skipped),'keys',sum(skipped.values()))
print('han probes left out',hanSkipped,'; zh strings the root orders left out',rootHanSkipped)
print('unverified:',' '.join(sorted(skipped))[:3000])
