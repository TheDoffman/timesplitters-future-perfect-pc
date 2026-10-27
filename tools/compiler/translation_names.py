"""Deterministic guest C names independent of bundled-toolkit name data."""
from pathlib import Path
import re
from generation_output import C_KEYWORDS


# Factual API identifiers regenerated directly from the Windows SDK. Use the
# same inventory on every host so declarations and call sites cannot diverge.
HOST_EXPORTS=frozenset(Path(__file__).with_name('translation_host_symbols.txt').read_text(encoding='ascii').splitlines())
_STRING=frozenset('memchr memcmp memcpy memmove memset strcat strchr strcmp strcoll strcpy strcspn strerror strlen strncat strncmp strncpy strpbrk strrchr strspn strstr strtok strxfrm strdup strnlen'.split())
_MATH=frozenset('acos asin atan atan2 ceil cos cosh exp fabs floor fmod frexp ldexp log log10 modf pow sin sinh sqrt tan tanh acosh asinh atanh cbrt copysign erf erfc exp2 expm1 fdim fma fmax fmin hypot ilogb lgamma llrint llround log1p log2 logb lrint lround nan nearbyint nextafter nexttoward remainder remquo rint round scalbln scalbn tgamma trunc fpclassify isfinite isgreater isgreaterequal isinf isless islessequal islessgreater isnan isnormal isunordered signbit'.split())
_STDLIB=frozenset('abort abs atexit atof atoi atol bsearch calloc div exit free getenv labs ldiv malloc mblen mbstowcs mbtowc onexit qsort rand realloc srand strtod strtol strtoul system wctomb wcstombs atoll llabs lldiv strtoll strtoull setjmp longjmp'.split())
RESERVED=C_KEYWORDS | HOST_EXPORTS | _STRING | _MATH | _STDLIB | {'asm'}


def identifier(address, name):
    base=name or f'sub_{address:08X}'
    safe=re.sub('[^A-Za-z_0-9]','_',base)
    if not safe or safe[0].isdigit():safe='guest_'+safe
    if safe!=base or safe in RESERVED:safe+=f'_{address:08X}'
    return safe


def call_target(context, address):
    metadata=context.func_db.get(address)
    chosen=(metadata.get('call_name') or metadata.get('name')) if metadata is not None else context.label_db.get(address)
    result=identifier(address,chosen)
    context.referenced_calls[address]=result
    return result


def call_arguments(context, address):
    metadata=context.abi_db.get(address,{})
    count=metadata.get('estimated_params',0)
    prefix=['(void*)(uintptr_t)ecx'] if metadata.get('calling_convention') in ('thiscall','thiscall_cdecl') else []
    return ', '.join(prefix+[f'0 /* a{index} */' for index in range(1,count+1)])
