using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
#if LINUX
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
#endif

namespace TimeSplitters.Launcher
{
    internal interface IModSessionVault
    {
        byte[] Read(string binding);
        void Write(string binding,byte[] secret);
        void Delete(string binding);
    }
    // Only opaque session bytes enter the OS store. Passwords/invitations never do.
    internal sealed class ModSessionVault : IModSessionVault
    {
        const string Service="org.timesplitters.launcher.workshop";
        const int Maximum=2048;
        static void Binding(string value){Core.Require(value!=null && Regex.IsMatch(value,@"\A[0-9a-f]{64}\z"),"Invalid saved sign-in binding.");}
        static void Secret(byte[] value){Core.Require(value!=null && value.Length>0 && value.Length<=Maximum,"Saved sign-in exceeds its limit.");}
        static Exception Failed(){return new InvalidOperationException("The system credential store is unavailable or locked. Sign in without remembering, or unlock the credential store and retry.");}
        public byte[] Read(string binding)
        {
            Binding(binding);
            try {
#if MACOS
                return MacRead(binding);
#elif LINUX
                string text=Linux("lookup",binding,null);if(text==null)return null;
                byte[] value=Convert.FromBase64String(text.Trim());Secret(value);return value;
#else
                return WindowsRead(binding);
#endif
            }catch{throw Failed();}
        }
        public void Write(string binding,byte[] secret)
        {
            Binding(binding);Secret(secret);
            try {
#if MACOS
                MacWrite(binding,secret);
#elif LINUX
                Linux("store",binding,Convert.ToBase64String(secret));
#else
                WindowsWrite(binding,secret);
#endif
            }catch{throw Failed();}
        }
        public void Delete(string binding)
        {
            Binding(binding);
            try {
#if MACOS
                using(var query=MacQuery(binding)){int status=SecItemDelete(query.Value);Core.Require(status==0 || status==-25300,"Keychain delete failed.");}
#elif LINUX
                Linux("clear",binding,null);
#else
                Core.Require(CredDelete(Service+"/"+binding,1,0) || Marshal.GetLastWin32Error()==1168,"Credential delete failed.");
#endif
            }catch{throw Failed();}
        }
#if MACOS
        const string Security="/System/Library/Frameworks/Security.framework/Security";
        const string Foundation="/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation";
        static readonly IntPtr SecLibrary=NativeLibrary.Load(Security),CfLibrary=NativeLibrary.Load(Foundation);
        static IntPtr Constant(string name){return Marshal.ReadIntPtr(NativeLibrary.GetExport(SecLibrary,name));}
        [DllImport(Foundation)] static extern IntPtr CFDictionaryCreateMutable(IntPtr allocator,long capacity,IntPtr keys,IntPtr values);
        [DllImport(Foundation)] static extern void CFDictionarySetValue(IntPtr dictionary,IntPtr key,IntPtr value);
        [DllImport(Foundation)] static extern IntPtr CFStringCreateWithCString(IntPtr allocator,[MarshalAs(UnmanagedType.LPUTF8Str)] string value,uint encoding);
        [DllImport(Foundation)] static extern IntPtr CFDataCreate(IntPtr allocator,byte[] bytes,long length);
        [DllImport(Foundation)] static extern long CFDataGetLength(IntPtr data);
        [DllImport(Foundation)] static extern IntPtr CFDataGetBytePtr(IntPtr data);
        [DllImport(Foundation)] static extern void CFRelease(IntPtr value);
        [DllImport(Security)] static extern int SecItemCopyMatching(IntPtr query,out IntPtr result);
        [DllImport(Security)] static extern int SecItemAdd(IntPtr attributes,IntPtr result);
        [DllImport(Security)] static extern int SecItemUpdate(IntPtr query,IntPtr attributes);
        [DllImport(Security)] static extern int SecItemDelete(IntPtr query);
        sealed class MacDictionary : IDisposable
        {
            internal readonly IntPtr Value;
            internal MacDictionary(){Value=CFDictionaryCreateMutable(IntPtr.Zero,0,NativeLibrary.GetExport(CfLibrary,"kCFTypeDictionaryKeyCallBacks"),NativeLibrary.GetExport(CfLibrary,"kCFTypeDictionaryValueCallBacks"));Core.Require(Value!=IntPtr.Zero,"Keychain dictionary allocation failed.");}
            internal void Set(string key,IntPtr value){Core.Require(value!=IntPtr.Zero,"Keychain value allocation failed.");CFDictionarySetValue(Value,Constant(key),value);}
            internal void Text(string key,string value){IntPtr text=CFStringCreateWithCString(IntPtr.Zero,value,0x08000100);try{Set(key,text);}finally{if(text!=IntPtr.Zero)CFRelease(text);}}
            internal void Data(byte[] value){IntPtr data=CFDataCreate(IntPtr.Zero,value,value.Length);try{Set("kSecValueData",data);}finally{if(data!=IntPtr.Zero)CFRelease(data);}}
            public void Dispose(){CFRelease(Value);}
        }
        static MacDictionary MacQuery(string binding)
        {
            var query=new MacDictionary();try {
                query.Set("kSecClass",Constant("kSecClassGenericPassword"));query.Text("kSecAttrService",Service);query.Text("kSecAttrAccount",binding);
                query.Set("kSecUseAuthenticationUI",Constant("kSecUseAuthenticationUIFail"));return query;
            }catch{query.Dispose();throw;}
        }
        static byte[] MacRead(string binding)
        {
            using(var query=MacQuery(binding)) {
                query.Set("kSecReturnData",Marshal.ReadIntPtr(NativeLibrary.GetExport(CfLibrary,"kCFBooleanTrue")));
                query.Set("kSecMatchLimit",Constant("kSecMatchLimitOne"));IntPtr result=IntPtr.Zero;
                try {int status=SecItemCopyMatching(query.Value,out result);if(status==-25300)return null;
                    Core.Require(status==0 && result!=IntPtr.Zero,"Keychain read failed.");long size=CFDataGetLength(result);Core.Require(size>0 && size<=Maximum,"Invalid keychain item size.");
                    byte[] value=new byte[(int)size];Marshal.Copy(CFDataGetBytePtr(result),value,0,value.Length);return value;
                }finally{if(result!=IntPtr.Zero)CFRelease(result);}
            }
        }
        static void MacWrite(string binding,byte[] secret)
        {
            using(var query=MacQuery(binding))using(var changes=new MacDictionary()) {
                changes.Data(secret);int status=SecItemUpdate(query.Value,changes.Value);
                if(status==-25300){query.Data(secret);status=SecItemAdd(query.Value,IntPtr.Zero);}
                Core.Require(status==0,"Keychain write failed.");
            }
        }
#elif LINUX
        static string Linux(string operation,string binding,string input)
        {
            const string executable="/usr/bin/secret-tool";
            Core.Require(File.Exists(executable),"Secret Service client unavailable.");
            var start=new ProcessStartInfo(executable){UseShellExecute=false,CreateNoWindow=true,RedirectStandardInput=true,RedirectStandardOutput=true,RedirectStandardError=true};
            start.ArgumentList.Add(operation);
            if(operation=="store"){start.ArgumentList.Add("--label=TimeSplitters workshop sign-in");start.ArgumentList.Add("--collection=default");}
            foreach(string part in new[]{"application",Service,"binding",binding})start.ArgumentList.Add(part);
            using(var process=Process.Start(start))using(var timeout=new CancellationTokenSource(TimeSpan.FromSeconds(15))) {
                try {
                    Task<string> output=Bounded(process.StandardOutput,4096,timeout.Token),error=Bounded(process.StandardError,4096,timeout.Token);
                    if(input!=null)process.StandardInput.WriteLine(input);process.StandardInput.Close();
                    process.WaitForExitAsync(timeout.Token).GetAwaiter().GetResult();string text=output.GetAwaiter().GetResult(),err=error.GetAwaiter().GetResult();
                    if((operation=="lookup" || operation=="clear") && process.ExitCode==1 && String.IsNullOrWhiteSpace(err))return null;
                    Core.Require(process.ExitCode==0,"Secret Service operation failed.");return text;
                }catch{try{if(!process.HasExited)process.Kill(true);}catch{}throw;}
            }
        }
        static async Task<string> Bounded(StreamReader reader,int maximum,CancellationToken cancel)
        {
            var result=new StringBuilder();char[] buffer=new char[1024];int count;
            while((count=await reader.ReadAsync(buffer.AsMemory(0,buffer.Length),cancel).ConfigureAwait(false))>0){Core.Require(result.Length+count<=maximum,"Credential-store response exceeds its limit.");result.Append(buffer,0,count);}return result.ToString();
        }
#else
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
        struct Credential
        {
            internal uint Flags,Type;internal string TargetName,Comment;internal System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
            internal uint BlobSize;internal IntPtr Blob;internal uint Persist,AttributeCount;internal IntPtr Attributes;internal string TargetAlias,UserName;
        }
        [DllImport("advapi32.dll",EntryPoint="CredReadW",CharSet=CharSet.Unicode,SetLastError=true)] [return:MarshalAs(UnmanagedType.Bool)] static extern bool CredRead(string target,uint type,uint flags,out IntPtr credential);
        [DllImport("advapi32.dll",EntryPoint="CredWriteW",CharSet=CharSet.Unicode,SetLastError=true)] [return:MarshalAs(UnmanagedType.Bool)] static extern bool CredWrite(ref Credential credential,uint flags);
        [DllImport("advapi32.dll",EntryPoint="CredDeleteW",CharSet=CharSet.Unicode,SetLastError=true)] [return:MarshalAs(UnmanagedType.Bool)] static extern bool CredDelete(string target,uint type,uint flags);
        [DllImport("advapi32.dll")] static extern void CredFree(IntPtr credential);
        static byte[] WindowsRead(string binding)
        {
            IntPtr pointer;
            if(!CredRead(Service+"/"+binding,1,0,out pointer)){Core.Require(Marshal.GetLastWin32Error()==1168,"Credential read failed.");return null;}
            try {var item=(Credential)Marshal.PtrToStructure(pointer,typeof(Credential));Core.Require(item.Type==1 && item.BlobSize>0 && item.BlobSize<=Maximum && item.Blob!=IntPtr.Zero,"Invalid credential item.");byte[] value=new byte[(int)item.BlobSize];Marshal.Copy(item.Blob,value,0,value.Length);return value;}finally{CredFree(pointer);}
        }
        static void WindowsWrite(string binding,byte[] secret)
        {
            IntPtr blob=Marshal.AllocHGlobal(secret.Length);
            try {Marshal.Copy(secret,0,blob,secret.Length);var item=new Credential {Type=1,TargetName=Service+"/"+binding,Comment="TimeSplitters workshop sign-in",BlobSize=(uint)secret.Length,Blob=blob,Persist=2,UserName="workshop-session"};Core.Require(CredWrite(ref item,0),"Credential write failed.");}
            finally{Marshal.Copy(new byte[secret.Length],0,blob,secret.Length);Marshal.FreeHGlobal(blob);}
        }
#endif
    }
}
