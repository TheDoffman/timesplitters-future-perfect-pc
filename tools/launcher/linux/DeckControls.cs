#if LINUX
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.LogicalTree;
using Avalonia.Media;
using Avalonia.VisualTree;
using Avalonia.Threading;
namespace TimeSplitters.Launcher
{
    internal static class LinuxHandheld
    {
        static string Product(){try{return File.ReadAllText("/sys/devices/virtual/dmi/id/product_name");}catch{return "";}}
        static HandheldProfile profile;
        public static HandheldProfile Profile=>profile??=HandheldProfile.Resolve(Program.Arguments??Array.Empty<string>(),Environment.GetEnvironmentVariable,Product());
        public static void Style(Control root){
            foreach(var item in root.GetVisualDescendants().OfType<Control>().Concat(root.GetLogicalDescendants().OfType<Control>()).Prepend(root).Distinct()){
                if(item is Grid frame&&frame.Name=="MinimalSettingsFrame")frame.RowDefinitions[0].Height=new GridLength(48);
                if(item is TextBlock text&&text.FontSize<14)text.FontSize=14;
                if(item is TextBox input&&input.FontSize<14)input.FontSize=14;
                if(item is Button button){button.MinWidth=Math.Max(button.Width==112?136:44,button.MinWidth);}
                if(item is Button||item is ComboBox||item is CheckBox||item is Expander||item is Slider){item.MinHeight=Math.Max(44,item.MinHeight);}
            }
        }
        public static void Window(Window window){if(!Profile.Enabled)return;if(Profile.GamingMode){window.SystemDecorations=SystemDecorations.None;window.WindowState=WindowState.FullScreen;window.CanResize=false;}}
    }
    internal sealed class DeckControls:IDisposable
    {
        readonly Window window;readonly Grid host;readonly Border overlay,outline;readonly Func<Control> scope;readonly Func<bool> enabled;readonly Action back;
        readonly DispatcherTimer timer=new DispatcherTimer {Interval=TimeSpan.FromMilliseconds(33)};readonly Stopwatch watch=Stopwatch.StartNew();readonly HandheldPadState state=new HandheldPadState();
        DateTime lastPoll=DateTime.UtcNow;int padGeneration;readonly string root;DeckGamepad pad;Control overlayScope;Action cancelOverlay;ComboBox openCombo;bool gamepadFocus,disposed,inputAvailable;long retryAt;
        public string InputHint=>inputAvailable?"A Select   B Back   LB/RB Focus   Y Keyboard   Right stick Scroll":pad!=null&&!pad.Available?"Touch / trackpad ready · Controller runtime unavailable; the installed game supplies SDL3.":"Touch / trackpad ready · For controller navigation, use a Gamepad layout in Steam Input.";
        public DeckControls(Window window,string root,Func<Control> scope,Func<bool> enabled,Action back)
        {
            this.window=window;this.root=root;this.scope=scope;this.enabled=enabled;this.back=back;
            var content=(Control)window.Content;window.Content=null;host=new Grid();host.Children.Add(content);window.Content=host;
            overlay=new Border {Background=PageTheme.Brush("#FA0B1420"),IsVisible=false,Padding=new Thickness(24)};host.Children.Add(overlay);
            outline=new Border {BorderBrush=PageTheme.Brush("#85D4FF"),BorderThickness=new Thickness(2),IsHitTestVisible=false,IsVisible=false,HorizontalAlignment=HorizontalAlignment.Left,VerticalAlignment=VerticalAlignment.Top};host.Children.Add(outline);
            timer.Tick+=(s,e)=>Poll();window.Opened+=(s,e)=>{LinuxHandheld.Window(window);timer.Start();};window.Closed+=(s,e)=>Dispose();
            window.LayoutUpdated+=(s,e)=>{LinuxHandheld.Style(host);if(gamepadFocus)FocusOutline();};
            window.AddHandler(InputElement.PointerPressedEvent,(s,e)=>{gamepadFocus=false;outline.IsVisible=false;},RoutingStrategies.Tunnel);
            window.AddHandler(InputElement.KeyDownEvent,(s,e)=>{if(overlay.IsVisible&&e.Key==Key.Escape){cancelOverlay?.Invoke();e.Handled=true;}},RoutingStrategies.Tunnel);
        }
        Control Scope=>overlay.IsVisible?overlayScope:scope();
        IEnumerable<Control> Entries(){var area=Scope;if(area==null)return Enumerable.Empty<Control>();return area.GetVisualDescendants().OfType<Control>().Prepend(area).Where(c=>c.IsEffectivelyVisible&&c.IsEnabled&&c.Focusable&&c.Bounds.Width>0&&(c is Button||c is ToggleButton||c is Slider||c is TextBox||c is ComboBox||c is Expander||c is ListBoxItem||c is TabItem||c.Name=="BindingCaptureArea"));}
        void Poll(){
            var now=DateTime.UtcNow;if((now-lastPoll).TotalSeconds>2)state.Reset();lastPoll=now;
            bool active=window.IsActive&&window.WindowState!=WindowState.Minimized&&enabled();
            if(!active){state.Reset();outline.IsVisible=false;return;}
            try{
                if(pad==null&&watch.ElapsedMilliseconds>=retryAt){pad=new DeckGamepad(root);retryAt=watch.ElapsedMilliseconds+3000;}
                int buttons,x,y,scroll;if(pad==null||!pad.Read(out buttons,out x,out y,out scroll)){inputAvailable=false;state.Reset();if(watch.ElapsedMilliseconds>=retryAt){pad?.Dispose();pad=null;}return;}
                if(padGeneration!=pad.Generation){padGeneration=pad.Generation;state.Reset();}
                inputAvailable=true;var command=state.Read(buttons,x,y,scroll,true,watch.ElapsedMilliseconds);if(command!=PadCommand.None)Handle(command);
            }catch(Exception ex){LauncherLog.Write(root,"Handheld input unavailable: "+ex.GetType().Name);pad?.Dispose();pad=null;state.Reset();retryAt=watch.ElapsedMilliseconds+3000;}
        }
        internal void Handle(PadCommand command){
            var focused=window.FocusManager?.GetFocusedElement() as Control;gamepadFocus=true;
            if(command==PadCommand.Back){if(openCombo!=null){openCombo.IsDropDownOpen=false;openCombo=null;}else if(overlay.IsVisible)cancelOverlay?.Invoke();else back();FocusOutline();return;}
            if(openCombo!=null){if(command==PadCommand.Up||command==PadCommand.Down)openCombo.SelectedIndex=Math.Clamp(openCombo.SelectedIndex+(command==PadCommand.Down?1:-1),0,Math.Max(0,openCombo.ItemCount-1));else if(command==PadCommand.Accept){openCombo.IsDropDownOpen=false;openCombo.Focus();openCombo=null;}return;}
            var allowed=Entries().ToArray();if(focused==null||!allowed.Contains(focused)){focused=allowed.FirstOrDefault();focused?.Focus();}
            if(command==PadCommand.Keyboard||(command==PadCommand.Accept&&focused is TextBox)){if(focused is TextBox input)Keyboard(input);else if(focused?.Name=="BindingCaptureArea")CaptureKey(focused);return;}
            if(command==PadCommand.Accept){
                if(focused is ToggleButton toggle)toggle.IsChecked=toggle.IsChecked!=true;
                else if(focused is Button button)button.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
                else if(focused is Expander expander)expander.IsExpanded=!expander.IsExpanded;
                else if(focused is ComboBox combo){openCombo=combo;combo.IsDropDownOpen=true;}
                else Entries().FirstOrDefault()?.Focus();FocusOutline();return;
            }
            if((command==PadCommand.Left||command==PadCommand.Right)&&focused is Slider slider){slider.Value=Math.Clamp(slider.Value+(command==PadCommand.Right?1:-1)*slider.SmallChange,slider.Minimum,slider.Maximum);return;}
            if(command==PadCommand.ScrollUp||command==PadCommand.ScrollDown){var scroll=focused?.GetVisualAncestors().OfType<ScrollViewer>().FirstOrDefault()??Scope?.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault(s=>s.IsEffectivelyVisible);if(scroll!=null)scroll.Offset=new Vector(scroll.Offset.X,Math.Max(0,scroll.Offset.Y+(command==PadCommand.ScrollDown?48:-48)));return;}
            var entries=Entries().ToArray();if(entries.Length==0)return;int index=Array.IndexOf(entries,focused);Control next=null;
            bool previous=command==PadCommand.Previous||command==PadCommand.Up||command==PadCommand.Left;
            if(index>=0&&command!=PadCommand.Next&&command!=PadCommand.Previous){var origin=focused.TranslatePoint(new Point(focused.Bounds.Width/2,focused.Bounds.Height/2),host);if(origin.HasValue){double best=double.MaxValue;foreach(var item in entries.Where(c=>c!=focused)){var p=item.TranslatePoint(new Point(item.Bounds.Width/2,item.Bounds.Height/2),host);if(!p.HasValue)continue;double dx=p.Value.X-origin.Value.X,dy=p.Value.Y-origin.Value.Y;bool horizontal=command==PadCommand.Left||command==PadCommand.Right;double forward=horizontal?dx:dy;if(previous)forward=-forward;if(forward<2)continue;double perpendicular=horizontal?dy:dx,score=forward*forward+perpendicular*perpendicular*4;if(score<best){best=score;next=item;}}}}
            if(next==null)next=entries[index<0?0:(index+(previous?-1:1)+entries.Length)%entries.Length];next.Focus(NavigationMethod.Directional);next.BringIntoView();FocusOutline();
        }
        void FocusOutline(){var target=window.FocusManager?.GetFocusedElement() as Control;var point=target?.TranslatePoint(new Point(0,0),host);if(target==null||!target.IsEffectivelyVisible||!point.HasValue||!window.IsActive){outline.IsVisible=false;return;}outline.Width=target.Bounds.Width+4;outline.Height=target.Bounds.Height+4;outline.Margin=new Thickness(Math.Max(0,point.Value.X-2),Math.Max(0,point.Value.Y-2),0,0);outline.IsVisible=true;}
        Button Action(Panel parent,string text,Action action){var b=new Button {Content=text,MinHeight=44,Padding=new Thickness(10,8),HorizontalContentAlignment=HorizontalAlignment.Center,HorizontalAlignment=HorizontalAlignment.Stretch,Margin=new Thickness(3)};PageTheme.Secondary(b);b.Click+=(s,e)=>action();parent.Children.Add(b);return b;}
        void Show(Control content,Action cancel){overlay.Child=content;overlayScope=content;cancelOverlay=cancel;overlay.IsVisible=true;Ui.Theme(overlay);LinuxHandheld.Style(content);KeyboardNavigation.SetTabNavigation(overlay,KeyboardNavigationMode.Cycle);window.LayoutUpdated+=InitialFocus;}
        void InitialFocus(object sender,EventArgs e){window.LayoutUpdated-=InitialFocus;Entries().FirstOrDefault()?.Focus();}
        void Hide(){overlay.IsVisible=false;overlay.Child=null;overlayScope=null;cancelOverlay=null;state.Reset();}
        void CaptureKey(Control target){
            if(overlay.IsVisible)return;var frame=new DockPanel();var title=PageTheme.Text("Choose a keyboard key",24);DockPanel.SetDock(title,Dock.Top);frame.Children.Add(title);var footer=new StackPanel();DockPanel.SetDock(footer,Dock.Bottom);frame.Children.Add(footer);Action(footer,"Cancel",()=>{Hide();target.Focus();});
            var keys=new UniformGrid {Columns=10};frame.Children.Add(keys);Action<Key> choose=key=>{Hide();target.Focus();target.RaiseEvent(new KeyEventArgs {RoutedEvent=InputElement.KeyDownEvent,Key=key});};
            foreach(char c in "1234567890QWERTYUIOPASDFGHJKLZXCVBNM"){Key key;if(Enum.TryParse<Key>(Char.IsDigit(c)?"D"+c:c.ToString(),out key))Action(keys,c.ToString(),()=>choose(key));}Action(keys,"Space",()=>choose(Key.Space));Action(keys,"Enter",()=>choose(Key.Enter));Action(keys,"Tab",()=>choose(Key.Tab));Show(frame,()=>{Hide();target.Focus();});
        }
        void Keyboard(TextBox input){
            if(overlay.IsVisible||input.IsReadOnly)return;var saved=input.Text??"";var edit=new TextBox {Text=saved,MinHeight=44,Margin=new Thickness(3,8),Watermark="Enter text"};var frame=new DockPanel();var title=PageTheme.Text("Enter text",24);DockPanel.SetDock(title,Dock.Top);frame.Children.Add(title);
            var footer=new UniformGrid {Rows=1,Columns=5};DockPanel.SetDock(footer,Dock.Bottom);frame.Children.Add(footer);
            var preview=new Border {Child=edit};DockPanel.SetDock(preview,Dock.Top);frame.Children.Add(preview);
            bool upper=false;var keys=new UniformGrid {Columns=10};frame.Children.Add(keys);Action keysFill=null;keysFill=()=>{keys.Children.Clear();foreach(char raw in "1234567890qwertyuiopasdfghjkl-zxcvbnm_./@:"){char key=upper?Char.ToUpperInvariant(raw):raw;Action(keys,key.ToString(),()=>{edit.Text=(edit.Text??"")+key;});}};keysFill();
            Action(footer,"Cancel",()=>{Hide();input.Focus();});Action(footer,"Shift",()=>{upper=!upper;keysFill();});Action(footer,"Space",()=>edit.Text=(edit.Text??"")+" ");Action(footer,"⌫",()=>{if(!String.IsNullOrEmpty(edit.Text))edit.Text=edit.Text.Substring(0,edit.Text.Length-1);});Action(footer,"Done",()=>{input.Text=edit.Text;input.CaretIndex=(input.Text??"").Length;Hide();input.Focus();});
            Show(frame,()=>{Hide();input.Focus();});
        }
        public void Help(){
            if(overlay.IsVisible)return;var frame=new DockPanel();var title=PageTheme.Text("Steam Deck guide",25);DockPanel.SetDock(title,Dock.Top);frame.Children.Add(title);var footer=new StackPanel();DockPanel.SetDock(footer,Dock.Bottom);frame.Children.Add(footer);Action(footer,"Back",Hide);
            var body=new StackPanel {Spacing=14};foreach(var text in new[]{"SteamOS support is experimental. Physical Steam Deck testing is still pending.","Gaming Mode fills the screen. Desktop Mode keeps a normal window and remembers its size and position.","Use the D-pad or left stick to move, A to select, and B to go back. LB/RB move between controls; the right stick scrolls. Touch and the trackpads work too.","For controller navigation, launch from a non-Steam entry with a Gamepad Steam Input layout. Add TimeSplitters.Launcher as the target and keep its installation folder in the same place. This is a native Linux launcher; leave Proton compatibility off.","Press Y on a text field to open the built-in keyboard. Press Y on the keyboard-binding capture area to choose a key without an external keyboard.","Setup can browse your home folder, Downloads and mounted SD cards without opening another desktop window. Your supported US Xbox game data is still required.","Saves can use a folder you choose. If an SD card or drive is missing, reconnect it before playing; the launcher does not replace your saved progress.","Keep the launcher open while installing updates. It stays running during the game and becomes ready again after the game closes.","Steam and the Quick Access button remain controlled by SteamOS. Controller navigation pauses when this launcher is inactive or the game is running."})body.Children.Add(PageTheme.Text(text,15));frame.Children.Add(new ScrollViewer {Content=body,Margin=new Thickness(0,16),HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled});Show(frame,Hide);
        }
        public Task<string> PickGameData(bool folder,string start=null){return PickPath(folder,true,folder?"Choose your extracted game folder":"Choose your US Xbox ISO",start);}
        public Task<string> PickFolder(string title,string start=null){return PickPath(true,false,title,start);}
        Task<string> PickPath(bool folder,bool requireGameData,string title,string start){
            var completion=new TaskCompletionSource<string>();var frame=new DockPanel();var top=new StackPanel();DockPanel.SetDock(top,Dock.Top);frame.Children.Add(top);top.Children.Add(PageTheme.Text(title,24));var location=PageTheme.Text("",14);top.Children.Add(location);
            var shortcuts=new UniformGrid {Rows=1,Columns=4};top.Children.Add(shortcuts);var footer=new UniformGrid {Rows=1,Columns=2};DockPanel.SetDock(footer,Dock.Bottom);frame.Children.Add(footer);var rows=new StackPanel();var scroll=new ScrollViewer {Content=rows,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};frame.Children.Add(scroll);
            string current=start??Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);Action<string> browse=null;Action<string> finish=path=>{Hide();completion.TrySetResult(path);};
            browse=path=>{try{var dir=new DirectoryInfo(path);if(!dir.Exists){location.Text="This location is not available.";return;}var dirs=dir.EnumerateDirectories().Where(d=>!d.Name.StartsWith(".")).OrderBy(d=>d.Name,StringComparer.OrdinalIgnoreCase).ToArray();var files=folder?Array.Empty<FileInfo>():dir.EnumerateFiles().Where(f=>f.Extension.Equals(".iso",StringComparison.OrdinalIgnoreCase)).OrderBy(f=>f.Name,StringComparer.OrdinalIgnoreCase).ToArray();current=dir.FullName;location.Text=current;rows.Children.Clear();foreach(var d in dirs)Action(rows,"▸ "+d.Name,()=>browse(d.FullName));foreach(var f in files)Action(rows,f.Name,()=>finish(f.FullName));if(dirs.Length+files.Length==0)rows.Children.Add(PageTheme.Text("No "+(folder?"subfolders":"ISO files or folders")+" here. Choose another location or go up a folder.",14));scroll.Offset=new Vector();}catch(Exception ex){location.Text="Couldn’t open this folder: "+ex.Message;}};
            Action(shortcuts,"Home",()=>browse(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile)));Action(shortcuts,"Downloads",()=>browse(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),"Downloads")));Action(shortcuts,"Drives / SD card",()=>browse(Directory.Exists("/run/media")?"/run/media":"/media"));Action(shortcuts,"Up a folder",()=>browse(Directory.GetParent(current)?.FullName??"/"));
            Action(footer,"Cancel",()=>finish(null));if(folder)Action(footer,"Use this folder",()=>{if(!requireGameData||(File.Exists(Path.Combine(current,"default.xbe"))&&Directory.Exists(Path.Combine(current,"pak"))&&Directory.Exists(Path.Combine(current,"xmv"))))finish(current);else location.Text="Choose the folder containing default.xbe, pak and xmv.";});
            Show(frame,()=>finish(null));browse(current);return completion.Task;
        }
        public void Dispose(){if(disposed)return;disposed=true;timer.Stop();pad?.Dispose();pad=null;cancelOverlay?.Invoke();}
    }
}
#endif
