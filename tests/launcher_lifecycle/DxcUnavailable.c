/* Nonshipped acceptance fixture: a present DXC library that cannot initialize.
 * Installed only in the disposable game sandbox, never a build/package input. */
__declspec(dllexport) long __stdcall DxcCreateInstance(const void *class_id,const void *interface_id,void **object)
{
    (void)class_id;(void)interface_id;
    if(object)*object=0;
    return (long)0x80004005u;
}
