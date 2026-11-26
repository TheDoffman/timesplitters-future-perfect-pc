using System;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Documents;
using Avalonia.Layout;
using Avalonia.Media;
namespace TimeSplitters.Launcher
{
    public static class NativeMarkdown
    {
        public static void Render(StackPanel panel,string markdown,Action<string> openLink=null){foreach(var b in ReleaseNotesDocument.Parse(markdown)){
            if(b.Kind=="rule"){panel.Children.Add(new Border{Height=1,Background=Brush.Parse("#294E70"),Margin=new Thickness(0,8,0,12)});continue;}
            var text=new SelectableTextBlock{FontSize=12,LineHeight=20,TextWrapping=TextWrapping.Wrap,Foreground=Brush.Parse("#CCDCE8"),Margin=new Thickness(0,0,0,10)};
            if(b.Kind=="code"){text.Text=b.Text;text.FontFamily=new FontFamily("Menlo");panel.Children.Add(new Border{Background=Brush.Parse("#08182B"),Padding=new Thickness(10),Margin=new Thickness(0,4,0,12),Child=text});continue;}
            if(b.Kind=="item")text.Inlines.Add(new Run(b.Marker+"  "));Inline(text.Inlines,b.Inlines,openLink);
            if(b.Kind=="heading"){text.FontSize=b.Level==1?20:b.Level==2?17:15;text.FontWeight=FontWeight.SemiBold;text.Foreground=Brush.Parse("#CDE8FC");text.Margin=new Thickness(0,10,0,10);}
            if(b.Kind=="item")text.Margin=new Thickness(8+b.Indent*4,0,0,5);
            if(b.Kind=="quote")panel.Children.Add(new Border{BorderBrush=Brush.Parse("#65C7FF"),BorderThickness=new Thickness(2,0,0,0),Padding=new Thickness(10,3,0,3),Child=text});else panel.Children.Add(text);
        }}
        static void Inline(InlineCollection target,IEnumerable<NotesInline> nodes,Action<string> openLink){foreach(var n in nodes){
            if(n.Kind=="text"||n.Kind=="code"){var r=new Run(n.Text);if(n.Kind=="code"){r.FontFamily=new FontFamily("Menlo");r.Background=Brush.Parse("#08182B");}target.Add(r);continue;}
            var span=new Span();if(n.Kind=="bold")span.FontWeight=FontWeight.Bold;if(n.Kind=="italic")span.FontStyle=FontStyle.Italic;if(n.Kind=="strike")span.TextDecorations=TextDecorations.Strikethrough;
            if(n.Kind=="link"){
                var label=new TextBlock{Foreground=Brush.Parse("#65C7FF"),TextDecorations=TextDecorations.Underline};Inline(label.Inlines,n.Children,openLink);
                var link=new Button{Content=label,Padding=new Thickness(0),Margin=new Thickness(0),MinHeight=0,Background=Brushes.Transparent,BorderThickness=new Thickness(0),VerticalAlignment=VerticalAlignment.Center};ToolTip.SetTip(link,n.Url);
                link.Click+=(s,e)=>{Uri uri;if(ReleaseNotesDocument.TryLink(n.Url,out uri)&&openLink!=null)openLink(uri.AbsoluteUri);};target.Add(new InlineUIContainer(link));continue;
            }Inline(span.Inlines,n.Children,openLink);target.Add(span);
        }}
    }
}
