/* Native Microsoft x86 ABI evidence, not a reproduction of the guest decoder. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
_Static_assert(sizeof(void *)==4 && sizeof(double)==8,"32-bit x86 probe");
__declspec(noinline) static int probe(unsigned marker,...) {
    va_list arguments;size_t written;(void)marker;va_start(arguments,marker);
    written=fwrite(arguments,1,24,stdout);va_end(arguments);return written==24;
}
int main(void) {
    if(_setmode(_fileno(stdout),_O_BINARY)==-1)return 1;
    return probe(0,0x11223344u,UINT64_C(0x8877665544332211),3.25,0xa1b2c3d4u)?0:1;
}
