// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation search against ICU4C's usearch: reads
// conform_search's input (a locale tag, a pattern's and a text's code
// points in hex, a position in UTF-16 units and a comparison, tab-
// separated, and a sixth field 1 for a backward search), makes ICU's
// collator for each tag with normalization on (the library always reads
// the NFD), opens a string search on it with the default (character)
// break iterator and the comparison, and writes usearch_search's match
// from the position (usearch_searchBackwards' with the sixth field): its
// start and end, "none", or "error" for a failure. Each line runs in a child process: ICU's
// CEIBuffer dereferences a null element when a match's window outgrows
// it (UPRV_UNREACHABLE_ASSERT), which crashes, and some identical-
// strength searches never end; both read as "error" (a child gets 10
// seconds).
//   g++ -O2 -I$ICU/include icu4c_search.cpp -L$ICU/lib -licui18n \
//       -licuuc -licudata -Wl,-rpath,$ICU/lib -o icu_search
#include "unicode/coll.h"
#include "unicode/usearch.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
U_CAPI UBool U_EXPORT2 usearch_search(UStringSearch*, int32_t, int32_t*, int32_t*, UErrorCode*);
U_CAPI UBool U_EXPORT2 usearch_searchBackwards(UStringSearch*, int32_t, int32_t*, int32_t*,
                                               UErrorCode*);
static std::u16string Units(char* hex){ std::u16string s;
 for(char* p=strtok(hex," "); p; p=strtok(nullptr," ")){ unsigned long c=strtoul(p,nullptr,16);
  if(c>=0x10000){ s+=(char16_t)(0xD7C0+(c>>10)); s+=(char16_t)(0xDC00|(c&0x3FF)); } else s+=(char16_t)c; }
 return s; }
int main(){ static char line[1<<16]; setvbuf(stdout,nullptr,_IOLBF,0);
 while(fgets(line,sizeof line,stdin)){ line[strcspn(line,"\n")]=0;
  char* f[6]={line}; int n=1;
  for(char* p=line; *p && n<6; p++) if(*p=='\t'){ *p=0; f[n++]=p+1; }
  if(n<5){ printf("error\n"); continue; }
  fflush(stdout);
  pid_t child=fork();
  if(child==0) alarm(10);
  if(child>0){ int status=0; waitpid(child,&status,0); if(!WIFEXITED(status)) printf("error\n"); continue; }
  std::u16string pat=Units(f[1]); std::u16string text=Units(f[2]);
  int32_t from=atoi(f[3]); int type=atoi(f[4]); bool back=n==6 && atoi(f[5])==1;
  UErrorCode e=U_ZERO_ERROR; icu::Locale loc=icu::Locale::forLanguageTag(f[0],e);
  icu::Collator* c=U_SUCCESS(e)?icu::Collator::createInstance(loc,e):nullptr;
  if(U_SUCCESS(e)) c->setAttribute(UCOL_NORMALIZATION_MODE,UCOL_ON,e);
  UStringSearch* s=U_SUCCESS(e)?usearch_openFromCollator(pat.data(),(int32_t)pat.size(),
     text.data(),(int32_t)text.size(),c->toUCollator(),nullptr,&e):nullptr;
  static const USearchAttributeValue types[]={USEARCH_STANDARD_ELEMENT_COMPARISON,
     USEARCH_PATTERN_BASE_WEIGHT_IS_WILDCARD,USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD};
  if(U_SUCCESS(e)) usearch_setAttribute(s,USEARCH_ELEMENT_COMPARISON,types[type],&e);
  int32_t ms=-1,ml=-1;
  UBool found=!U_SUCCESS(e)?false:back?usearch_searchBackwards(s,from,&ms,&ml,&e)
     :usearch_search(s,from,&ms,&ml,&e);
  if(U_FAILURE(e)) printf("error\n"); else if(!found) printf("none\n"); else printf("%d %d\n",ms,ml);
  if(s) usearch_close(s); delete c; fflush(stdout); _exit(0); } }
