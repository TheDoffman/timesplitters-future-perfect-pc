__attribute__((visibility("default"),noinline)) void tsfp_native_library_fault(void){
#ifdef __APPLE__
    __asm__ volatile("brk #0");
#else
    __asm__ volatile("ud2");
#endif
}
