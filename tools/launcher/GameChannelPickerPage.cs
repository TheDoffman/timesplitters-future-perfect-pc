using System;
using System.Linq;
#if MACOS
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Automation;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Automation;
#endif

namespace TimeSplitters.Launcher
{
    // Both hosts use the same grouping, remembered track and selection events.
    public sealed class GameChannelPickerPage
    {
        public readonly StackPanel View=new StackPanel();
        public readonly ComboBox Channel=new ComboBox {MinWidth=120,FontSize=13};
        public readonly ComboBox Track=new ComboBox {MinWidth=160,MaxWidth=260,FontSize=13};
        readonly Grid trackRow;
        bool updating;
        string lastPreview="beta";
        public event Action Changed;
        public string Selected {get{return Channel.SelectedIndex==1?LastPreview:"stable";}}
        public string LastPreview {get{return lastPreview;}}
        public bool ShowingTracks {get{return Channel.SelectedIndex==1;}}

        public GameChannelPickerPage()
        {
            Ui.Theme(View);
            Channel.Name="ChannelPicker";Track.Name="PreviewTrackPicker";
            AutomationProperties.SetName(Channel,"Game update channel");
            AutomationProperties.SetName(Track,"Preview testing track");
            Ui.Tip(Channel,"Stable is the default. Choose Preview to opt into testing.");
            Ui.Tip(Track,"Updates stay within the selected testing track.");
            Channel.Items.Add("Stable");Channel.Items.Add("Preview");
            View.Children.Add(Row("UPDATE CHANNEL",Channel));
            trackRow=Row("TESTING TRACK",Track);View.Children.Add(trackRow);
            SetReleases(new Release[0],"stable","beta");
            Channel.SelectionChanged+=(s,e)=>Notify();
            Track.SelectionChanged+=(s,e)=>{
                if(updating)return;
                var track=Track.SelectedItem as GameChannelChoice;
                if(track!=null)lastPreview=track.Id;
                Notify();
            };
        }
        static Grid Row(string label,ComboBox picker)
        {
            var row=new Grid {Margin=new Thickness(0,0,0,8)};
            row.ColumnDefinitions.Add(new ColumnDefinition {Width=new GridLength(1,GridUnitType.Star)});
            row.ColumnDefinitions.Add(new ColumnDefinition {Width=GridLength.Auto});
            var text=PageTheme.Text(label,10,"#9EB4C8");text.VerticalAlignment=VerticalAlignment.Center;
            text.Margin=new Thickness(0,0,12,0);row.Children.Add(text);
            Grid.SetColumn(picker,1);row.Children.Add(picker);return row;
        }
        void Notify()
        {
            Ui.Show(trackRow,ShowingTracks);
            if(!updating && Changed!=null)Changed();
        }
        public void SetReleases(Release[] releases,string selected,string rememberedPreview=null)
        {
            updating=true;
            try {
                selected=GameChannels.Preference(selected);rememberedPreview=GameChannels.Preference(rememberedPreview);
                if(selected!="stable")lastPreview=selected;
                else if(GameChannels.Valid(rememberedPreview) && rememberedPreview!="stable")lastPreview=rememberedPreview;
                var tracks=GameChannels.Choices(releases,lastPreview).Where(c=>c.Id!="stable")
                    .Select(c=>new GameChannelChoice {Id=c.Id,Name=c.Id=="beta"?"Beta":
                        c.Name.EndsWith(" Preview",StringComparison.Ordinal)?c.Name.Substring(0,c.Name.Length-8):c.Name}).ToArray();
                Track.ItemsSource=tracks;Track.SelectedItem=tracks.First(c=>c.Id==lastPreview);
                Channel.SelectedIndex=selected=="stable"?0:1;
                Ui.Show(trackRow,ShowingTracks);
            } finally {updating=false;}
        }
        public void SetEnabled(bool enabled) {Channel.IsEnabled=enabled;Track.IsEnabled=enabled;}
    }
}
