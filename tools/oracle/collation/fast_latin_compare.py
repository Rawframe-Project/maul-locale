# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# conform_collation_fast against icu4c_collation_fast_latin.cpp's answers
#, table by table, by what the compare reads rather
# than by bytes: each fast character decoded to its result and its
# contraction entries. A list holding only its default is not the plain
# character: it makes the compare read the next character and bail out
# on one outside the table. Only tags
# whose locale and type ICU's loader took itself count. Prints the
# counts (a table differing only by the library's holding such a list
# where ICU's has the character counts apart: it bails out more), then
# each differing tag with its first differing characters.
#   python3 -I fast_latin_compare.py icu.txt ours.txt
import re,sys
def words(fields):
    return [int(x,16) for x in fields[1:]] if fields and fields[0]!='error' else None
icu={}; actual={}
for line in open(sys.argv[1]):
    tag,rest=line.rstrip('\n').split('\t'); f=rest.split(' ')
    actual[tag]=f[0]; icu[tag]=words(f[1:])
ours={l.split()[0]:words(l.split()[1:]) for l in open(sys.argv[2])}
def verified(tag):
    m=re.match(r'(.*?)(?:-u-co-([a-z0-9-]+))?$',tag); base,co=m.group(1),m.group(2)
    a=actual.get(tag,'-'); loc,_,kw=a.partition('@')
    want='root' if base=='und' else base.replace('-','_')
    long={'phonebk':'phonebook','trad':'traditional','dict':'dictionary'}
    if co and co!='standard' and kw!='collation='+long.get(co,co): return False
    return loc==want or (loc=='' and want=='root')
def result(w,at,length):
    return ('bail',) if length==1 else ('one',w[at]) if length==2 else ('two',w[at],w[at+1])
def decode(w):
    if w is None or len(w)<453: return [len(w or [])]
    out=[tuple(w[:5])]
    for c in range(448):
        word=w[5+c]
        if word>=0xC00 or word<0x400: out.append(('bail',) if word==1 else ('one',word)); continue
        at=453+(word&0x3FF)
        if word>=0x800: out.append(('two',w[at],w[at+1])); continue
        length=w[at]>>9; default=result(w,at+1,length); at+=length; entries=[]
        while w[at]&0x1FF!=0x1FF:
            length=w[at]>>9; entries.append((w[at]&0x1FF,result(w,at+1,length))); at+=length
        out.append(('list',default,tuple(entries)))
    return out
same=0; unverified=0; more=0; bad=[]
for tag,w in ours.items():
    if tag not in icu or not verified(tag): unverified+=1; continue
    a=decode(icu[tag]); b=decode(w)
    diff=[i for i in range(min(len(a),len(b))) if a[i]!=b[i]]+([0] if len(a)!=len(b) else [])
    if not diff: same+=1
    elif all(i>0 and b[i]==('list',a[i],()) for i in diff): more+=1
    else: bad.append((tag,[i-1 for i in diff][:4]))
print('same',same,'unverified',unverified,'bails out more',more,'different',len(bad))
for tag,chars in bad: print(tag,' '.join('U+%04X'%(c if c<0x180 else c-0x180+0x2000) for c in chars))
