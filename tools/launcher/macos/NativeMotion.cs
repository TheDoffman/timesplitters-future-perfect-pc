using System;
using System.Runtime.InteropServices;
namespace TimeSplitters.Launcher {
public static partial class NativeMotion {
#if MACOS
    [DllImport("/usr/lib/libobjc.A.dylib")]static extern IntPtr objc_getClass(string name);
    [DllImport("/usr/lib/libobjc.A.dylib")]static extern IntPtr sel_registerName(string name);
    [DllImport("/usr/lib/libobjc.A.dylib",EntryPoint="objc_msgSend")]static extern IntPtr Send(IntPtr receiver,IntPtr selector);
    [DllImport("/usr/lib/libobjc.A.dylib",EntryPoint="objc_msgSend")][return:MarshalAs(UnmanagedType.I1)]static extern bool Flag(IntPtr receiver,IntPtr selector);
#endif
    public static bool ReduceMotion(){
#if MACOS
        var workspace=Send(objc_getClass("NSWorkspace"),sel_registerName("sharedWorkspace"));
        return workspace!=IntPtr.Zero&&Flag(workspace,sel_registerName("accessibilityDisplayShouldReduceMotion"));
#else
        return false;
#endif
    }
}
}
