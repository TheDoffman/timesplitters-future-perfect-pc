using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace TimeSplitters.Launcher
{
    public sealed class KnownIssue
    {
        public long Number;
        public string Title;
        public string Url { get { return "https://github.com/"+ReleaseClient.Repository+"/issues/"+Number.ToString(System.Globalization.CultureInfo.InvariantCulture); } }
    }
    public sealed class KnownIssues
    {
        public const string TrackerUrl="https://github.com/"+ReleaseClient.Repository+"/issues?q=is%3Aissue%20is%3Aopen%20label%3Abug";
        public KnownIssue[] Items=new KnownIssue[0];
        public DateTime CheckedUtc;
        internal static KnownIssue[] ParsePage(string json,out bool more)
        {
            var records=Json.Serializer().DeserializeObject(json) as object[];
            if(records==null || records.Length>100)throw new System.IO.InvalidDataException("GitHub returned invalid issue information.");
            more=records.Length==100;
            var issues=new List<KnownIssue>();
            foreach(var item in records) {
                var record=item as Dictionary<string,object>;
                if(record==null)throw new System.IO.InvalidDataException("GitHub returned an invalid issue.");
                // GitHub's issues endpoint also returns pull requests. Recheck the
                // state and label locally instead of trusting only query filters.
                if(record.ContainsKey("pull_request"))continue;
                string state=Json.String(record,"state");
                if(state!="open" && state!="closed")throw new System.IO.InvalidDataException("GitHub returned an invalid issue state.");
                if(state!="open")continue;
                object raw;var labels=record.TryGetValue("labels",out raw)?raw as object[]:null;
                if(labels==null)throw new System.IO.InvalidDataException("GitHub returned invalid issue labels.");
                bool bug=false;
                foreach(var label in labels) {
                    string name=label is string?(string)label:label is Dictionary<string,object>?Json.String((Dictionary<string,object>)label,"name"):"";
                    if(String.IsNullOrWhiteSpace(name))throw new System.IO.InvalidDataException("GitHub returned an invalid issue label.");
                    if(String.Equals(name,"bug",StringComparison.OrdinalIgnoreCase))bug=true;
                }
                if(!bug)continue;
                long number;string title=Json.String(record,"title");
                if(!Int64.TryParse(Json.String(record,"number"),System.Globalization.NumberStyles.None,System.Globalization.CultureInfo.InvariantCulture,out number) || number<=0 || String.IsNullOrWhiteSpace(title))
                    throw new System.IO.InvalidDataException("GitHub returned an invalid bug report.");
                title=new string(title.Select(c=>Char.IsControl(c)?' ':c).ToArray()).Trim();
                if(title.Length>300)title=title.Substring(0,297)+"…";
                issues.Add(new KnownIssue {Number=number,Title=title});
            }
            return issues.ToArray();
        }
    }

    // Release descriptions are display-only data, never HTML, XAML or executable links.
    public sealed class ReleaseNews
    {
        public Release Game, Launcher, InstalledGame, InstalledLauncher;
        public DateTime CheckedUtc;
        public bool Preview;

        public static Release NotesFor(IEnumerable<Dictionary<string, object>> records, string tag)
        {
            var record = records.FirstOrDefault(r => !Json.Flag(r, "draft") && Json.String(r, "tag_name") == tag);
            if (record == null) return null;
            string version = tag.StartsWith("launcher-v", StringComparison.Ordinal) ? tag.Substring(10) : tag;
            return new Release { Tag=version, Notes=GameChannels.VisibleNotes(Json.String(record,"body")),
                Url=ReleaseClient.ReleasesPage+"/tag/"+Uri.EscapeDataString(tag),
                Name=Json.String(record,"name"), Published=Json.String(record,"published_at") };
        }
        public static string Availability(string installed, Release latest, bool launcher)
        {
            if (latest == null) return "No compatible published release is available on this channel.";
            if (String.IsNullOrEmpty(installed)) return "Available to install. Open Home to install the game runtime.";
            try {
                int order=GameVersion.Parse(latest.Tag).CompareTo(GameVersion.Parse(installed));
                if(!launcher && latest.Channel!=GameChannels.FromTag(installed))return "A different game channel is selected. Open Home to confirm switching versions; back up saves before testing.";
                if(order<0)return "Your installed version is newer than this channel's latest release. No downgrade is offered.";
                if(order==0)return "You're up to date on this channel. These notes describe your installed release.";
                return launcher ? "A newer launcher is available. Restart the launcher to let the startup updater install it." :
                    "A newer game release is available. Read what changes below, then open Home to update.";
            } catch(System.IO.InvalidDataException) { return "The installed version cannot be compared. Review the published release below."; }
        }
    }

    public sealed class ReleaseNotesText
    {
        public readonly string Changes, KnownIssues;
        public readonly bool Shortened;
        public ReleaseNotesText(string notes)
        {
            notes=(notes??"").Replace("\r\n","\n").Replace('\r','\n');
            if(notes.Length>64000){notes=notes.Substring(0,64000);Shortened=true;}
            var changes=new StringBuilder();var issues=new StringBuilder();
            bool inIssues=false,fenced=false;int issueLevel=0,lineCount=0;
            foreach(string line in notes.Split('\n')) {
                if(++lineCount>512){Shortened=true;break;}
                bool fence=Regex.IsMatch(line,@"^\s*(```|~~~)");
                var heading=fenced?Match.Empty:Regex.Match(line,@"^\s{0,3}(#{1,6})\s+(.+?)\s*#*\s*$");
                if(heading.Success) {
                    int level=heading.Groups[1].Value.Length;
                    if(inIssues && level<=issueLevel)inIssues=false;
                    string title=Plain(heading.Groups[2].Value).Trim().TrimEnd(':').ToLowerInvariant();
                    if(Regex.IsMatch(title,@"^(known issues|known limitations|known bugs|limitations|known issues\s*[/&]\s*limitations)$")) {
                        inIssues=true;issueLevel=level;continue;
                    }
                }
                (inIssues?issues:changes).AppendLine(line);
                if(fence)fenced=!fenced;
            }
            Changes=changes.ToString().Trim();KnownIssues=issues.ToString().Trim();
        }
        public static string Plain(string text)
        {
            text=Regex.Replace(text,@"!\[[^\]]*\]\([^\r\n]*?\)","[Image: view on GitHub]");
            text=Regex.Replace(text,@"\[([^\]]+)\]\(([^\r\n]*?)\)","$1 ($2)");
            return text.Replace("**","").Replace("__","").Replace("`","");
        }
    }
}
