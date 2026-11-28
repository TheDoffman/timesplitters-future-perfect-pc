using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.Controls.Documents;
using Avalonia.Controls.Primitives;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;
using System.Windows.Documents;
#endif

namespace TimeSplitters.Launcher
{
    public enum ConsoleSeverity { Normal, Debug, Info, Warning, Error }

    // Tails capture files, never owns or changes the game's output pipes.
    public sealed class GameLogTail
    {
        sealed class Source
        {
            public long Offset; public string Partial=""; public Decoder Decoder=Encoding.UTF8.GetDecoder();
        }
        readonly string root;
        readonly Source[] sources={new Source(),new Source()};
        readonly Queue<string> lines=new Queue<string>();
        string identity="";
        public string Status="Waiting for a game launched from this launcher.";
        public GameLogTail(string root){this.root=root;}
        public void Clear(){lines.Clear();foreach(var source in sources)source.Partial="";}
        public string Text(bool errorsOnly)
        {
            var all=lines.Concat(sources.Select((s,i)=>s.Partial.Length==0?"":Prefix(i)+s.Partial));
            return String.Join(Environment.NewLine,all.Where(s=>s.Length>0&&(!errorsOnly||IsError(s))));
        }
        static string Prefix(int index){return index==0?"[OUT] ":"[ERR] ";}
        const RegexOptions LogOptions=RegexOptions.IgnoreCase|RegexOptions.CultureInvariant;
        // OUT/ERR describe the pipe. An explicit log level describes severity.
        static readonly Regex level=new Regex(@"^(?:\[[^\]\r\n]+\]\s*)*?(?:\[(?<level>ERROR|FATAL|PANIC|CRITICAL|WARN(?:ING)?|INFO|DEBUG|TRACE)\]|(?<level>ERROR|FATAL|PANIC|CRITICAL|WARN(?:ING)?|INFO|DEBUG|TRACE)(?=[:\s]))",LogOptions);
        static readonly Regex zeroErrors=new Regex(@"\b(?:[a-z0-9]+_)*(?:errors?|failed|failures?)\s*[=:]\s*0+(?![\w.])|(?<![\w.])0+\s+(?:(?:create|device|voice|allocation)\s+)?(?:errors?|failed|failures?)\b|\bno\s+(?:errors?|failures?|exceptions?)\b",LogOptions);
        static readonly Regex error=new Regex(@"\b(?:[a-z0-9]+_)*(?:errors?|fatal|exceptions?|panic|failed|failures?|assertion)(?:\b|_)",LogOptions);
        public static bool IsError(string text) { return Severity(text)==ConsoleSeverity.Error; }
        public static ConsoleSeverity Severity(string text)
        {
            if(String.IsNullOrWhiteSpace(text))return ConsoleSeverity.Normal;
            var severity=level.Match(text.TrimStart());
            if(severity.Success) {
                string value=severity.Groups["level"].Value.ToUpperInvariant();
                if(value=="ERROR"||value=="FATAL"||value=="PANIC"||value=="CRITICAL")return ConsoleSeverity.Error;
                if(value=="WARN"||value=="WARNING")return ConsoleSeverity.Warning;
                return value=="INFO"?ConsoleSeverity.Info:ConsoleSeverity.Debug;
            }
            // Remove only zero/negative evidence, not the whole statistics line:
            // a positive counter or a separate failure on that line must survive.
            return error.IsMatch(zeroErrors.Replace(text,""))?ConsoleSeverity.Error:ConsoleSeverity.Normal;
        }
        void Add(string line){lines.Enqueue(line);while(lines.Count>1000)lines.Dequeue();}
        public void Poll()
        {
            try {
                string tag=GameSessions.ConsoleLogTag(root);
                if(String.IsNullOrEmpty(tag))return;
                if(identity!=tag){identity=tag;lines.Clear();for(int i=0;i<2;i++)sources[i]=new Source();}
                string err=Core.PathIn(root,LauncherPlatform.SessionLog(tag));
                string[] paths={Path.ChangeExtension(err,".out"),err};
                bool found=false;
                for(int i=0;i<2;i++) {
                    if(!File.Exists(paths[i]))continue;
                    found=true;var source=sources[i];
                    using(var file=new FileStream(paths[i],FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)) {
                        if(file.Length<source.Offset){source.Offset=0;source.Partial="";source.Decoder.Reset();Add("[Console] Log restarted.");}
                        if(file.Length-source.Offset>65536){source.Offset=file.Length-65536;source.Partial="";source.Decoder.Reset();Add("[Console] Showing the latest 64 KiB of this stream.");}
                        file.Position=source.Offset;var bytes=new byte[65536];int count=file.Read(bytes,0,bytes.Length);source.Offset+=count;
                        var chars=new char[Encoding.UTF8.GetMaxCharCount(count)];int length=source.Decoder.GetChars(bytes,0,count,chars,0,false);
                        string text=source.Partial+new string(chars,0,length);int start=0;
                        for(int j=0;j<text.Length;j++)if(text[j]=='\n'){Add(Prefix(i)+text.Substring(start,Math.Min(j-start,4096)).TrimEnd('\r'));start=j+1;}
                        source.Partial=text.Substring(start);if(source.Partial.Length>4096)source.Partial=source.Partial.Substring(source.Partial.Length-4096);
                    }
                }
                Status=found?"Session "+tag+" · latest 1,000 lines · OUT / ERR identify the source stream":"Waiting for game output…";
            } catch(IOException){Status="Log temporarily unavailable; retrying…";}
              catch(UnauthorizedAccessException){Status="Cannot read the game log. Check folder permissions.";}
              catch(Exception ex){Status="Cannot read session: "+ex.Message;}
        }
    }
    // Framework document adapters share classification, colors and original text.
    // Both permit selection/copy across differently colored lines.
    public sealed class ConsoleOutput : UserControl
    {
        string text="";
        public event EventHandler TextChanged;
        public bool IsReadOnly {get{return true;}}
        public static string Color(ConsoleSeverity severity)
        {
            switch(severity){case ConsoleSeverity.Error:return "#FF9E95";case ConsoleSeverity.Warning:return "#FFD18A";case ConsoleSeverity.Info:return "#85D4FF";case ConsoleSeverity.Debug:return "#9EB4C8";default:return "#C7D9E8";}
        }
#if MACOS || LINUX
        readonly SelectableTextBlock document=new SelectableTextBlock {TextWrapping=TextWrapping.Wrap,FontFamily=Ui.Monospace,FontSize=12};
        readonly ScrollViewer scroll=new ScrollViewer {VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};
        public ConsoleOutput(){scroll.Content=document;Content=scroll;}
#else
        readonly RichTextBox document=new RichTextBox {IsReadOnly=true,IsDocumentEnabled=true,Background=Brushes.Transparent,BorderThickness=new Thickness(0),Padding=new Thickness(0),VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};
        public ConsoleOutput(){Content=document;}
#endif
        public string Text {get{return text;}set{
            value=value??"";if(value==text)return;text=value;
#if MACOS || LINUX
            document.Inlines.Clear();
#else
            var flow=new FlowDocument {PagePadding=new Thickness(0),FontFamily=Ui.Monospace,FontSize=12};
            var paragraph=new Paragraph {Margin=new Thickness(0)};flow.Blocks.Add(paragraph);
#endif
            // Keep the delimiters in each run so copied output retains line breaks.
            foreach(Match line in Regex.Matches(text,@"[^\n]*\n|[^\n]+$")){
                var run=new Run(line.Value){Foreground=Ui.Brush(Color(GameLogTail.Severity(line.Value)))};
#if MACOS || LINUX
                document.Inlines.Add(run);
#else
                paragraph.Inlines.Add(run);
#endif
            }
#if !MACOS && !LINUX
            document.Document=flow;
#endif
            if(TextChanged!=null)TextChanged(this,EventArgs.Empty);
        }}
        public void ScrollToEnd(){
#if MACOS || LINUX
            scroll.ScrollToEnd();
#else
            document.ScrollToEnd();
#endif
        }
    }
    public sealed class ConsolePage : IDisposable
    {
        public readonly Grid View=new Grid();
        public readonly ConsoleOutput Output;
        readonly TextBlock status;
        readonly GameLogTail tail;
        readonly DispatcherTimer timer=new DispatcherTimer {Interval=TimeSpan.FromMilliseconds(500)};
        readonly CheckBox errors,follow;
        readonly Button pause;
        bool paused,polling,disposed;
        public ConsolePage(string root,bool preview=false,bool embedded=false)
        {
            tail=new GameLogTail(root);Ui.Theme(View);View.Margin=embedded?new Thickness(0,16,0,0):new Thickness(30,24,30,20);
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            View.RowDefinitions.Add(new RowDefinition {Height=new GridLength(1,GridUnitType.Star)});View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            if(!embedded)View.Children.Add(PageTheme.Header("LIVE GAME OUTPUT","Console","Game logs and errors appear here while you play. Select text to copy it."));
            var actions=new WrapPanel {Margin=new Thickness(0,0,0,16)};Grid.SetRow(actions,1);View.Children.Add(actions);
            pause=new Button {Content="Pause",Name="PauseConsole",Margin=new Thickness(0,0,10,0)};
            var clear=new Button {Content="Clear view",Name="ClearConsole",Margin=new Thickness(0,0,18,0)};
            errors=new CheckBox {Content="Errors only",Name="ConsoleErrorsOnly",Margin=new Thickness(0,0,18,0),VerticalAlignment=VerticalAlignment.Center};
            follow=new CheckBox {Content="Auto-scroll",Name="ConsoleFollow",IsChecked=true,VerticalAlignment=VerticalAlignment.Center};
            actions.Children.Add(pause);actions.Children.Add(clear);actions.Children.Add(errors);actions.Children.Add(follow);
            Output=new ConsoleOutput {Name="GameConsoleOutput",FontFamily=Ui.Monospace,FontSize=12,
                Background=Ui.Brush("#08182B"),Foreground=Ui.Brush("#C7D9E8"),BorderThickness=new Thickness(0),Padding=new Thickness(14)};
            var card=PageTheme.Card(Output);card.Padding=new Thickness(0);Grid.SetRow(card,2);View.Children.Add(card);
            var legend=new WrapPanel {Margin=new Thickness(0,10,0,0)};
            foreach(var severity in new[]{ConsoleSeverity.Error,ConsoleSeverity.Warning,ConsoleSeverity.Info,ConsoleSeverity.Debug}){
                var label=PageTheme.Text(severity.ToString(),11,ConsoleOutput.Color(severity));label.Margin=new Thickness(0,0,16,0);legend.Children.Add(label);
            }
            var footer=new StackPanel();Grid.SetRow(footer,3);View.Children.Add(footer);footer.Children.Add(legend);
            status=PageTheme.Text("Waiting for a game launched from this launcher.",12,"#9EB4C8");status.Margin=new Thickness(0,12,0,0);footer.Children.Add(status);
            pause.Click+=(s,e)=>{paused=!paused;pause.Content=paused?"Resume":"Pause";status.Text=paused?"Display paused. Game logs continue to be saved.":tail.Status;};
            clear.Click+=(s,e)=>{if(polling)return;tail.Clear();Output.Text="";};
            Ui.CheckedChanged(errors,()=>{if(!polling)Display();});
            timer.Tick+=async(s,e)=>{if(paused||polling||disposed)return;polling=true;try{await Task.Run(()=>tail.Poll());if(!disposed&&!paused)Display();}finally{polling=false;}};
            if(preview){Output.Text="[OUT] TimeSplitters: Future Perfect\n[OUT] Renderer initialized · 2560 × 1440\n[OUT] Audio device ready\n[OUT] Loading game data…\n[OUT] Entering main menu\n[OUT] Multiplayer session started\n[ERR] Warning: connection interrupted; retrying\n[ERR] Error: connection timed out\n[OUT] Returned to lobby";status.Text="Sample output · preview only";}
            else timer.Start();
        }
        void Display()
        {
            string text=tail.Text(errors.IsChecked==true);if(Output.Text!=text){Output.Text=text;
                if(follow.IsChecked==true){
                    Output.ScrollToEnd();
                }}
            status.Text=paused?"Display paused. Game logs continue to be saved.":tail.Status;
        }
        public void Dispose(){disposed=true;timer.Stop();}
    }
}
