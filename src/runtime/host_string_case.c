/* Original width/locale adapter to permitted standard character libraries. */
#include "host_string_case.h"
#include <ctype.h>
#include <wctype.h>
uint32_t tsfp_host_string_case(uint32_t value,unsigned width,int upper) {
    if(width!=1 && width!=2)return value;
    value&=width==1?UINT32_C(255):UINT32_C(65535);
#ifdef TSFP_TEST_ASCII_CASE
    if(upper && value>='a' && value<='z')return value-32;
    if(!upper && value>='A' && value<='Z')return value+32;
    return value;
#else
    if(width==1)return (uint32_t)(upper?toupper((unsigned char)value):tolower((unsigned char)value))&255;
    return (uint32_t)(upper?towupper((wint_t)value):towlower((wint_t)value))&65535;
#endif
}
