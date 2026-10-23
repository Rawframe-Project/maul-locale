// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root elements table the tailoring builder reads
// against ICU4C's. Alone it writes the table of ICU's root collation
// data, one 32-bit unit per line in hex, the five index units first, as
// conform_collation_root_elements --dump writes the generator's. With
// --queries it reads conform_collation_root_elements's queries from
// stdin and answers them through ICU's CollationRootElements over that
// table, in the same form.
// CollationRoot and CollationRootElements are exported by ICU4C's i18n
// library; their headers are ICU's internal ones from the same
// release's sources:
//   g++ -O2 -I$ICU_SRC/i18n -I$ICU_SRC/common -I$ICU/include \
//       -DU_SHOW_CPLUSPLUS_API=1 icu4c_collation_root_elements.cpp \
//       -L$ICU/lib -licui18n -licuuc -Wl,-rpath,$ICU/lib -o icu_root_elements
// Queries: python3 -I root_elements_queries.py TABLE FractionalUCA.txt.
#include "collationdata.h"
#include "collationroot.h"
#include "collationrootelements.h"
#include <cstdio>
#include <cstring>
using icu::CollationRootElements;
int main(int argc,char**argv){ UErrorCode e=U_ZERO_ERROR; const icu::CollationData* d=icu::CollationRoot::getData(e);
 if(U_FAILURE(e)||!d){ fprintf(stderr,"%s\n",u_errorName(e)); return 1; }
 if(argc<2||strcmp(argv[1],"--queries")!=0){ for(int32_t i=0;i<d->rootElementsLength;i++) printf("%08x\n",(unsigned)d->rootElements[i]); return 0; }
 CollationRootElements r(d->rootElements,d->rootElementsLength); char line[256], op[3];
 while(fgets(line,sizeof line,stdin)){ unsigned a=0,b=0,c=0; int n=sscanf(line,"%2s %x %x %x",op,&a,&b,&c);
  int32_t ix=a==0?0:r.findPrimary(a);
  if(n==2&&!strcmp(op,"lb")) printf("%016llx\n",(unsigned long long)r.lastCEWithPrimaryBefore(a));
  else if(n==2&&!strcmp(op,"fa")) printf("%016llx\n",(unsigned long long)r.firstCEWithPrimaryAtLeast(a));
  else if(n==3&&!strcmp(op,"pb")) printf("%08x\n",r.getPrimaryBefore(a,b!=0));
  else if(n==3&&!strcmp(op,"pa")) printf("%08x\n",r.getPrimaryAfter(a,ix,b!=0));
  else if(n==3&&!strcmp(op,"sb")) printf("%04x\n",r.getSecondaryBefore(a,b));
  else if(n==3&&!strcmp(op,"sa")) printf("%04x\n",r.getSecondaryAfter(ix,b));
  else if(n==4&&!strcmp(op,"tb")) printf("%04x\n",r.getTertiaryBefore(a,b,c));
  else if(n==4&&!strcmp(op,"ta")) printf("%04x\n",r.getTertiaryAfter(ix,b,c));
  else { fprintf(stderr,"a query not understood: %s",line); return 1; } } }
