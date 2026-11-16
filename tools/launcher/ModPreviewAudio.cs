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
            }catch(DllNotFoundException){throw new InvalidOperationException("The system PCM audio service is unavailable. On Linux, install ALSA with a configured default output.");}
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
        sealed class Device : IPreviewPcmDevice
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
