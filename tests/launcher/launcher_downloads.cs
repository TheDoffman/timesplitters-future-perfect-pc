// Synthetic HTTP/ZIPs and disposable files only. No real game or GitHub issue.
using System;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.IO.Compression;
using TimeSplitters.Launcher;
static class LauncherDownloadChecks
{
    static string temp;static int count;
    static void Need(bool value){if(!value)throw new Exception("Download assertion failed.");}
    static void Test(string name,Action action){action();count++;Console.WriteLine("PASS "+name);}
    static void Reject(Action action){try{action();}catch(IOException){return;}catch(InvalidDataException){return;}throw new Exception("Expected download rejection.");}
    static string Hash(byte[] data){using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(data)).Replace("-","").ToLowerInvariant();}
    sealed class Handler:HttpMessageHandler {public Func<HttpRequestMessage,HttpResponseMessage> Reply;protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage r,CancellationToken t){var v=Reply(r);v.RequestMessage=r;return Task.FromResult(v);}}
    sealed class Capture:IProgress<double>,IProgress<TransferSnapshot> {public Action<TransferSnapshot> OnValue;public List<TransferSnapshot> Values=new List<TransferSnapshot>();public void Report(double v){}public void Report(TransferSnapshot v){Values.Add(v);if(OnValue!=null)OnValue(v);}}
    static Release Release(byte[] bytes){return new Release{Tag="v1.0.0",AssetName="test.zip",AssetUrl="https://github.com/"+ReleaseClient.Repository+"/releases/download/v1.0.0/test.zip",Digest="sha256:"+Hash(bytes),Size=bytes.Length};}
    static HttpResponseMessage Response(byte[] data,int status=200,long start=0,long total=0){var r=new HttpResponseMessage((HttpStatusCode)status){Content=new ByteArrayContent(data)};r.Headers.ETag=new EntityTagHeaderValue("\"one\"");if(status==206)r.Content.Headers.ContentRange=new ContentRangeHeaderValue(start,start+data.Length-1,total);return r;}
    sealed class Fixture:IDisposable {
        public readonly string Root,Prefix,Dest;public readonly byte[] Bytes=Encoding.UTF8.GetBytes("0123456789abcdefghijklmnopqrstuv");public Release Item;
        public Fixture(){Root=Path.Combine(temp,Guid.NewGuid().ToString("N"));Directory.CreateDirectory(Root);Prefix=Path.Combine(Root,"cache");Dest=Path.Combine(Root,"release.zip");Item=Release(Bytes);}
        public void Run(Func<HttpRequestMessage,HttpResponseMessage> response,IProgress<double> progress=null,CancellationToken token=default(CancellationToken),string destination=null){using(var client=new ReleaseClient(new Handler{Reply=response}))client.Download(Item,destination??Dest,progress,token,Prefix).GetAwaiter().GetResult();}
        public void Partial(){Reject(()=>Run(r=> {var result=Response(Bytes.Take(10).ToArray());result.Content.Headers.ContentLength=Bytes.Length;return result;}));Need(new FileInfo(Prefix+".part").Length==10 && !File.Exists(Dest));}
        public void Complete(){Need(File.ReadAllBytes(Dest).SequenceEqual(Bytes));}
        public void Dispose(){Need(Path.GetDirectoryName(Root)==temp);Directory.Delete(Root,true);}
    }
    sealed class CancelStream:MemoryStream {
        readonly CancellationTokenSource stop;int reads;
        public CancelStream(byte[] bytes,CancellationTokenSource source):base(bytes){stop=source;}
        public override Task<int> ReadAsync(byte[] buffer,int offset,int length,CancellationToken token){if(reads++>0){stop.Cancel();return Task.FromCanceled<int>(stop.Token);}return base.ReadAsync(buffer,offset,Math.Min(10,length),token);}
    }
    sealed class StalledStream:MemoryStream {public override Task<int> ReadAsync(byte[] buffer,int offset,int count,CancellationToken token){return new TaskCompletionSource<int>().Task;}}
    sealed class CutStream:Stream {
        readonly Stream source;readonly IDisposable owner;int remaining=65536;
        public CutStream(Stream input,IDisposable content){source=input;owner=content;}
        public override bool CanRead{get{return true;}}public override bool CanSeek{get{return false;}}public override bool CanWrite{get{return false;}}public override long Length{get{throw new NotSupportedException();}}public override long Position{get{throw new NotSupportedException();}set{throw new NotSupportedException();}}
        public override Task<int> ReadAsync(byte[] bytes,int offset,int count,CancellationToken token){if(remaining==0)throw new IOException("Synthetic connection interruption");return ReadPart(bytes,offset,Math.Min(count,remaining),token);}
        async Task<int> ReadPart(byte[] bytes,int offset,int count,CancellationToken token){int read=await source.ReadAsync(bytes,offset,count,token).ConfigureAwait(false);remaining-=read;return read;}
        public override int Read(byte[] bytes,int offset,int count){throw new NotSupportedException();}public override void Flush(){}public override long Seek(long offset,SeekOrigin origin){throw new NotSupportedException();}public override void SetLength(long size){throw new NotSupportedException();}public override void Write(byte[] bytes,int offset,int count){throw new NotSupportedException();}
        protected override void Dispose(bool disposing){if(disposing){source.Dispose();owner.Dispose();}base.Dispose(disposing);}
    }
    sealed class NetworkProbe:DelegatingHandler {
        readonly bool cut;public HttpStatusCode AssetStatus;
        public NetworkProbe(bool interrupt):base(new HttpClientHandler()){cut=interrupt;}
        protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken token){
            bool asset=request.RequestUri.AbsolutePath.EndsWith(".zip",StringComparison.OrdinalIgnoreCase);
            var response=await base.SendAsync(request,token).ConfigureAwait(false);
            if(asset){
                AssetStatus=response.StatusCode;
                if(cut){var original=response.Content;var headers=original.Headers.ToArray();var input=await original.ReadAsStreamAsync().ConfigureAwait(false);response.Content=new StreamContent(new CutStream(input,original));foreach(var header in headers)response.Content.Headers.TryAddWithoutValidation(header.Key,header.Value);}
            }
            return response;
        }
    }
    public static int VerifyPublicDownload(){
        string root=Path.Combine(Path.GetDirectoryName(typeof(LauncherDownloadChecks).Assembly.Location),"public-resume-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        try{using(var timeout=new CancellationTokenSource(60000)){
            Release item;using(var client=new ReleaseClient())item=client.CheckLauncher(false,timeout.Token).GetAwaiter().GetResult();Need(item!=null&&item.Size>65536);
            string destination=Path.Combine(root,"verified.zip"),prefix=Path.Combine(root,"cache");
            using(var client=new ReleaseClient(new NetworkProbe(true)))Reject(()=>client.Download(item,destination,null,timeout.Token,prefix).GetAwaiter().GetResult());
            Need(DownloadTransfer.SavedBytes(prefix,item)==65536);var handler=new NetworkProbe(false);
            using(var client=new ReleaseClient(handler))client.Download(item,destination,null,timeout.Token,prefix).GetAwaiter().GetResult();
            Need(Core.Hash(destination)==item.Digest.Substring(7));Console.WriteLine("PASS real GitHub launcher "+item.Tag+" interrupted at 65536 bytes, retried in a new client, and verified SHA-256; server status "+(int)handler.AssetStatus);
            return 0;
        }}finally{Need(Path.GetDirectoryName(root)==Path.GetDirectoryName(typeof(LauncherDownloadChecks).Assembly.Location));Directory.Delete(root,true);}
    }
    static void STA(Action action){Exception error=null;var thread=new Thread(()=> {try{action();}catch(Exception ex){error=ex;}});thread.SetApartmentState(ApartmentState.STA);thread.Start();Need(thread.Join(60000));if(error!=null)throw error;}
    public static int Run(){temp=Path.Combine(Path.GetDirectoryName(typeof(LauncherDownloadChecks).Assembly.Location),"download-tests-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(temp);count=0;try{
        Test("verified fresh download reports verification before completion",()=>{using(var f=new Fixture()){var progress=new Capture();f.Run(r=>Response(f.Bytes),progress);f.Complete();Need(progress.Values.Any(v=>v.Phase=="Verifying")&&progress.Values.Last().Phase=="Complete");}});
        Test("partial survives a new client and resumes using byte range and strong If-Range",()=>{using(var f=new Fixture()){f.Partial();Need(DownloadTransfer.SavedBytes(f.Prefix,f.Item)==10);f.Run(r=>{Need(r.Headers.Range.Ranges.Single().From==10&&r.Headers.IfRange.EntityTag.Tag=="\"one\"");return Response(f.Bytes.Skip(10).ToArray(),206,10,f.Bytes.Length);});f.Complete();}});
        Test("user cancellation retains written bytes for restart",()=>{using(var f=new Fixture())using(var stop=new CancellationTokenSource()){try{f.Run(r=>{var v=new HttpResponseMessage(HttpStatusCode.OK){Content=new StreamContent(new CancelStream(f.Bytes,stop))};v.Content.Headers.ContentLength=f.Bytes.Length;return v;},null,stop.Token);throw new Exception("Expected cancel");}catch(OperationCanceledException){}Need(new FileInfo(f.Prefix+".part").Length==10);f.Run(r=>Response(f.Bytes.Skip(10).ToArray(),206,10,f.Bytes.Length));f.Complete();}});
        Test("cancellation interrupts a stalled stream that ignores its token",()=>{using(var f=new Fixture())using(var stop=new CancellationTokenSource(80)){
            var timer=System.Diagnostics.Stopwatch.StartNew();try{f.Run(r=>{var v=new HttpResponseMessage(HttpStatusCode.OK){Content=new StreamContent(new StalledStream())};v.Content.Headers.ContentLength=f.Item.Size;return v;},null,stop.Token);throw new Exception("Expected cancel");}catch(OperationCanceledException){}Need(timer.ElapsedMilliseconds<3000 && !File.Exists(f.Dest));
        }});
        Test("pausing verified preparation retains a complete cache and no destination",()=>{using(var f=new Fixture())using(var stop=new CancellationTokenSource()){
            var progress=new Capture {OnValue=v=>{if(v.Phase=="Preparing")stop.Cancel();}};
            try{f.Run(r=>Response(f.Bytes),progress,stop.Token);throw new Exception("Expected cancel");}catch(OperationCanceledException){}
            Need(!File.Exists(f.Dest)&&DownloadTransfer.SavedBytes(f.Prefix,f.Item)==f.Bytes.Length);
            f.Run(r=>{throw new Exception("Completed download should be reused");});f.Complete();
        }});
        Test("server ignoring Range restarts instead of appending",()=>{using(var f=new Fixture()){f.Partial();f.Run(r=>{Need(r.Headers.Range!=null);return Response(f.Bytes);});f.Complete();}});
        Test("416 resets once and retries from zero",()=>{using(var f=new Fixture()){f.Partial();int calls=0;f.Run(r=>{if(calls++==0)return new HttpResponseMessage(HttpStatusCode.RequestedRangeNotSatisfiable);Need(r.Headers.Range==null);return Response(f.Bytes);});Need(calls==2);f.Complete();}});
        foreach(string defect in new[]{"start","total","length","etag","encoding"})Test("invalid resume "+defect+" is discarded before retry",()=>{using(var f=new Fixture()){f.Partial();Reject(()=>f.Run(r=>{var v=Response(f.Bytes.Skip(10).ToArray(),206,10,f.Bytes.Length);if(defect=="start")v.Content.Headers.ContentRange=new ContentRangeHeaderValue(9,30,32);if(defect=="total")v.Content.Headers.ContentRange=new ContentRangeHeaderValue(10,31,33);if(defect=="length")v.Content.Headers.ContentLength=10;if(defect=="etag")v.Headers.ETag=new EntityTagHeaderValue("\"two\"");if(defect=="encoding")v.Content.Headers.ContentEncoding.Add("gzip");return v;}));Need(!File.Exists(f.Dest)&&!File.Exists(f.Prefix+".part"));f.Run(r=>{Need(r.Headers.Range==null);return Response(f.Bytes);});f.Complete();}});
        Test("release hash change invalidates previous saved bytes",()=>{using(var f=new Fixture()){f.Partial();var bytes=Encoding.UTF8.GetBytes("different release bytes");f.Item=Release(bytes);f.Run(r=>{Need(r.Headers.Range==null);return Response(bytes);});Need(File.ReadAllBytes(f.Dest).SequenceEqual(bytes));}});
        Test("release URL change invalidates previous saved bytes",()=>{using(var f=new Fixture()){f.Partial();f.Item.AssetUrl=f.Item.AssetUrl.Replace("v1.0.0/","v1.0.1/");f.Run(r=>{Need(r.Headers.Range==null);return Response(f.Bytes);});f.Complete();}});
        Test("verified complete cache avoids another request and corrupt cache restarts",()=>{using(var f=new Fixture()){f.Run(r=>Response(f.Bytes));f.Run(r=>{throw new Exception("Unexpected request");},null,default(CancellationToken),Path.Combine(f.Root,"second.zip"));File.Delete(f.Dest);File.WriteAllBytes(f.Prefix+".part",new byte[f.Bytes.Length]);f.Run(r=>{Need(r.Headers.Range==null);return Response(f.Bytes);});f.Complete();}});
        Test("resumed prefixes are included in the final checksum",()=>{using(var f=new Fixture()){f.Partial();File.WriteAllBytes(f.Prefix+".part",new byte[10]);Reject(()=>f.Run(r=>Response(f.Bytes.Skip(10).ToArray(),206,10,f.Bytes.Length)));Need(!File.Exists(f.Dest)&&!File.Exists(f.Prefix+".part"));}});
        Test("checksum mismatch never publishes a destination or retains damaged bytes",()=>{using(var f=new Fixture()){Reject(()=>f.Run(r=>Response(new byte[f.Bytes.Length])));Need(!File.Exists(f.Dest)&&!File.Exists(f.Prefix+".part"));}});
        Test("oversized body cannot grow the cache beyond published size",()=>{using(var f=new Fixture()){Reject(()=>f.Run(r=>{var v=Response(f.Bytes.Concat(f.Bytes).ToArray());v.Content.Headers.ContentLength=null;return v;}));Need(!File.Exists(f.Dest)&&!File.Exists(f.Prefix+".part"));}});
        Test("oversized saved cache restarts safely",()=>{using(var f=new Fixture()){f.Partial();File.WriteAllBytes(f.Prefix+".part",new byte[100]);f.Run(r=>{Need(r.Headers.Range==null);return Response(f.Bytes);});f.Complete();}});
        Test("unknown partial and invalid metadata are preserved",()=>{using(var f=new Fixture()){File.WriteAllText(f.Prefix+".part","personal ISO marker");Reject(()=>f.Run(r=>Response(f.Bytes)));Need(File.ReadAllText(f.Prefix+".part")=="personal ISO marker");File.WriteAllText(f.Prefix+".json","null");Reject(()=>f.Run(r=>Response(f.Bytes)));Need(File.ReadAllText(f.Prefix+".part")=="personal ISO marker");}});
        Test("busy cache lock and pre-existing destination never overwrite files",()=>{using(var f=new Fixture()){f.Partial();using(var guard=new FileStream(f.Prefix+".lock",FileMode.Open,FileAccess.ReadWrite,FileShare.None))Reject(()=>f.Run(r=>Response(f.Bytes)));Need(new FileInfo(f.Prefix+".part").Length==10);File.WriteAllText(f.Dest,"keep");Reject(()=>f.Run(r=>Response(f.Bytes)));Need(File.ReadAllText(f.Dest)=="keep");}});
        Test("cache refuses a redirected directory without touching its target",()=>{using(var f=new Fixture()){
            string target=Path.Combine(temp,"link-target-"+Guid.NewGuid().ToString("N")),link=Path.Combine(f.Root,"redirect");Directory.CreateDirectory(target);File.WriteAllText(Path.Combine(target,"original.iso"),"keep");
            var info=new System.Diagnostics.ProcessStartInfo("cmd.exe","/c mklink /J "+SelfUpdate.Quote(link)+" "+SelfUpdate.Quote(target)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
            using(var process=System.Diagnostics.Process.Start(info)){process.StandardOutput.ReadToEnd();process.StandardError.ReadToEnd();Need(process.WaitForExit(10000)&&process.ExitCode==0);}
            try{using(var client=new ReleaseClient(new Handler{Reply=r=>{throw new Exception("No network call expected");}}))Reject(()=>client.Download(f.Item,f.Dest,null,CancellationToken.None,Path.Combine(link,"cache")).GetAwaiter().GetResult());Need(File.ReadAllText(Path.Combine(target,"original.iso"))=="keep"&&Directory.GetFiles(target).Length==1);
                string missing=target+"-missing",dangling=Path.Combine(f.Root,"dangling");
                var danglingInfo=new System.Diagnostics.ProcessStartInfo("cmd.exe","/c mklink /J "+SelfUpdate.Quote(dangling)+" "+SelfUpdate.Quote(missing)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
                using(var process=System.Diagnostics.Process.Start(danglingInfo)){process.StandardOutput.ReadToEnd();process.StandardError.ReadToEnd();Need(process.WaitForExit(10000)&&process.ExitCode==0);}
                try{using(var client=new ReleaseClient(new Handler{Reply=r=>{throw new Exception("No request for a dangling link");}}))Reject(()=>client.Download(f.Item,f.Dest,null,CancellationToken.None,Path.Combine(dangling,"cache")).GetAwaiter().GetResult());Need(!Directory.Exists(missing));}
                finally{Need(Path.GetDirectoryName(dangling)==f.Root&&(File.GetAttributes(dangling)&FileAttributes.ReparsePoint)!=0);Directory.Delete(dangling);}
}
            finally{Need(Path.GetDirectoryName(link)==f.Root&&(File.GetAttributes(link)&FileAttributes.ReparsePoint)!=0);Directory.Delete(link);}
        }});
        Test("pending cleanup and cancelled preparation retain the independent download cache",()=>{using(var f=new Fixture()){
            string prefix=Core.PathIn(f.Root,".launcher/downloads/game"),pending=Core.PathIn(f.Root,".launcher/pending"),zip=Path.Combine(pending,"release.zip");Directory.CreateDirectory(pending);
            using(var client=new ReleaseClient(new Handler{Reply=r=>{var v=Response(f.Bytes.Take(10).ToArray());v.Content.Headers.ContentLength=f.Bytes.Length;return v;}}))Reject(()=>client.Download(f.Item,zip,null,CancellationToken.None,prefix).GetAwaiter().GetResult());
            Core.DiscardGameStaging(f.Root);Need(DownloadTransfer.SavedBytes(prefix,f.Item)==10);Directory.CreateDirectory(pending);
            using(var client=new ReleaseClient(new Handler{Reply=r=>{Need(r.Headers.Range.Ranges.Single().From==10);return Response(f.Bytes.Skip(10).ToArray(),206,10,f.Bytes.Length);}}))client.Download(f.Item,zip,null,CancellationToken.None,prefix).GetAwaiter().GetResult();
            Core.DiscardRepairStaging(f.Root,false);Need(DownloadTransfer.SavedBytes(prefix,f.Item)==f.Bytes.Length);Directory.CreateDirectory(pending);
            using(var client=new ReleaseClient(new Handler{Reply=r=>{throw new Exception("Verified cache should be reused");}}))client.Download(f.Item,zip,null,CancellationToken.None,prefix).GetAwaiter().GetResult();Need(File.ReadAllBytes(zip).SequenceEqual(f.Bytes));
        }});
        Test("cache cleanup is bounded and preserves ISO and settings siblings",()=>{using(var f=new Fixture()){f.Partial();File.WriteAllText(Path.Combine(f.Root,"original.iso"),"keep");File.WriteAllText(Path.Combine(f.Root,"settings.ini"),"keep");DownloadTransfer.Forget(f.Prefix);Need(!File.Exists(f.Prefix+".part")&&!File.Exists(f.Prefix+".json")&&File.ReadAllText(Path.Combine(f.Root,"original.iso"))=="keep"&&File.Exists(Path.Combine(f.Root,"settings.ini")));}});
        Test("rate and ETA use remaining bytes with explicit unknown estimate",()=>{var state=new TransferSnapshot{Bytes=75*1048576,Total=100*1048576,BytesPerSecond=5*1048576,Phase="Resuming"};Need(state.Percent==75&&state.Remaining.Value.TotalSeconds==5&&state.Summary.Contains("5.0 MiB/s")&&state.Summary.Contains("~5s"));state.BytesPerSecond=0;Need(!state.Remaining.HasValue&&state.Summary.Contains("Estimating"));});
        Test("recovery messages distinguish pause, network and corrupt downloads",()=>{Need(RecoveryInfo.From(new OperationCanceledException(),true).RetryLabel=="Resume download");Need(RecoveryInfo.From(new TimeoutException("private URL"),true).Title=="Connection interrupted");Need(!RecoveryInfo.From(new HttpRequestException("private URL"),true).Message.Contains("private URL"));Need(RecoveryInfo.From(new InvalidDataException("Checksum mismatch"),true).Message.Contains("fresh"));});
        Test("update feedback stops progress on pause and error and resets on retry",()=>STA(()=>{using(var f=new Fixture()){
            var ui=new LauncherWindow(f.Root,true);
            try {
                var bar=(System.Windows.Controls.ProgressBar)ui.Window.FindName("UpdateProgress");
                var label=(System.Windows.Controls.TextBlock)ui.Window.FindName("UpdateStateLabel");
                var recover=typeof(LauncherWindow).GetMethod("ShowRecovery",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance);
                ui.ShowUpdateState(UpdatePresentationState.Downloading);bar.Value=42;
                Need(bar.Visibility==System.Windows.Visibility.Visible && !bar.IsIndeterminate);
                ui.ShowUpdateState(UpdatePresentationState.Verifying);Need(bar.IsIndeterminate);
                recover.Invoke(ui,new object[]{new OperationCanceledException(),new Func<Task>(()=>Task.FromResult(0)),true});
                Need(label.Text=="PAUSED" && !bar.IsIndeterminate && bar.Visibility==System.Windows.Visibility.Hidden && bar.Value==0);
                Need((string)((System.Windows.Controls.Button)ui.Window.FindName("RetryButton")).Content=="Resume download");
                ui.ShowUpdateState(UpdatePresentationState.Checking);Need(bar.IsIndeterminate && bar.Visibility==System.Windows.Visibility.Visible);
                recover.Invoke(ui,new object[]{new TimeoutException(),new Func<Task>(()=>Task.FromResult(0)),false});
                Need(label.Text=="NEEDS ATTENTION" && !bar.IsIndeterminate && bar.Visibility==System.Windows.Visibility.Hidden);
                ui.ShowUpdateState(UpdatePresentationState.Ready);Need(label.Text=="UP TO DATE" && bar.Visibility==System.Windows.Visibility.Hidden);
                Need(!Directory.Exists(Path.Combine(f.Root,".launcher/pending")));
            }finally{ui.Window.Close();}
        }}));
        Test("recovery actions retry and open embedded repair while busy state disables retry",()=>STA(()=>{using(var f=new Fixture()){var ui=new LauncherWindow(f.Root,true);try{int retried=0;var show=typeof(LauncherWindow).GetMethod("ShowRecovery",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance);show.Invoke(ui,new object[]{new IOException("network"),new Func<Task>(()=>{retried++;return Task.FromResult(0);}),true});var retry=(System.Windows.Controls.Button)ui.Window.FindName("RetryButton");retry.RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));Need(retried==1);((System.Windows.Controls.Button)ui.Window.FindName("RecoveryRepairButton")).RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));Need(((System.Windows.Controls.ContentControl)ui.Window.FindName("MaintenanceHost")).Visibility==System.Windows.Visibility.Visible);typeof(LauncherWindow).GetField("busy",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance).SetValue(ui,true);typeof(LauncherWindow).GetMethod("Refresh",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance).Invoke(ui,null);Need(!retry.IsEnabled);}finally{typeof(LauncherWindow).GetField("busy",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance).SetValue(ui,false);ui.Window.Close();}}}));
        Console.WriteLine("PASS: "+count+" isolated download and recovery checks.");return 0;
    }finally{Need(Path.GetDirectoryName(temp)==Path.GetDirectoryName(typeof(LauncherDownloadChecks).Assembly.Location));Directory.Delete(temp,true);}}
}
