using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

#if MACOS
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Automation;
using UIElement = Avalonia.Controls.Control;
using Brush = Avalonia.Media.IBrush;
using FontWeights = Avalonia.Media.FontWeight;
#else
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
#endif

namespace TimeSplitters.Launcher
{
    public sealed class GameVersionsPage
    {
        public readonly Grid View = new Grid();
        public readonly ComboBox Versions = new ComboBox { MinWidth=240 };
        public readonly Button Install, Older, Refresh, OnGitHub;
        readonly TextBlock current, details, status;
        readonly Border installedBadge,releaseBadge;
        readonly ReleaseNotesMarkdown notes;
        readonly Dictionary<string,Release> releases = new Dictionary<string,Release>(StringComparer.Ordinal);
        string installed = "";
        bool blocked, loading, more, running;
        public Release Selected { get { var item=Versions.SelectedItem as ComboBoxItem;return item==null?null:item.Tag as Release; } }
        public string StatusText { get { return status.Text; } }
        public int Count { get { return releases.Count; } }
        public GameVersionsPage(Action<string> openLink=null)
        {
            View.Margin=new Thickness(30,24,30,20);Ui.Theme(View);
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            View.RowDefinitions.Add(new RowDefinition {Height=new GridLength(1,GridUnitType.Star)});
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            var header=PageTheme.Header("TESTING TOOLS","Game versions","Install a specific published release for comparisons and bug reproduction.");
            var installation=new WrapPanel {Margin=new Thickness(0,0,0,12)};header.Children.Add(installation);
            installedBadge=PageTheme.Badge("Not installed",StatusTone.Neutral);installedBadge.Margin=new Thickness(0,0,10,0);installation.Children.Add(installedBadge);
            current=PageTheme.Text("",13,"#C7DAE9");current.VerticalAlignment=VerticalAlignment.Center;installation.Children.Add(current);
            var selector=new Grid {Margin=new Thickness(0,0,0,12)};
            selector.ColumnDefinitions.Add(new ColumnDefinition());selector.ColumnDefinitions.Add(new ColumnDefinition {Width=GridLength.Auto});
            AutomationProperties.SetName(Versions,"Published game version");selector.Children.Add(Versions);
            Older=new Button {Content="Load older releases",Margin=new Thickness(10,0,0,0)};Grid.SetColumn(Older,1);selector.Children.Add(Older);header.Children.Add(selector);
            releaseBadge=PageTheme.Badge("Select a release",StatusTone.Neutral);releaseBadge.Margin=new Thickness(0,0,0,10);header.Children.Add(releaseBadge);
            details=PageTheme.Text("Refresh to load stable and preview releases with verified packages for this platform.",12,"#9EB4C8");details.Margin=new Thickness(0,0,0,14);header.Children.Add(details);View.Children.Add(header);
            var body=new Grid();body.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});body.RowDefinitions.Add(new RowDefinition());
            body.Children.Add(PageTheme.Text("RELEASE NOTES",10,"#9EB4C8"));
            notes=new ReleaseNotesMarkdown(openLink) {Margin=new Thickness(0,12,0,0)};
            AutomationProperties.SetName(notes,"Selected game release notes");Grid.SetRow(notes,1);body.Children.Add(notes);
            var card=PageTheme.Card(body);Grid.SetRow(card,1);View.Children.Add(card);
            var footer=new StackPanel {Margin=new Thickness(0,14,0,0)};Grid.SetRow(footer,2);View.Children.Add(footer);
            footer.Children.Add(PageTheme.Text("Switches the installed game only. Saves, settings, imported data and the launcher are kept. Back up saves before testing older builds: save compatibility can vary.",12,"#B4C9DA"));
            var actions=new WrapPanel {Margin=new Thickness(0,13,0,0)};footer.Children.Add(actions);
            Install=Action("Install selected version");PageTheme.Primary(Install);OnGitHub=Action("View release ↗");Refresh=Action("Refresh list");
            actions.Children.Add(Install);actions.Children.Add(OnGitHub);actions.Children.Add(Refresh);
            status=PageTheme.Text("Load the list to choose a release.",12,"#9EB4C8");footer.Children.Add(status);
            Versions.SelectionChanged+=(s,e)=>Render();Render();
        }
        static Button Action(string label) {return new Button {Content=label,Margin=new Thickness(0,0,10,10)};}
        public void SetInstalled(string version)
        {installed=version??"";current.Text=installed.Length==0?"No game version recorded":installed;PageTheme.SetBadge(installedBadge,installed.Length==0?"Not installed":"Installed",installed.Length==0?StatusTone.Neutral:StatusTone.Success);Render();}
        public void SetBusy(bool value,bool gameRunning)
        {blocked=value;running=gameRunning;Render();}
        public void SetLoading(bool value)
        {loading=value;if(value)status.Text="Loading published game releases…";Render();}
        public void SetReleases(GameReleaseList page,bool reset)
        {
            string selection=Selected==null?installed:Selected.Tag;
            if(reset)releases.Clear();
            foreach(var release in page.Releases)releases[release.Tag]=release;
            more=page.HasMore;Versions.Items.Clear();
            foreach(var release in releases.Values.OrderByDescending(r=>GameVersion.Parse(r.Tag))) {
                var item=new ComboBoxItem {Content=release.Tag+(release.IsPreview?"  ·  Preview":"  ·  Stable"),Tag=release};Versions.Items.Add(item);
                if(release.Tag==selection)Versions.SelectedItem=item;
            }
            if(Versions.SelectedIndex<0 && Versions.Items.Count>0)Versions.SelectedIndex=0;
            status.Text=releases.Count==0?"No compatible game packages for this platform found on this page. Load older releases if available.":"Showing "+releases.Count+" published game versions. Choose one to install.";
            loading=false;Render();
        }
        public void Failed()
        {loading=false;status.Text="Could not load releases. Check your connection or GitHub request limit, then retry. Previously loaded versions and installed files were kept.";Render();}
        public void Status(string text) {status.Text=text;}
        void Render()
        {
            var release=Selected;
            Versions.IsEnabled=!blocked && !loading;
            Older.IsEnabled=more && !blocked && !loading;Refresh.IsEnabled=!blocked && !loading;
            Install.IsEnabled=release!=null && !blocked && !loading && !running;OnGitHub.IsEnabled=release!=null;
            Install.Content=release!=null && release.Tag==installed?"Reinstall selected version":"Install selected version";
            Ui.Tip(Install,running?"Close the game before switching versions.":"Review and confirm the selected game version before downloading.");
            PageTheme.SetBadge(releaseBadge,release==null?"Select a release":release.IsPreview?"Preview":"Stable release",
                release==null?StatusTone.Neutral:release.IsPreview?StatusTone.Warning:StatusTone.Info);
            if(release==null) {notes.SetMarkdown("");return;}
            DateTime date;string published=DateTime.TryParse(release.Published,CultureInfo.InvariantCulture,DateTimeStyles.RoundtripKind,out date)?date.ToUniversalTime().ToString("yyyy-MM-dd",CultureInfo.InvariantCulture):"Date unavailable";
            details.Text=release.Tag+"  ·  "+release.ChannelName+"  ·  "+published+"  ·  "+(release.Size/1048576.0).ToString("0.0",CultureInfo.InvariantCulture)+" MB";
            string text=String.IsNullOrWhiteSpace(release.Notes)?"No release notes were provided.":release.Notes;
            notes.SetMarkdown(text.Length>24000?text.Substring(0,24000)+"\n\nNotes shortened. View the release on GitHub for the full text.":text);
        }
    }
}
