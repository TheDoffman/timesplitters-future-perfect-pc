using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace TimeSplitters.Launcher
{
    public sealed class TransferSnapshot
    {
        public long Bytes, Total;
        public double BytesPerSecond;
        public string Phase;
        public double Percent { get { return Total>0?100.0*Bytes/Total:0; } }
        public TimeSpan? Remaining { get { return BytesPerSecond>0 && Bytes<Total?TimeSpan.FromSeconds(Math.Min(31536000,(Total-Bytes)/BytesPerSecond)):(TimeSpan?)null; } }
        static string Size(double bytes) { return bytes>=1073741824?(bytes/1073741824).ToString("0.00",CultureInfo.InvariantCulture)+" GiB":bytes>=1048576?(bytes/1048576).ToString("0.0",CultureInfo.InvariantCulture)+" MiB":(bytes/1024).ToString("0.0",CultureInfo.InvariantCulture)+" KiB"; }
        public string Summary {
            get {
                if(Phase=="Verifying")return "Verifying download · "+Size(Total);
                if(Phase=="Preparing")return "Preparing verified download · "+Size(Total);
                if(Phase=="Complete")return "Download verified · "+Size(Total);
                var eta=Remaining;
                string time=!eta.HasValue?"Estimating time remaining":eta.Value.TotalHours>=1?"~"+((int)eta.Value.TotalHours)+"h "+eta.Value.Minutes+"m remaining":eta.Value.TotalMinutes>=1?"~"+((int)eta.Value.TotalMinutes)+"m "+eta.Value.Seconds+"s remaining":"~"+Math.Max(1,(int)Math.Ceiling(eta.Value.TotalSeconds))+"s remaining";
                return Phase+" · "+Percent.ToString("0",CultureInfo.InvariantCulture)+"% · "+Size(Bytes)+" / "+Size(Total)+" · "+(BytesPerSecond>0?Size(BytesPerSecond)+"/s · ":"")+time;
            }
        }
    }
    public sealed class TransferProgress : IProgress<double>, IProgress<TransferSnapshot>
    {
        readonly IProgress<TransferSnapshot> target;
        public TransferProgress(Action<TransferSnapshot> report) { target=new Progress<TransferSnapshot>(report); }
        public void Report(TransferSnapshot value) { target.Report(value); }
        public void Report(double value) { target.Report(new TransferSnapshot {Bytes=(long)value,Total=100,Phase="Preparing"}); }
    }
    // Keep the OS error and exact operation paths together before UI recovery
    // replaces low-level exceptions with friendly text. Do not infer a detection
    // from access denied, a missing file, or localized exception messages.
    public sealed class AntivirusBlockedException : IOException
    {
        public AntivirusBlockedException(string paths, int code, Exception inner)
            : base("Antivirus blocked access to a file (Windows error " + code + ").\n" + paths +
                "\nCheck your antivirus quarantine/security history (including McAfee or Windows Security) for this path and detection name. If a file was removed, restore the complete official package after reviewing the detection, then retry. Keep .launcher for recovery.", inner) { }
    }
    public static class AntivirusErrors
    {
        public static int Code(Exception error)
        {
#if !MACOS && !LINUX
            var native = error as System.ComponentModel.Win32Exception;
            int code = native != null ? native.NativeErrorCode :
                error is IOException && (error.HResult & unchecked((int)0xffff0000)) == unchecked((int)0x80070000) ? error.HResult & 65535 : 0;
            if (code == 225 || code == 226) return code;
#endif
            return 0;
        }
        public static AntivirusBlockedException Find(Exception error)
        {
            if (error == null) return null;
            var blocked = error as AntivirusBlockedException;
            if (blocked != null) return blocked;
            var aggregate = error as AggregateException;
            if (aggregate != null) {
                foreach (var inner in aggregate.InnerExceptions) { blocked = Find(inner); if (blocked != null) return blocked; }
                return null;
            }
            return Find(error.InnerException);
        }
        public static void At(string path, Action operation)
        {
            try { operation(); }
            catch (Exception error) {
                int code = Code(error);
                if (code != 0) throw new AntivirusBlockedException("Affected file: " + path, code, error);
                throw;
            }
        }
        public static void Copy(string source, string target, bool overwrite)
        {
            try { File.Copy(source, target, overwrite); }
            catch (Exception error) {
                int code = Code(error);
                // CopyFile does not identify which side was rejected. Report both
                // paths rather than claiming the destination was detected.
                if (code != 0) throw new AntivirusBlockedException("Copy source: " + source + "\nCopy destination: " + target, code, error);
                throw;
            }
        }
        public static Process Start(ProcessStartInfo info)
        {
            Process process = null;
            At(info.FileName, () => { process = Process.Start(info); });
            return process;
        }
    }
    public sealed class RecoveryInfo
    {
        public string Title, Message, RetryLabel;
        public static RecoveryInfo LauncherStart(Exception error, string application = null)
        {
            var blocked = AntivirusErrors.Find(error);
            if (blocked != null) return From(blocked, false);
            int code = AntivirusErrors.Code(error);
            if (code != 0) return From(new AntivirusBlockedException("Affected file: " + (application ?? "Bin\\TimeSplitters.App.exe"), code, error), false);
            var native = error as System.ComponentModel.Win32Exception;
            if (native != null && native.NativeErrorCode == 1223)
                return new RecoveryInfo { Title="Windows didn't open the launcher", RetryLabel="Retry",
                    Message="Windows cancelled the launch request. SmartScreen or another security prompt may need attention. Choose Open launcher folder, then open Bin\\TimeSplitters.App.exe directly to check for a prompt. Review it, then return here and choose Retry." };
            if (error is UnauthorizedAccessException || (native != null && native.NativeErrorCode == 5))
                return new RecoveryInfo { Title="Windows blocked launcher startup", RetryLabel="Retry",
                    Message="Windows denied access to the launcher application. Check the folder's permissions and Windows Security protection history. Open launcher folder to inspect the application, then Retry. View logs for details." };
            if (error is FileNotFoundException || error is DirectoryNotFoundException ||
                (native != null && (native.NativeErrorCode == 2 || native.NativeErrorCode == 3)) ||
                (error is InvalidDataException && error.Message.StartsWith("Launcher application files are missing",StringComparison.Ordinal)))
                return new RecoveryInfo { Title="Launcher files are missing", RetryLabel="Retry",
                    Message="Missing file: " + (application ?? "Bin\\TimeSplitters.App.exe") + "\nRestore the complete official launcher package into this folder, keeping Bin and Support beside TimeSplitters Launcher.exe, then Retry. Check antivirus quarantine/security history if the file disappeared. Open launcher folder to check the files, or View logs for details." };
            return new RecoveryInfo { Title="The launcher couldn't open", RetryLabel="Retry",
                Message="Try again, or choose Open launcher folder to check the application. View logs for details about the startup failure." };
        }
        public static RecoveryInfo From(Exception error,bool download)
        {
            var blocked = AntivirusErrors.Find(error);
            if (blocked != null) return new RecoveryInfo {Title="Antivirus blocked a file", Message=blocked.Message +
                (ReferenceEquals(error, blocked) ? "" : "\nRecovery details: " + error.Message), RetryLabel="Retry"};
            string title="Operation could not finish",message="Your installed files are kept. Retry, or open the logs for details.";
            if(error is OperationCanceledException) {title=download?"Download paused":"Operation cancelled";message=download?"Downloaded bytes are kept. Resume when you are ready; installed files have not changed.":"Installed files have not changed. You can retry when ready.";}
            else if(error.Message.StartsWith("Checksum mismatch",StringComparison.Ordinal)){title="Download verification failed";message="The damaged download was discarded. Retry to download a fresh verified copy. Installed files have not changed.";}
            else if(error is HttpRequestException || error is TimeoutException){title="Connection interrupted";message="Check your connection, then retry. Matching partial downloads will resume automatically.";}
            else if(error.Message.IndexOf("request limit",StringComparison.OrdinalIgnoreCase)>=0){title="GitHub is temporarily limiting requests";message="Wait a little before retrying. Your installed game is still available.";}
            else if(error is UnauthorizedAccessException){title="The launcher cannot write these files";message="Check the installation folder's write permissions and whether files are locked, then retry. View logs for details.";}
            else if((error is IOException && ((error.HResult&65535)==112 || (error.HResult&65535)==39)) || error.Message.IndexOf("not enough free space",StringComparison.OrdinalIgnoreCase)>=0){title="Not enough free space";message="Free space on the installation drive, then retry. Matching partial downloads are kept.";}
            else if(error is InvalidDataException){title="Release could not be prepared";message=error.Message+" Use Repair / Verify to check the installed files, or view logs for details.";}
            else if(error is IOException){title=download?"Download or file operation interrupted":"File operation interrupted";message="Matching partial downloads are kept. Check your connection, free space and file access, then retry or view logs.";}
            return new RecoveryInfo {Title=title,Message=message,RetryLabel=download && error is OperationCanceledException?"Resume download":"Retry"};
        }
    }
    public static class DownloadTransfer
    {
        const string Owner="tsfp-download-v1";
        static string Safe(string prefix,string extension)
        {
            string path=Core.PathIn(Path.GetDirectoryName(Path.GetFullPath(prefix)),Path.GetFileName(prefix)+extension);
            // Exists follows links and returns false for a dangling target. Inspect
            // attributes as well before creating a cache file or its atomic sidecar.
            for(string cursor=path;cursor!=null;cursor=Path.GetDirectoryName(cursor)){
                Core.RequireUnlinked(cursor,"Linked download paths are not supported; saved files were kept.");
            }
            return path;
        }
        static string HashText(string text) { using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(text))).Replace("-","").ToLowerInvariant(); }
        static string Identity(Release release,string hash) { return HashText(release.AssetUrl+"\n"+release.Size.ToString(CultureInfo.InvariantCulture)+"\n"+hash.ToLowerInvariant()); }
        static bool Owned(System.Collections.Generic.Dictionary<string,object> meta)
        { long size;return meta!=null && Json.String(meta,"owner")==Owner && Regex.IsMatch(Json.String(meta,"identity"),"^[a-f0-9]{64}$") && long.TryParse(Json.String(meta,"size"),out size) && size>0 && size<=2147483648L; }
        static void Discard(string prefix)
        {
            string meta=Safe(prefix,".json"),part=Safe(prefix,".part");
            if(!File.Exists(meta)){Core.Require(!File.Exists(part),"An unidentified saved download was kept. View logs before removing it manually.");return;}
            Core.Require(new FileInfo(meta).Length<=16384 && Owned(Json.Object(File.ReadAllText(meta))),"An unreadable saved download was kept. View logs for details.");
            if(File.Exists(part))File.Delete(part);File.Delete(meta);
        }
        static async Task<string> Hash(string path,CancellationToken token)
        {
            return await Task.Run(()=> {
                using(var stream=File.OpenRead(path))using(var sha=SHA256.Create()){
                    var bytes=new byte[65536];int count;
                    while((count=stream.Read(bytes,0,bytes.Length))>0){token.ThrowIfCancellationRequested();sha.TransformBlock(bytes,0,count,bytes,0);}
                    sha.TransformFinalBlock(new byte[0],0,0);return BitConverter.ToString(sha.Hash).Replace("-","").ToLowerInvariant();
                }
            },token).ConfigureAwait(false);
        }
        static async Task Publish(string part,string destination,CancellationToken token)
        {
            token.ThrowIfCancellationRequested();bool created=false;
            try {
                using(var input=new FileStream(part,FileMode.Open,FileAccess.Read,FileShare.Read,65536,true))
                using(var output=new FileStream(destination,FileMode.CreateNew,FileAccess.Write,FileShare.None,65536,true)){
                    created=true;await input.CopyToAsync(output,65536,token).ConfigureAwait(false);token.ThrowIfCancellationRequested();
                }
            }catch{if(created)try{File.Delete(destination);}catch(IOException){}catch(UnauthorizedAccessException){}throw;}
        }
        static void Report(IProgress<double> progress,long bytes,long total,string phase,double speed=0)
        {
            var detailed=progress as IProgress<TransferSnapshot>;
            if(detailed!=null)detailed.Report(new TransferSnapshot {Bytes=bytes,Total=total,Phase=phase,BytesPerSecond=speed});
            else if(progress!=null)progress.Report(100.0*bytes/total);
        }
        public static long SavedBytes(string prefix,Release release)
        {
            try {
                if(release==null || release.Digest==null || !release.Digest.StartsWith("sha256:",StringComparison.Ordinal))return 0;
                string path=Safe(prefix,".json"),part=Safe(prefix,".part");
                if(!File.Exists(path)||!File.Exists(part)||new FileInfo(path).Length>16384)return 0;
                var meta=Json.Object(File.ReadAllText(path));
                long bytes=new FileInfo(part).Length;
                return Owned(meta) && Json.String(meta,"identity")==Identity(release,release.Digest.Substring(7)) && bytes>0 && bytes<=release.Size?bytes:0;
            }catch(IOException){return 0;}catch(InvalidDataException){return 0;}catch(UnauthorizedAccessException){return 0;}catch(ArgumentException){return 0;}
        }
        public static void Forget(string prefix)
        {
            try {using(var guard=new FileStream(Safe(prefix,".lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None))Discard(prefix);}
            catch(IOException) { } catch(InvalidDataException) { } catch(UnauthorizedAccessException) { } catch(ArgumentException) { }
        }
        static void Observe(Task task) { task.ContinueWith(t=> {var observed=t.Exception;},TaskContinuationOptions.OnlyOnFaulted); }
        static async Task<int> Read(Stream input,byte[] buffer,HttpResponseMessage response,CancellationToken token)
        {
            using(var wait=CancellationTokenSource.CreateLinkedTokenSource(token)) {
                var read=input.ReadAsync(buffer,0,buffer.Length,token);
                try {
                    if(await Task.WhenAny(read,Task.Delay(TimeSpan.FromSeconds(30),wait.Token)).ConfigureAwait(false)!=read){
                        response.Dispose();Observe(read);
                        token.ThrowIfCancellationRequested();throw new TimeoutException("Download stopped responding. Retry to resume.");
                    }
                    return await read.ConfigureAwait(false);
                }finally{wait.Cancel();}
            }
        }
        public static async Task Run(HttpClient http,Release release,string destination,string expected,string prefix,IProgress<double> progress,CancellationToken token)
        {
            Core.Require(release.Size>0 && release.Size<=2147483648L && Regex.IsMatch(expected,"^[a-fA-F0-9]{64}$"),"The published download size or checksum is invalid.");
            destination=Safe(destination,"");
            Core.Require(!File.Exists(destination),"A download destination already exists. Its files were kept.");
            string part=Safe(prefix,".part"),metaPath=Safe(prefix,".json"),lockPath=Safe(prefix,".lock");
            Directory.CreateDirectory(Path.GetDirectoryName(part));
            using(var guard=new FileStream(lockPath,FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)) {
                string id=Identity(release,expected),etag="";
                if(File.Exists(metaPath)){
                    Core.Require(new FileInfo(metaPath).Length<=16384,"Saved download metadata is oversized; its files were kept.");
                    var meta=Json.Object(File.ReadAllText(metaPath));
                    Core.Require(Owned(meta),"Unidentified saved download metadata was kept. View logs for details.");
                    if(Json.String(meta,"identity")!=id)Discard(prefix);else etag=Json.String(meta,"etag");
                } else Core.Require(!File.Exists(part),"An unidentified saved download was kept. View logs before removing it manually.");
                Action save=()=> {Safe(prefix,".json.new");Core.Write(metaPath,Json.Encode(new {owner=Owner,identity=id,size=release.Size,etag=etag}));};
                save();
                long offset=File.Exists(part)?new FileInfo(part).Length:0;
                if(offset>release.Size){Discard(prefix);etag="";save();offset=0;}
                if(offset==release.Size){
                    Report(progress,offset,release.Size,"Verifying");
                    if((await Hash(part,token).ConfigureAwait(false)).Equals(expected,StringComparison.OrdinalIgnoreCase)){Report(progress,release.Size,release.Size,"Preparing");await Publish(part,destination,token).ConfigureAwait(false);Report(progress,offset,release.Size,"Complete");return;}
                    Discard(prefix);etag="";save();offset=0;
                }
                bool invalid=false;
                try {
                    for(int attempt=0;attempt<2;attempt++) {
                        token.ThrowIfCancellationRequested();Report(progress,offset,release.Size,offset>0?"Resuming":"Connecting");
                        using(var request=new HttpRequestMessage(HttpMethod.Get,release.AssetUrl)) {
                            request.Headers.AcceptEncoding.ParseAdd("identity");
                            if(offset>0){request.Headers.Range=new RangeHeaderValue(offset,null);EntityTagHeaderValue tag;if(EntityTagHeaderValue.TryParse(etag,out tag)&&!tag.IsWeak)request.Headers.IfRange=new RangeConditionHeaderValue(tag);}
                            HttpResponseMessage response;
                            using(var headers=CancellationTokenSource.CreateLinkedTokenSource(token)){
                                headers.CancelAfter(TimeSpan.FromSeconds(30));
                                try{response=await http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,headers.Token).ConfigureAwait(false);}
                                catch(OperationCanceledException){token.ThrowIfCancellationRequested();throw new TimeoutException("Download connection timed out. Retry to resume.");}
                            }
                            using(response){
                                if(response.StatusCode==HttpStatusCode.RequestedRangeNotSatisfiable && offset>0 && attempt==0){Discard(prefix);etag="";save();offset=0;continue;}
                                if(response.StatusCode==HttpStatusCode.Forbidden || (int)response.StatusCode==429)throw new IOException("GitHub's request limit was reached. Retry later.");
                                response.EnsureSuccessStatusCode();
                                if(response.RequestMessage.RequestUri.Scheme!="https")throw new InvalidDataException("Insecure asset redirect.");
                                long end=release.Size;
                                if(response.Content.Headers.ContentEncoding.Any(e=>!e.Equals("identity",StringComparison.OrdinalIgnoreCase))){invalid=true;throw new InvalidDataException("Unexpected download encoding; retry a fresh download.");}
                                if(response.StatusCode==HttpStatusCode.PartialContent){
                                    var range=response.Content.Headers.ContentRange;
                                    if(range==null || range.Unit!="bytes" || !range.HasRange || range.From!=offset || range.Length!=release.Size || range.To<offset || range.To>=release.Size ||
                                        (response.Content.Headers.ContentLength.HasValue && response.Content.Headers.ContentLength.Value!=range.To.Value-offset+1) ||
                                        (etag.Length>0 && response.Headers.ETag!=null && !response.Headers.ETag.IsWeak && response.Headers.ETag.ToString()!=etag)){
                                        invalid=true;throw new InvalidDataException("The server returned an invalid resume range. Retry to start a fresh download.");
                                    }
                                    end=range.To.Value+1;
                                } else if(response.StatusCode==HttpStatusCode.OK){offset=0;}
                                else {invalid=true;throw new InvalidDataException("Unexpected download response. Retry to start a fresh download.");}
                                if(response.Content.Headers.ContentLength.HasValue && response.Content.Headers.ContentLength.Value!=end-offset){invalid=true;throw new InvalidDataException("The download size differs from the published release.");}
                                etag=response.Headers.ETag!=null && !response.Headers.ETag.IsWeak && response.Headers.ETag.ToString().Length<=512?response.Headers.ETag.ToString():"";save();
                                var timer=Stopwatch.StartNew();long start=offset,lastBytes=offset;double lastTime=0,speed=0;
                                using(var input=await response.Content.ReadAsStreamAsync().ConfigureAwait(false))
                                using(var output=new FileStream(part,offset==0?FileMode.Create:FileMode.Open,FileAccess.Write,FileShare.None,65536,true)){
                                    Core.Require(output.Length==offset,"Saved download changed while resuming; retry after checking the logs.");output.Position=offset;
                                    var buffer=new byte[65536];int got;
                                    while((got=await Read(input,buffer,response,token).ConfigureAwait(false))>0){
                                        if(offset+got>end){invalid=true;throw new InvalidDataException("The download exceeds its published size.");}
                                        await output.WriteAsync(buffer,0,got,token).ConfigureAwait(false);offset+=got;
                                        double elapsed=timer.Elapsed.TotalSeconds;
                                        if(elapsed-lastTime>=0.25 || offset==release.Size){
                                            double seconds=elapsed-lastTime;
                                            if(seconds>=0.25){double sample=(offset-lastBytes)/seconds;speed=speed==0?sample:0.25*sample+0.75*speed;}
                                            Report(progress,offset,release.Size,start>0?"Resuming":"Downloading",speed);lastTime=elapsed;lastBytes=offset;
                                        }
                                    }
                                }
                                if(offset!=release.Size)throw new IOException("The release download is incomplete. Retry to resume the saved bytes.");
                                Report(progress,offset,release.Size,"Verifying");
                                if(!(await Hash(part,token).ConfigureAwait(false)).Equals(expected,StringComparison.OrdinalIgnoreCase)){invalid=true;throw new InvalidDataException("Checksum mismatch. The damaged download was discarded; retry a fresh download.");}
                                token.ThrowIfCancellationRequested();Report(progress,release.Size,release.Size,"Preparing");await Publish(part,destination,token).ConfigureAwait(false);Report(progress,offset,release.Size,"Complete");return;
                            }
                        }
                    }
                }catch{if(invalid)Discard(prefix);throw;}
            }
        }
    }
}
