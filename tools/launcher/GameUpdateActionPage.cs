using System;
using System.IO;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
#else
using System.Windows;
using System.Windows.Controls;
#endif

namespace TimeSplitters.Launcher
{
    // Shared update offer and opt-out presentation; host adapters retain the
    // existing verified install and game-launch commands.
    public sealed class GameUpdateActionPage
    {
        public const string UpdateNow = "Update now";
        public readonly Button PlayCurrent;
        public event Action PlayCurrentRequested;
        bool playable;

        public GameUpdateActionPage()
        {
            PlayCurrent = new Button { Name="PlayCurrentVersion", Content="Play current version",
                Background=Ui.Brush("#00000000"), Foreground=Ui.Brush("#85D4FF"),
                BorderThickness=new Thickness(0), Padding=new Thickness(0), MinHeight=28,
                FontSize=12, HorizontalContentAlignment=HorizontalAlignment.Center };
            PlayCurrent.Click+=(s,e)=>{if(playable && PlayCurrentRequested!=null)PlayCurrentRequested();};
            Update(false,false,"");
        }

        public void Update(bool offered,bool canPlay,string installed)
        {
            playable=offered&&canPlay;
            PlayCurrent.IsEnabled=playable;
            Ui.Show(PlayCurrent,playable);
            Ui.Tip(PlayCurrent,"Play "+installed+" without installing the available update.");
        }

        public static bool NewerOnCurrentBranch(Release release,string installed,string selected)
        {
            if(release==null || String.IsNullOrEmpty(installed) || !GameChannels.Valid(selected))return false;
            try {
                string channel=GameChannels.FromTag(installed);
                return release.Channel==channel && GameChannels.Preference(selected)==channel &&
                    GameVersion.Parse(release.Tag).CompareTo(GameVersion.Parse(installed))>0;
            } catch(InvalidDataException) { return false; }
        }
    }
}
