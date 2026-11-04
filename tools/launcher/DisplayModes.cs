using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Runtime.InteropServices;

namespace TimeSplitters.Launcher
{
    public sealed class DisplayMode
    {
        public readonly int Width, Height;
        public bool SavedCustom;
        public DisplayMode(int width, int height) { Width=width; Height=height; }
        public bool Automatic { get { return Width==1 && Height==1; } }
        public override string ToString() { return Automatic ? "Auto (current display)" : Width==0 ? "Original console resolution" : Width+" × "+Height+(SavedCustom?" (saved custom)":""); }
        public static bool Valid(int w,int h)
        { return (w==1 && h==1) || (w==0 && h==0) || (w>=640 && h>=480 && w<=7680 && h<=4320 && (long)w*h<=33177600 && w>=h && w<=4*h); }
    }
    public static class DisplayModes
    {
        public sealed class Aspect
        {
            public string Label; public int Width,Height;
            public override string ToString() { return Label; }
        }
        static readonly string Catalog=ReadCatalog();
        static string ReadCatalog()
        {
            using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("display-modes.inc"))
            using(var reader=new StreamReader(stream)) return reader.ReadToEnd();
        }
        public static readonly Aspect[] Aspects=new[]{new Aspect{Label="All",Width=0,Height=0}}.Concat(
            Regex.Matches(Catalog,"PC_DISPLAY_ASPECT\\(\"([^\"]+)\", ([0-9]+), ([0-9]+)\\)").Cast<Match>()
            .Select(m=>new Aspect{Label=m.Groups[1].Value,Width=Int32.Parse(m.Groups[2].Value),Height=Int32.Parse(m.Groups[3].Value)})).ToArray();
        public static int AspectIndex(int width,int height)
        {
            if(width==1 && height==1)return Aspects.Length-1;
            if(width==0 && height==0)return 1;
            if(!DisplayMode.Valid(width,height))return -1;
            for(int i=1;i<Aspects.Length-1;i++)
                if(Math.Abs((double)width*Aspects[i].Height/(height*Aspects[i].Width)-1)<(Aspects[i].Label=="21:9"?.02:.002))return i;
            return -1;
        }
        public static string RatioLabel(int width,int height)
        {
            int group=AspectIndex(width,height);if(group>=0)return Aspects[group].Label;
            if(!DisplayMode.Valid(width,height))return "Invalid size";
            int a=width,b=height;while(b!=0){int next=a%b;a=b;b=next;}
            return (width/a)+":"+(height/a);
        }
        public static IEnumerable<DisplayMode> Presets()
        {
            return Regex.Matches(Catalog,@"PC_DISPLAY_PRESET\(([0-9]+), ([0-9]+)\)").Cast<Match>()
                .Select(m=>new DisplayMode(Int32.Parse(m.Groups[1].Value),Int32.Parse(m.Groups[2].Value)));
        }
        // DEVMODEW's physical-pixel fields; fixed native layout is independent of DPI.
        [StructLayout(LayoutKind.Explicit, Size=220, CharSet=CharSet.Unicode)]
        struct Mode
        {
            [FieldOffset(68)] public ushort Size;
            [FieldOffset(70)] public ushort Extra;
            [FieldOffset(168)] public uint Bits;
            [FieldOffset(172)] public int Width;
            [FieldOffset(176)] public int Height;
            [FieldOffset(180)] public uint Flags;
            [FieldOffset(184)] public int Hertz;
        }
        [DllImport("user32.dll", CharSet=CharSet.Unicode, ExactSpelling=true)]
        [return:MarshalAs(UnmanagedType.Bool)]
        static extern bool EnumDisplaySettingsW(string device,uint index,ref Mode mode);
        public static List<DisplayMode> Choices(IEnumerable<DisplayMode> reported,int savedWidth,int savedHeight)
        {
            var list=new List<DisplayMode>{new DisplayMode(0,0)};
            list.AddRange(Presets().Concat(reported).Where(m=>m.Width>1 && DisplayMode.Valid(m.Width,m.Height))
                .GroupBy(m=>new {m.Width,m.Height}).Select(g=>new DisplayMode(g.Key.Width,g.Key.Height))
                .OrderBy(m=>m.Width).ThenBy(m=>m.Height));
            list.Add(new DisplayMode(1,1));
            if(DisplayMode.Valid(savedWidth,savedHeight) && !list.Any(m=>m.Width==savedWidth && m.Height==savedHeight))
                list.Add(new DisplayMode(savedWidth,savedHeight){SavedCustom=true});
            return list;
        }
        public static List<DisplayMode> Primary(int savedWidth,int savedHeight)
        {
            var reported=new List<DisplayMode>();
            #if MACOS || LINUX
            reported.AddRange(NativeHost.DisplayModes());
#else
            var screen=System.Windows.Forms.Screen.PrimaryScreen;
            if(screen!=null) for(uint index=0;;index++)
            {
                var mode=new Mode{Size=220};
                if(!EnumDisplaySettingsW(screen.DeviceName,index,ref mode)) break;
                if(mode.Bits>=24) reported.Add(new DisplayMode(mode.Width,mode.Height));
            }
            #endif
            return Choices(reported,savedWidth,savedHeight);
        }
        public static List<int> PrimaryRefreshRates()
        {
            var rates=new List<int>{0};
#if MACOS || LINUX
            return rates; // Native refresh switching is not implemented; keep the saved rate.
#else
            var screen=System.Windows.Forms.Screen.PrimaryScreen;
            var current=new Mode{Size=220};
            if(screen==null || !EnumDisplaySettingsW(screen.DeviceName,UInt32.MaxValue,ref current)) return rates;
            for(uint index=0;;index++) {
                var mode=new Mode{Size=220};if(!EnumDisplaySettingsW(screen.DeviceName,index,ref mode))break;
                if(mode.Width==current.Width && mode.Height==current.Height && mode.Bits==current.Bits &&
                    (mode.Flags&2)==0 && (mode.Hertz==120 || mode.Hertz==144) && !rates.Contains(mode.Hertz)) rates.Add(mode.Hertz);
            }
            rates.Sort();return rates;
#endif
        }
        public static int PrimaryRefreshRate()
        {
#if MACOS || LINUX
            return 0; // Variable refresh displays may not expose a fixed desktop rate.
#else
            var screen=System.Windows.Forms.Screen.PrimaryScreen;
            var current=new Mode{Size=220};
            return screen!=null && EnumDisplaySettingsW(screen.DeviceName,UInt32.MaxValue,ref current)
                && current.Hertz>1 ? current.Hertz : 0;
#endif
        }
    }
}
