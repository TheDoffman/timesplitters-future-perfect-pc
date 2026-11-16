using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace TimeSplitters.Launcher
{
    internal interface IPreviewPcmDevice : IDisposable
    {
        void Open(byte[] pcm,int rate,int channels);
        bool Advance();
    }
    // One explicitly requested, already validated PCM window. No container or decoder.
    internal sealed class ModPreviewAudio
    {
        static readonly SemaphoreSlim gate=new SemaphoreSlim(1,1);
        readonly Func<IPreviewPcmDevice> create;
        internal ModPreviewAudio():this(NativePreviewPcm.Create){}
        internal ModPreviewAudio(Func<IPreviewPcmDevice> create){this.create=create;}
        internal async Task Play(ModeratorAudioPreview clip,CancellationToken cancel)
        {
            cancel.ThrowIfCancellationRequested();byte[] pcm=clip.PlaybackPcm();
            await PlayPcm(pcm,clip.Rate,clip.Channels,cancel).ConfigureAwait(false);
        }
        internal async Task PlayPcm(byte[] pcm,int rate,int channels,CancellationToken cancel)
        {
            cancel.ThrowIfCancellationRequested();
            Core.Require(pcm!=null && rate>=8000 && rate<=48000 && (channels==1 || channels==2) && pcm.Length>0 && pcm.Length%(channels*2)==0 && pcm.Length<=rate*channels*4,"Invalid bounded preview PCM.");
            // Own a copy and attenuate by12dB without changing the system mixer.
            var owned=new byte[pcm.Length];
            for(int i=0;i<pcm.Length;i+=2){int sample=(short)(pcm[i]|pcm[i+1]<<8)/4;owned[i]=(byte)sample;owned[i+1]=(byte)(sample>>8);}
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                await Task.Run(()=>{
                    cancel.ThrowIfCancellationRequested();
                    using(var device=create()) {
                        device.Open(owned,rate,channels);var clock=Stopwatch.StartNew();
                        while(true) {
                            cancel.ThrowIfCancellationRequested();
                            Core.Require(clock.ElapsedMilliseconds<5000,"The audio device did not finish the preview. Playback was stopped.");
                            if(device.Advance())return;
                            cancel.WaitHandle.WaitOne(10);
                        }
                    }
                },cancel).ConfigureAwait(false);
            }catch(DllNotFoundException){throw new InvalidOperationException("The system PCM audio service is unavailable. Linux needs PulseAudio/PipeWire or a configured ALSA output.");}
            finally{gate.Release();}
        }
    }
    internal static class NativePreviewPcm
    {
        static int unreleased;
        internal static IPreviewPcmDevice Create()
        {
            Core.Require(Volatile.Read(ref unreleased)==0,"An audio device could not be released. Restart the launcher before listening again.");
            return new Device();
        }
        static void Check(long code){Core.Require(code==0,"The system audio output could not play this PCM preview.");}
        static void ReleaseFailed(){Interlocked.Exchange(ref unreleased,1);throw new InvalidOperationException("The system audio output did not release its preview resources. Restart the launcher before listening again.");}
#if MACOS
        const string Audio="/System/Library/Frameworks/AudioToolbox.framework/AudioToolbox";
        [StructLayout(LayoutKind.Sequential)]struct Format {public double Rate;public uint Id,Flags,PacketBytes,PacketFrames,FrameBytes,Channels,Bits,Reserved;}
        [StructLayout(LayoutKind.Sequential)]struct Buffer {public uint Capacity;public IntPtr Data;public uint Size;}
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate void OutputCallback(IntPtr user,IntPtr queue,IntPtr buffer);
        static readonly OutputCallback returned=(user,queue,buffer)=>Marshal.WriteInt32(user,1);
        [DllImport(Audio)]static extern int AudioQueueNewOutput(ref Format format,OutputCallback callback,IntPtr user,IntPtr loop,IntPtr mode,uint flags,out IntPtr queue);
        [DllImport(Audio)]static extern int AudioQueueAllocateBuffer(IntPtr queue,uint bytes,out IntPtr buffer);
        [DllImport(Audio)]static extern int AudioQueueEnqueueBuffer(IntPtr queue,IntPtr buffer,uint descriptions,IntPtr packets);
        [DllImport(Audio)]static extern int AudioQueueStart(IntPtr queue,IntPtr start);
        [DllImport(Audio)]static extern int AudioQueueStop(IntPtr queue,[MarshalAs(UnmanagedType.U1)]bool immediate);
        [DllImport(Audio)]static extern int AudioQueueDispose(IntPtr queue,[MarshalAs(UnmanagedType.U1)]bool immediate);
        [DllImport(Audio)]static extern int AudioQueueGetProperty(IntPtr queue,uint property,out uint value,ref uint size);
        sealed class Device : IPreviewPcmDevice
        {
            IntPtr queue,flag;bool draining;
            public void Open(byte[] pcm,int rate,int channels)
            {
                flag=Marshal.AllocHGlobal(4);Marshal.WriteInt32(flag,0);
                var format=new Format {Rate=rate,Id=0x6c70636d,Flags=12,PacketBytes=(uint)channels*2,PacketFrames=1,FrameBytes=(uint)channels*2,Channels=(uint)channels,Bits=16};
                Check(AudioQueueNewOutput(ref format,returned,flag,IntPtr.Zero,IntPtr.Zero,0,out queue));
                IntPtr buffer;Check(AudioQueueAllocateBuffer(queue,(uint)pcm.Length,out buffer));
                var view=(Buffer)Marshal.PtrToStructure(buffer,typeof(Buffer));Core.Require(view.Capacity>=pcm.Length && view.Data!=IntPtr.Zero,"Invalid system audio buffer.");
                Marshal.Copy(pcm,0,view.Data,pcm.Length);Marshal.WriteInt32(buffer,(int)Marshal.OffsetOf(typeof(Buffer),"Size"),pcm.Length);
                Check(AudioQueueEnqueueBuffer(queue,buffer,0,IntPtr.Zero));Check(AudioQueueStart(queue,IntPtr.Zero));
            }
            public bool Advance()
            {
                if(Marshal.ReadInt32(flag)==0)return false;
                if(!draining){Check(AudioQueueStop(queue,false));draining=true;}
                uint running,size=4;Check(AudioQueueGetProperty(queue,0x6171726e,out running,ref size));return size==4 && running==0;
            }
            public void Dispose()
            {
                // A static callback and unmanaged flag stay valid even on a failed close.
                if(queue!=IntPtr.Zero){if(AudioQueueDispose(queue,true)!=0)ReleaseFailed();queue=IntPtr.Zero;}
                if(flag!=IntPtr.Zero){Marshal.FreeHGlobal(flag);flag=IntPtr.Zero;}
            }
        }
#elif LINUX
        const string Audio="libasound.so.2";
        [DllImport(Audio)]static extern int snd_pcm_open(out IntPtr pcm,string name,int stream,int mode);
        [DllImport(Audio)]static extern int snd_pcm_set_params(IntPtr pcm,int format,int access,uint channels,uint rate,int resample,uint latency);
        [DllImport(Audio)]static extern IntPtr snd_pcm_writei(IntPtr pcm,IntPtr data,UIntPtr frames);
        [DllImport(Audio)]static extern int snd_pcm_start(IntPtr pcm);
        [DllImport(Audio)]static extern int snd_pcm_state(IntPtr pcm);
        [DllImport(Audio)]static extern int snd_pcm_delay(IntPtr pcm,out IntPtr delay);
        [DllImport(Audio)]static extern int snd_pcm_drop(IntPtr pcm);
        [DllImport(Audio)]static extern int snd_pcm_close(IntPtr pcm);
        sealed class AlsaDevice : IPreviewPcmDevice
        {
            IntPtr handle,data;int frames,written,align;
            public void Open(byte[] pcm,int rate,int channels)
            {
                Check(snd_pcm_open(out handle,"default",0,1));
                Check(snd_pcm_set_params(handle,2,3,(uint)channels,(uint)rate,1,100000));
                align=channels*2;frames=pcm.Length/align;data=Marshal.AllocHGlobal(pcm.Length);Marshal.Copy(pcm,0,data,pcm.Length);
            }
            void StartQueued(){if(written>0 && snd_pcm_state(handle)==2)Check(snd_pcm_start(handle));}
            public bool Advance()
            {
                for(int i=0;i<4 && written<frames;i++) {
                    int count=Math.Min(1024,frames-written);long n=snd_pcm_writei(handle,IntPtr.Add(data,written*align),(UIntPtr)(uint)count).ToInt64();
                    if(n==-11){StartQueued();return false;}
                    Core.Require(n>=0 && n<=count,"The system PCM output stopped accepting preview frames.");
                    if(n==0){StartQueued();return false;}written+=(int)n;
                }
                StartQueued();if(written<frames)return false;
                IntPtr delay;int result=snd_pcm_delay(handle,out delay);
                if(result==-32)return true;Check(result);return delay.ToInt64()<=0;
            }
            public void Dispose()
            {
                if(handle!=IntPtr.Zero){snd_pcm_drop(handle);if(snd_pcm_close(handle)!=0)ReleaseFailed();handle=IntPtr.Zero;}
                if(data!=IntPtr.Zero){Marshal.FreeHGlobal(data);data=IntPtr.Zero;}
            }
        }
        const string Pulse="libpulse.so.0";
        [StructLayout(LayoutKind.Sequential)]struct PulseFormat {public int Format;public uint Rate;public byte Channels;}
        [StructLayout(LayoutKind.Sequential)]struct PulseBuffers {public uint Maximum,Target,Prebuffer,Minimum,Fragment;}
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]delegate void PulseSuccess(IntPtr stream,int success,IntPtr user);
        static readonly PulseSuccess drained=(stream,success,user)=>Marshal.WriteInt32(user,success==0?-1:1);
        [DllImport(Pulse)]static extern IntPtr pa_mainloop_new();
        [DllImport(Pulse)]static extern IntPtr pa_mainloop_get_api(IntPtr loop);
        [DllImport(Pulse)]static extern int pa_mainloop_iterate(IntPtr loop,int block,out int result);
        [DllImport(Pulse)]static extern void pa_mainloop_free(IntPtr loop);
        [DllImport(Pulse)]static extern IntPtr pa_context_new(IntPtr api,string name);
        [DllImport(Pulse)]static extern int pa_context_connect(IntPtr context,IntPtr server,int flags,IntPtr spawn);
        [DllImport(Pulse)]static extern int pa_context_get_state(IntPtr context);
        [DllImport(Pulse)]static extern void pa_context_disconnect(IntPtr context);
        [DllImport(Pulse)]static extern void pa_context_unref(IntPtr context);
        [DllImport(Pulse)]static extern IntPtr pa_stream_new(IntPtr context,string name,ref PulseFormat format,IntPtr map);
        [DllImport(Pulse)]static extern int pa_stream_connect_playback(IntPtr stream,IntPtr device,ref PulseBuffers buffers,int flags,IntPtr volume,IntPtr sync);
        [DllImport(Pulse)]static extern int pa_stream_get_state(IntPtr stream);
        [DllImport(Pulse)]static extern UIntPtr pa_stream_writable_size(IntPtr stream);
        [DllImport(Pulse)]static extern int pa_stream_write(IntPtr stream,IntPtr data,UIntPtr bytes,IntPtr free,long offset,int seek);
        [DllImport(Pulse)]static extern IntPtr pa_stream_drain(IntPtr stream,PulseSuccess callback,IntPtr user);
        [DllImport(Pulse)]static extern int pa_stream_disconnect(IntPtr stream);
        [DllImport(Pulse)]static extern void pa_stream_unref(IntPtr stream);
        [DllImport(Pulse)]static extern void pa_operation_cancel(IntPtr operation);
        [DllImport(Pulse)]static extern void pa_operation_unref(IntPtr operation);
        sealed class PulseDevice : IPreviewPcmDevice
        {
            IntPtr loop,context,stream,data,flag,operation;int length,written,rate,channels;
            internal bool Submitted {get{return written>0;}}
            public void Open(byte[] pcm,int rate,int channels)
            {
                this.rate=rate;this.channels=channels;length=pcm.Length;
                loop=pa_mainloop_new();Core.Require(loop!=IntPtr.Zero,"The desktop audio loop is unavailable.");
                context=pa_context_new(pa_mainloop_get_api(loop),"TimeSplitters mod preview");Core.Require(context!=IntPtr.Zero,"The desktop audio context is unavailable.");
                // Connect only to the existing configured service; never spawn a daemon.
                Check(pa_context_connect(context,IntPtr.Zero,1,IntPtr.Zero));
                data=Marshal.AllocHGlobal(length);Marshal.Copy(pcm,0,data,length);flag=Marshal.AllocHGlobal(4);Marshal.WriteInt32(flag,0);
            }
            public bool Advance()
            {
                for(int i=0;i<8;i++){int status;Core.Require(pa_mainloop_iterate(loop,0,out status)>=0,"The desktop audio loop failed.");}
                int state=pa_context_get_state(context);Core.Require(state>=0 && state<=4,"The desktop audio service disconnected.");if(state!=4)return false;
                if(stream==IntPtr.Zero) {
                    var format=new PulseFormat {Format=3,Rate=(uint)rate,Channels=(byte)channels};
                    stream=pa_stream_new(context,"Inspected PCM portion",ref format,IntPtr.Zero);Core.Require(stream!=IntPtr.Zero,"The desktop PCM stream is unavailable.");
                    var buffers=new PulseBuffers {Maximum=(uint)(length+rate*channels*2/10),Target=(uint)(rate*channels*2/10),Prebuffer=0,Minimum=(uint)(rate*channels*2/50),Fragment=UInt32.MaxValue};
                    Check(pa_stream_connect_playback(stream,IntPtr.Zero,ref buffers,0x2000,IntPtr.Zero,IntPtr.Zero));
                }
                state=pa_stream_get_state(stream);Core.Require(state>=0 && state<=2,"The desktop PCM stream failed.");if(state!=2)return false;
                if(written<length) {
                    ulong available=pa_stream_writable_size(stream).ToUInt64();Core.Require(available!=UInt64.MaxValue,"The desktop PCM stream is not writable.");
                    int bytes=(int)Math.Min(Math.Min(available,(ulong)(length-written)),32768UL);bytes-=bytes%(channels*2);
                    if(bytes>0){Check(pa_stream_write(stream,IntPtr.Add(data,written),(UIntPtr)(uint)bytes,IntPtr.Zero,0,0));written+=bytes;}
                    if(written<length)return false;
                }
                if(operation==IntPtr.Zero){operation=pa_stream_drain(stream,drained,flag);Core.Require(operation!=IntPtr.Zero,"The desktop PCM stream could not drain.");}
                int result=Marshal.ReadInt32(flag);Core.Require(result>=0,"The desktop PCM drain failed.");return result==1;
            }
            public void Dispose()
            {
                // This non-threaded loop dispatches callbacks only inside Advance.
                if(operation!=IntPtr.Zero){pa_operation_cancel(operation);pa_operation_unref(operation);operation=IntPtr.Zero;}
                if(stream!=IntPtr.Zero){pa_stream_disconnect(stream);pa_stream_unref(stream);stream=IntPtr.Zero;}
                if(context!=IntPtr.Zero){pa_context_disconnect(context);pa_context_unref(context);context=IntPtr.Zero;}
                if(loop!=IntPtr.Zero){pa_mainloop_free(loop);loop=IntPtr.Zero;}
                if(flag!=IntPtr.Zero){Marshal.FreeHGlobal(flag);flag=IntPtr.Zero;}
                if(data!=IntPtr.Zero){Marshal.FreeHGlobal(data);data=IntPtr.Zero;}
            }
        }
        sealed class Device : IPreviewPcmDevice
        {
            IPreviewPcmDevice inner;byte[] pcm;int rate,channels;bool fallback;
            public void Open(byte[] pcm,int rate,int channels)
            {
                this.pcm=pcm;this.rate=rate;this.channels=channels;inner=new PulseDevice();
                try{inner.Open(pcm,rate,channels);}
                catch(Exception error){if(!MayFallback(error))throw;UseAlsa();}
            }
            static bool MayFallback(Exception error){return error is System.IO.InvalidDataException || error is DllNotFoundException || error is EntryPointNotFoundException;}
            void UseAlsa(){inner.Dispose();fallback=true;inner=new AlsaDevice();inner.Open(pcm,rate,channels);}
            public bool Advance()
            {
                try{return inner.Advance();}
                catch(Exception error) {
                    var pulse=inner as PulseDevice;
                    if(fallback || pulse==null || pulse.Submitted || !MayFallback(error))throw;
                    UseAlsa();return false;
                }
            }
            public void Dispose(){if(inner!=null){inner.Dispose();inner=null;}pcm=null;}
        }
#else
        const string Audio="winmm.dll";
        [StructLayout(LayoutKind.Sequential,Pack=2)]struct Format {public ushort Tag,Channels;public uint Rate,ByteRate;public ushort Align,Bits,Extra;}
        [StructLayout(LayoutKind.Sequential)]struct Header {public IntPtr Data;public uint Bytes,Recorded;public UIntPtr User;public uint Flags,Loops;public IntPtr Next;public UIntPtr Reserved;}
        [DllImport(Audio)]static extern uint waveOutOpen(out IntPtr handle,uint device,ref Format format,IntPtr callback,IntPtr instance,uint flags);
        [DllImport(Audio)]static extern uint waveOutPrepareHeader(IntPtr handle,IntPtr header,uint size);
        [DllImport(Audio)]static extern uint waveOutWrite(IntPtr handle,IntPtr header,uint size);
        [DllImport(Audio)]static extern uint waveOutReset(IntPtr handle);
        [DllImport(Audio)]static extern uint waveOutUnprepareHeader(IntPtr handle,IntPtr header,uint size);
        [DllImport(Audio)]static extern uint waveOutClose(IntPtr handle);
        sealed class Device : IPreviewPcmDevice
        {
            IntPtr handle,data,header;bool prepared;readonly uint size=(uint)Marshal.SizeOf(typeof(Header));
            public void Open(byte[] pcm,int rate,int channels)
            {
                var format=new Format {Tag=1,Channels=(ushort)channels,Rate=(uint)rate,ByteRate=(uint)(rate*channels*2),Align=(ushort)(channels*2),Bits=16};
                Check(waveOutOpen(out handle,UInt32.MaxValue,ref format,IntPtr.Zero,IntPtr.Zero,0));
                data=Marshal.AllocHGlobal(pcm.Length);Marshal.Copy(pcm,0,data,pcm.Length);header=Marshal.AllocHGlobal((int)size);
                Marshal.StructureToPtr(new Header {Data=data,Bytes=(uint)pcm.Length},header,false);
                Check(waveOutPrepareHeader(handle,header,size));prepared=true;Check(waveOutWrite(handle,header,size));
            }
            public bool Advance(){return (((Header)Marshal.PtrToStructure(header,typeof(Header))).Flags&1)!=0;}
            public void Dispose()
            {
                // Never free a header/buffer still borrowed by a driver after failed reset.
                if(handle!=IntPtr.Zero) {
                    if(waveOutReset(handle)!=0)ReleaseFailed();
                    if(prepared && waveOutUnprepareHeader(handle,header,size)!=0)ReleaseFailed();prepared=false;
                    if(waveOutClose(handle)!=0)ReleaseFailed();handle=IntPtr.Zero;
                }
                if(header!=IntPtr.Zero){Marshal.FreeHGlobal(header);header=IntPtr.Zero;}
                if(data!=IntPtr.Zero){Marshal.FreeHGlobal(data);data=IntPtr.Zero;}
            }
        }
#endif
    }
}
