using System;
using System.Globalization;

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
using System.Windows.Documents;
#endif

namespace TimeSplitters.Launcher
{
    public sealed class WhatsNewPage
    {
        public readonly Grid View=new Grid();
        public readonly Button Refresh, OnGitHub, GameTab, LauncherTab, RefreshIssues, BugTracker;
        public readonly ComboBox Edition=new ComboBox { MinWidth=190,Margin=new Thickness(0,0,0,10) };
        readonly TextBlock status,installedLabel,availableLabel,availability,channel,title,published;
        readonly StackPanel changes=new StackPanel(),publishedIssues=new StackPanel(),issues=new StackPanel();
        readonly TextBlock issuesStatus,issuesCount;
        KnownIssues knownIssues;
        readonly ScrollViewer scroll;
        readonly Action<string> openLink;
        ReleaseNews news;
        string installedGame="",installedLauncher=LauncherVersion.Value;
        bool launcher;
        public string SelectedUrl { get; private set; }
        public string StatusText { get { return status.Text; } }
        public string AvailabilityText { get { return availability.Text; } }
        public string IssuesStatusText { get { return issuesStatus.Text; } }
        static Button Button(string text,string name) { return new Button { Content=text,Name=name,Margin=new Thickness(0,0,10,10) }; }
        public WhatsNewPage(Action<string> openLink=null)
        {
            this.openLink=openLink;
            View.Margin=new Thickness(30,24,30,20);Ui.Theme(View);
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            View.RowDefinitions.Add(new RowDefinition {Height=new GridLength(1,GridUnitType.Star)});
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            var header=new StackPanel();var eyebrow=PageTheme.Text("RELEASE CENTER",10,"#86CDFF");eyebrow.FontWeight=FontWeights.SemiBold;header.Children.Add(eyebrow);
            var heading=PageTheme.Text("What's new",28);heading.Margin=new Thickness(0,5,0,7);header.Children.Add(heading);
            header.Children.Add(PageTheme.Text("Published changes for your game and launcher, all in one place.",12,"#9EB4C8"));
            var toolbar=new WrapPanel {Margin=new Thickness(0,20,0,6)};header.Children.Add(toolbar);
            GameTab=Button("Game releases","NewsGameTab");LauncherTab=Button("Launcher releases","NewsLauncherTab");
            toolbar.Children.Add(GameTab);toolbar.Children.Add(LauncherTab);toolbar.Children.Add(Edition);
            AutomationProperties.SetName(Edition,"Release notes version");Edition.Items.Add("Available release");Edition.Items.Add("Installed release");Edition.SelectedIndex=0;
            GameTab.Click+=(s,e)=>SelectComponent(false);LauncherTab.Click+=(s,e)=>SelectComponent(true);Edition.SelectionChanged+=(s,e)=>Render();
            View.Children.Add(header);
            var body=new StackPanel();scroll=new ScrollViewer {Content=body,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};Grid.SetRow(scroll,1);View.Children.Add(scroll);
            var summary=PageTheme.Section(body,"Version overview","");
            channel=PageTheme.Text("",11,"#86CDFF");channel.Margin=new Thickness(0,8,0,14);summary.Children.Add(channel);
            var versions=new Grid();versions.ColumnDefinitions.Add(new ColumnDefinition());versions.ColumnDefinitions.Add(new ColumnDefinition());summary.Children.Add(versions);
            var installed=new StackPanel();installed.Children.Add(PageTheme.Text("INSTALLED",10,"#9EB4C8"));installedLabel=PageTheme.Text("",21);installed.Children.Add(installedLabel);versions.Children.Add(installed);
            var available=new StackPanel();available.Children.Add(PageTheme.Text("AVAILABLE",10,"#9EB4C8"));availableLabel=PageTheme.Text("Not checked",21);available.Children.Add(availableLabel);Grid.SetColumn(available,1);versions.Children.Add(available);
            availability=PageTheme.Text("Refresh to check published releases.",12,"#B4C9DA");availability.Margin=new Thickness(0,14,0,0);summary.Children.Add(availability);
            var notes=PageTheme.Section(body,"What this release changes","");
            title=PageTheme.Text("Release notes",18,"#A7D9FA");title.Margin=new Thickness(0,12,0,4);notes.Children.Add(title);
            published=PageTheme.Text("",11,"#9EB4C8");published.Margin=new Thickness(0,0,0,14);notes.Children.Add(published);notes.Children.Add(changes);
            notes.Children.Add(publishedIssues);
            var known=PageTheme.Section(body,"Known issues","Current open GitHub reports labeled bug, across all versions.");
            issuesCount=PageTheme.Text("Not checked",13,"#A7D9FA");known.Children.Add(issuesCount);
            issues.Margin=new Thickness(0,10,0,0);known.Children.Add(issues);
            issuesStatus=PageTheme.Text("Open this page to check the bug tracker.",12,"#B4C7D8");issuesStatus.Margin=new Thickness(0,8,0,8);known.Children.Add(issuesStatus);
            var issueActions=new WrapPanel();known.Children.Add(issueActions);
            RefreshIssues=Button("Refresh issues","RefreshKnownIssues");BugTracker=Button("View bug tracker ↗","KnownIssuesOnGitHub");
            issueActions.Children.Add(RefreshIssues);issueActions.Children.Add(BugTracker);
            Ui.Tip(BugTracker,KnownIssues.TrackerUrl);BugTracker.Click+=(s,e)=>{if(this.openLink!=null)this.openLink(KnownIssues.TrackerUrl);};
            var footer=new StackPanel {Margin=new Thickness(0,12,0,0)};Grid.SetRow(footer,2);View.Children.Add(footer);
            var actions=new WrapPanel();footer.Children.Add(actions);
            Refresh=Button("Refresh notes","RefreshReleaseNotes");OnGitHub=Button("View release on GitHub ↗","NewsOnGitHub");
            actions.Children.Add(Refresh);actions.Children.Add(OnGitHub);
            Ui.Tip(OnGitHub,"Opens this published release in your browser.");
            status=PageTheme.Text("Refresh to load published release notes.",12,"#9EB4C8");footer.Children.Add(status);
            Render();
        }
        public void SelectComponent(bool value)
        {
            launcher=value;
            Edition.SelectedIndex=news!=null && (launcher?news.Launcher:news.Game)==null && (launcher?news.InstalledLauncher:news.InstalledGame)!=null?1:0;
            Render();Ui.ScrollHome(scroll);
        }
        public void SetVersions(string game,string currentLauncher,bool preview)
        { installedGame=game??"";installedLauncher=currentLauncher;channel.Text=preview?"PREVIEW CHANNEL · includes published test builds":"STABLE CHANNEL · published stable releases";Render(); }
        public void SetVersions(string game,string currentLauncher,string gameChannel)
        {
            SetVersions(game,currentLauncher,gameChannel!="stable");
            channel.Text=GameChannels.Name(gameChannel)+" game channel; launcher updates are independent.";
        }
        public void Reset() { news=null;Refresh.IsEnabled=true;Edition.SelectedIndex=0;status.Text="Refresh to load published release notes.";Render(); }
        public void SetLoading(bool value) { Refresh.IsEnabled=!value; if(value)status.Text="Checking published release notes…"; }
        public void SetNews(ReleaseNews value)
        {
            news=value;Refresh.IsEnabled=true;
            status.Text="Checked "+value.CheckedUtc.ToLocalTime().ToString("g",CultureInfo.CurrentCulture)+" · Updates are installed from Home or launcher startup.";
            if(Edition.SelectedIndex==0 && (launcher?news.Launcher:news.Game)==null && (launcher?news.InstalledLauncher:news.InstalledGame)!=null)Edition.SelectedIndex=1;
            Render();
        }
        public void Failed()
        {
            Refresh.IsEnabled=true;
            status.Text=news==null?"Could not load release notes. Check your connection and refresh; your installed game remains available.":
                "Could not refresh. Showing notes checked "+news.CheckedUtc.ToLocalTime().ToString("g",CultureInfo.CurrentCulture)+"; available versions may have changed.";
        }
        public void SetIssuesLoading()
        { RefreshIssues.IsEnabled=false;issuesStatus.Text=knownIssues==null?"Checking open bug reports…":"Checking open bug reports… Showing the previous list until this check finishes."; }
        public void SetIssues(KnownIssues value)
        {
            knownIssues=value;RefreshIssues.IsEnabled=true;issues.Children.Clear();
            issuesCount.Text=value.Items.Length==1?"1 open bug report":value.Items.Length+" open bug reports";
            if(value.Items.Length==0)issues.Children.Add(PageTheme.Text("No open issues labeled bug were returned by GitHub. This does not mean every version is bug-free.",12,"#B4C7D8"));
            foreach(var issue in value.Items) {
                var row=PageTheme.Text("",13);row.Margin=new Thickness(0,0,0,10);
                // Titles are literal text, not Markdown/HTML. Ignore remote URLs;
                // links always target this repository's numbered issue page.
                string url=issue.Url;Ui.Link(row,"#"+issue.Number+"  "+issue.Title,url,openLink);issues.Children.Add(row);
            }
            issuesStatus.Text="Checked "+value.CheckedUtc.ToLocalTime().ToString("g",CultureInfo.CurrentCulture)+" · Automatically checks every 5 minutes while this page is visible.";
        }
        public void IssuesFailed()
        {
            RefreshIssues.IsEnabled=true;
            issuesStatus.Text=knownIssues==null?"Could not check GitHub. Check your connection or request limit, then refresh or view the bug tracker.":
                "Could not refresh GitHub. Showing the list checked "+knownIssues.CheckedUtc.ToLocalTime().ToString("g",CultureInfo.CurrentCulture)+"; reports may have changed. Refresh or view the bug tracker.";
        }
        void Render()
        {
            GameTab.Background=PageTheme.Brush(launcher?"#1C3349":"#294C69");LauncherTab.Background=PageTheme.Brush(launcher?"#294C69":"#1C3349");
            AutomationProperties.SetHelpText(GameTab,launcher?"Show game releases":"Selected: game releases");AutomationProperties.SetHelpText(LauncherTab,launcher?"Selected: launcher releases":"Show launcher releases");
            string version=launcher?installedLauncher:installedGame;installedLabel.Text=String.IsNullOrEmpty(version)?"Not installed":version;
            Release latest=news==null?null:(launcher?news.Launcher:news.Game);
            availableLabel.Text=news==null?"Not checked":latest==null?"None on channel":latest.Tag;
            availability.Text=news==null?"Refresh to check published releases.":ReleaseNews.Availability(version,latest,launcher);
            Release selected=Edition.SelectedIndex==1?(news==null?null:(launcher?news.InstalledLauncher:news.InstalledGame)):latest;
            SelectedUrl=selected==null?null:selected.Url;OnGitHub.IsEnabled=SelectedUrl!=null;
            title.Text=selected==null?(Edition.SelectedIndex==1?"Installed release notes unavailable":"No release notes available"):
                (launcher?"Launcher ":"Game ")+selected.Tag;
            DateTimeOffset date;published.Text=selected!=null && DateTimeOffset.TryParse(selected.Published,CultureInfo.InvariantCulture,DateTimeStyles.None,out date)?"Published "+date.ToLocalTime().ToString("D",CultureInfo.CurrentCulture):"";
            if(selected!=null && !String.IsNullOrWhiteSpace(selected.Name))published.Text+=(published.Text.Length==0?"":" · ")+selected.Name;
            var text=new ReleaseNotesText(selected==null?null:selected.Notes);
            Paint(changes,text.Changes,selected==null?"Refresh notes or select another release. Unpublished or unavailable versions may have no release notes.":"No change notes were published for this release.");
            publishedIssues.Children.Clear();
            if(!String.IsNullOrWhiteSpace(text.KnownIssues)) {
                var label=PageTheme.Text("Limitations published with this release",13,"#A7D9FA");label.Margin=new Thickness(0,14,0,8);publishedIssues.Children.Add(label);
                var content=new StackPanel();publishedIssues.Children.Add(content);Paint(content,text.KnownIssues,"");
            }
            if(text.Shortened)changes.Children.Add(PageTheme.Text("Notes shortened for display. View the complete release on GitHub.",12,"#A7D9FA"));
        }
        void Paint(Panel panel,string text,string empty)
        {
            panel.Children.Clear();if(String.IsNullOrWhiteSpace(text)){panel.Children.Add(PageTheme.Text(empty,12,"#9EB4C8"));return;}
            var notes=new ReleaseNotesMarkdown(openLink) {VerticalScrollBarVisibility=ScrollBarVisibility.Disabled};
            Ui.ForwardWheel(notes,scroll);
            notes.SetMarkdown(text);panel.Children.Add(notes);
        }
    }
}
