using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Media;
namespace TimeSplitters.Launcher
{
    // WPF document adapter; parsing and link policy are shared with the Mac host.
    public sealed class ReleaseNotesMarkdown : RichTextBox
    {
        readonly Action<string> openLink;
        public ReleaseNotesMarkdown(Action<string> openLink=null){this.openLink=openLink;IsReadOnly=true;IsDocumentEnabled=true;Background=Brushes.Transparent;Foreground=PageTheme.Brush("#CCDCE8");BorderThickness=new Thickness(0);Padding=new Thickness(0);FontSize=12;VerticalScrollBarVisibility=ScrollBarVisibility.Auto;HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled;SetMarkdown("");}
        public static bool TryLink(string value,out Uri uri){return ReleaseNotesDocument.TryLink(value,out uri);}
        public void SetMarkdown(string source){
            var document=new FlowDocument{PagePadding=new Thickness(0),FontFamily=Ui.Serif,FontSize=12,Foreground=Foreground};
            System.Windows.Documents.List list=null;int group=-1;
            foreach(var b in ReleaseNotesDocument.Parse(source)){
                var p=new Paragraph{Margin=new Thickness(0,0,0,10),LineHeight=20};
                if(b.Kind=="code"){p.Inlines.Add(new Run(b.Text));p.FontFamily=new FontFamily("Consolas");p.Background=PageTheme.Brush("#0B1826");p.Padding=new Thickness(10);p.Margin=new Thickness(0,4,0,12);}
                else Inline(p.Inlines,b.Inlines);
                if(b.Kind=="heading"){p.FontSize=b.Level==1?20:b.Level==2?17:15;p.FontWeight=FontWeights.SemiBold;p.Foreground=PageTheme.Brush("#CDE8FC");p.Margin=new Thickness(0,10,0,10);}
                if(b.Kind=="item")p.Margin=new Thickness(8+b.Indent*4,0,0,5);
                if(b.Kind=="quote"){p.BorderBrush=PageTheme.Brush("#86CDFF");p.BorderThickness=new Thickness(2,0,0,0);p.Padding=new Thickness(10,3,0,3);}
                if(b.Kind=="rule"){p.BorderBrush=PageTheme.Brush("#2D455C");p.BorderThickness=new Thickness(0,0,0,1);p.Margin=new Thickness(0,8,0,12);}
                if(b.Kind=="item"){
                    if(list==null||group!=b.Group){list=new System.Windows.Documents.List{MarkerStyle=b.Marker=="•"?TextMarkerStyle.Disc:TextMarkerStyle.Decimal,Padding=new Thickness(23+b.Indent*4,0,0,0),Margin=new Thickness(0,0,0,8)};int start;if(Int32.TryParse(b.Marker.TrimEnd('.'),out start)&&start>0)list.StartIndex=start;document.Blocks.Add(list);group=b.Group;}
                    p.Margin=new Thickness(0,0,0,5);list.ListItems.Add(new ListItem(p));
                }else {list=null;document.Blocks.Add(p);}
            }Document=document;ScrollToHome();
        }
        void Inline(InlineCollection target,IEnumerable<NotesInline> nodes){foreach(var n in nodes){
            if(n.Kind=="text"||n.Kind=="code"){var r=new Run(n.Text);if(n.Kind=="code"){r.FontFamily=new FontFamily("Consolas");r.Background=PageTheme.Brush("#0B1826");}target.Add(r);continue;}
            Span span=n.Kind=="bold"?(Span)new Bold():n.Kind=="italic"?new Italic():new Span();
            if(n.Kind=="strike")span.TextDecorations=TextDecorations.Strikethrough;
            if(n.Kind=="link"){var link=new Hyperlink{NavigateUri=new Uri(n.Url),ToolTip=n.Url,Foreground=PageTheme.Brush("#86CDFF"),Focusable=true};link.RequestNavigate+=(s,e)=>{e.Handled=true;Uri u;if(TryLink(e.Uri.AbsoluteUri,out u)&&openLink!=null)openLink(u.AbsoluteUri);};span=link;}
            Inline(span.Inlines,n.Children);target.Add(span);
        }}
    }
}
