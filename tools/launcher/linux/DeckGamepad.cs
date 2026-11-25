#if LINUX
using System;
using System.IO;
using System.Runtime.InteropServices;
namespace TimeSplitters.Launcher
{
    // Read standard SDL mappings, not device-specific raw button numbers. No
    // video window, input grab, Steam configuration or game bindings are changed.
    internal sealed class DeckGamepad:IDisposable
    {
        IntPtr library,pad;bool sdl3,initialized;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int Init(uint flags);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate byte Init3(uint flags);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate void Quit(uint flags);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate void Update();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int Count();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate int IsPad(int index);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate IntPtr Open(uint id);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate IntPtr Pads(out int count);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate void Free(IntPtr data);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate void Close(IntPtr gamepad);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate byte Button(IntPtr gamepad,int button);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate short Axis(IntPtr gamepad,int axis);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate byte Connected(IntPtr gamepad);
        Update update;Count count;IsPad isPad;Open open;Pads pads;Free free;Close close;Button button;Axis axis;Connected connected;Quit quit;
        T Get<T>(string name) where T:Delegate=>Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(library,name));
        public DeckGamepad(string root)
        {
            foreach(string name in new[]{Path.Combine(root,"Bin/libSDL3.so.0"),"libSDL3.so.0","libSDL2-2.0.so.0"}){
                if(!NativeLibrary.TryLoad(name,out library))continue;
                try{
                    sdl3=NativeLibrary.TryGetExport(library,"SDL_GetGamepads",out _);
                    quit=Get<Quit>("SDL_QuitSubSystem");
                    initialized=sdl3?Get<Init3>("SDL_InitSubSystem")(0x2000)!=0:Get<Init>("SDL_InitSubSystem")(0x2000)==0;
                    if(!initialized)throw new InvalidOperationException("Controller subsystem unavailable");
                    update=Get<Update>(sdl3?"SDL_UpdateGamepads":"SDL_GameControllerUpdate");open=Get<Open>(sdl3?"SDL_OpenGamepad":"SDL_GameControllerOpen");close=Get<Close>(sdl3?"SDL_CloseGamepad":"SDL_GameControllerClose");button=Get<Button>(sdl3?"SDL_GetGamepadButton":"SDL_GameControllerGetButton");axis=Get<Axis>(sdl3?"SDL_GetGamepadAxis":"SDL_GameControllerGetAxis");connected=Get<Connected>(sdl3?"SDL_GamepadConnected":"SDL_GameControllerGetAttached");
                    if(sdl3){pads=Get<Pads>("SDL_GetGamepads");free=Get<Free>("SDL_free");}else{count=Get<Count>("SDL_NumJoysticks");isPad=Get<IsPad>("SDL_IsGameController");}return;
                }catch{Dispose();}
            }
        }
        public int Generation {get;private set;}
        public bool Available=>initialized;
        public bool Read(out int buttons,out int x,out int y,out int scrollY)
        {
            buttons=x=y=scrollY=0;if(library==IntPtr.Zero||!initialized)return false;update();
            if(pad!=IntPtr.Zero&&connected(pad)==0){close(pad);pad=IntPtr.Zero;}
            if(pad==IntPtr.Zero){
                if(sdl3){int total;var ids=pads(out total);try{if(total>0&&ids!=IntPtr.Zero){pad=open((uint)Marshal.ReadInt32(ids));if(pad!=IntPtr.Zero)Generation++;}}finally{if(ids!=IntPtr.Zero)free(ids);}}
                else for(int i=0;i<count();i++)if(isPad(i)!=0){pad=open((uint)i);if(pad!=IntPtr.Zero){Generation++;break;}}
            }
            if(pad==IntPtr.Zero)return false;for(int i=0;i<=14;i++)if(button(pad,i)!=0)buttons|=1<<i;x=axis(pad,0);y=axis(pad,1);scrollY=axis(pad,3);return true;
        }
        public void Dispose(){if(pad!=IntPtr.Zero){close?.Invoke(pad);pad=IntPtr.Zero;}if(initialized){quit?.Invoke(0x2000);initialized=false;}if(library!=IntPtr.Zero){NativeLibrary.Free(library);library=IntPtr.Zero;}}
    }
}
#endif
