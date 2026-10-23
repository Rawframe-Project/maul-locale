// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Search's processed elements backward, the twin of icu4c_search_elements.cpp: reads
// conform_search_elements' input (a locale tag, a tab and a text's code
// points in hex), makes ICU's collator for each tag and writes the
// text's processed elements as UCollationPCE gives them, each as
// "element:low:high" in hex, offsets in UTF-16 units. Normalization is
// on, as the library always reads the NFD; the walk starts at the
// text's end and calls previousProcessed. UCollationPCE is internal
// (usrchimp.h) but exported.
//   g++ -O2 -I$ICU/include -I$ICU_SRC/common -I$ICU_SRC/i18n \
//       icu4c_search_elements_back.cpp -L$ICU/lib -licui18n -licuuc \
//       -licudata -Wl,-rpath,$ICU/lib -o icu_search_elements_back
#include "usrchimp.h"
#include "unicode/coll.h"
#include "unicode/ucoleitr.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
int main(){ static char line[1<<16];
 while(fgets(line,sizeof line,stdin)){
  char* tab=strchr(line,'\t'); if(!tab){ printf("error\n"); continue; } *tab=0;
  std::u16string text;
  for(char* p=strtok(tab+1," \n"); p; p=strtok(nullptr," \n")){
   unsigned long c=strtoul(p,nullptr,16);
   if(c>=0x10000){ text+=(char16_t)(0xD7C0+(c>>10)); text+=(char16_t)(0xDC00|(c&0x3FF)); }
   else text+=(char16_t)c; }
  UErrorCode e=U_ZERO_ERROR; icu::Locale loc=icu::Locale::forLanguageTag(line,e);
  icu::Collator* c=U_SUCCESS(e)?icu::Collator::createInstance(loc,e):nullptr;
  if(U_SUCCESS(e)) c->setAttribute(UCOL_NORMALIZATION_MODE,UCOL_ON,e);
  UCollationElements* el=U_SUCCESS(e)?ucol_openElements(c->toUCollator(),text.data(),(int32_t)text.size(),&e):nullptr;
  if(U_FAILURE(e)){ printf("error\n"); delete c; continue; }
  icu::UCollationPCE pce(el); const char* gap=""; ucol_setOffset(el,(int32_t)text.size(),&e);
  for(;;){ int32_t lo=0,hi=0; int64_t p=pce.previousProcessed(&lo,&hi,&e);
   if(p==UCOL_PROCESSED_NULLORDER||U_FAILURE(e)) break;
   printf("%s%llx:%x:%x",gap,(unsigned long long)p,lo,hi); gap=" "; }
  printf("\n"); ucol_closeElements(el); delete c; } }
