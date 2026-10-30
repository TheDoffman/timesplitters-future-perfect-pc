using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;

namespace TimeSplitters.Launcher
{
    public sealed class GameSettings
    {
        public const int BindingSlots = 2;
        static readonly string[] PadActions = { "a", "b", "x", "y", "black", "white", "start", "back", "ltrigger", "rtrigger", "dpad_up", "dpad_down", "dpad_left", "dpad_right", "lstick_up", "lstick_down", "lstick_left", "lstick_right", "lthumb", "rstick_up", "rstick_down", "rstick_left", "rstick_right", "rthumb" };
        static readonly string[] PadLabels = { "Interact / confirm (A)", "Melee / back (B)", "Reload (X)", "Y action", "Black button action", "White button action", "Pause / Start", "Objectives / Back", "Throw grenade", "Fire", "Weapon mode / zoom in", "Grenade type / zoom out", "Previous weapon", "Next weapon", "Move forward", "Move backward", "Strafe left", "Strafe right", "Crouch", "Look up", "Look down", "Look left", "Look right", "Aim" };
        static string[] WeaponActions()
        {
            var defaults=Json.Object(DefaultControls());
            var profile=Map(((IEnumerable)defaults["controllers"]).Cast<object>().First());
            return Map(profile["bindings"]).Keys.Where(k=>k.StartsWith("weapon_slot_",StringComparison.Ordinal))
                .OrderBy(k=>Int32.Parse(k.Substring(12),CultureInfo.InvariantCulture)).ToArray();
        }
        public static readonly string[] Actions=PadActions.Concat(WeaponActions()).ToArray();
        public static readonly string[] Labels=PadLabels.Concat(WeaponActions().Select(k=>"Weapon slot "+k.Substring(12))).ToArray();
        readonly string root;
        readonly Dictionary<string, byte[]> originals = new Dictionary<string, byte[]>();
        Dictionary<string, object> document, controller, bindings;
        string runtime;
        public double Sensitivity = .10;
        public bool DiscordEnabled;
        public bool EnhancedTextures;
        public bool EnhancedAudio;
        public string DiscordApplicationId = "1561627743353962566";
        public bool MouseEnabled = true, InvertY, Fullscreen, Vrr = true, FpsOverlay, SkipStartupSplashes;
        public const int FovDefault = 55, FovMin = 40, FovMax = 100;
        public int VerticalFov = FovDefault;
        // Zero selects the original clock; 60 is not the experimental native-60 path.
        public int NativeFps;
        public static readonly int[] NativeFrameRates = { 0, 120, 180, 240 };
        public static readonly string[] NativeFrameRateLabels = { "60 FPS (Original)", "120 FPS (Beta)", "180 FPS (Beta)", "240 FPS (Beta)" };
        bool SupportsExtendedDisplay
        {
            get {
                // alpha.4 and earlier accept only V1 and heights through 2160.
                // Unpackaged development builds have no BUILD.json.
                try {
                    string version = Core.InstalledVersion(root);
                    return version.Length == 0 || GameVersion.Parse(version).CompareTo(GameVersion.Parse("v0.2.0-alpha.4")) > 0;
                } catch { return false; }
            }
        }
        public bool CanAdjustFov { get { return SupportsExtendedDisplay; } }
        public bool CanAutoResolution
        {
            get {
                try {
                    string path=Core.PathIn(root,"BUILD.json");
                    if(!File.Exists(path))return true; // Local development build.
                    object value;
                    return Json.Object(Text(Read(path))).TryGetValue("auto_display_resolution",out value) && value is bool && (bool)value;
                } catch { return false; }
            }
        }
        public bool SupportsResolution(int width, int height)
        { return (!(width==1 && height==1) || CanAutoResolution) && DisplayMode.Valid(width,height) && (SupportsExtendedDisplay || height <= 2160); }
        public int Width, Height, Scale = 1, Anisotropy = 1;
        // Retained keys migrate old installations; they are no longer user options.
        public int Interpolation { get { return 1; } }
        public int PresentationFps { get { return 0; } }
        public int RefreshRate { get { return 0; } }
        public string FrameCap { get { return "60"; } }
        public string Device { get { return Convert.ToString(controller["device"], CultureInfo.InvariantCulture); } set { controller["device"] = value; } }
        readonly string[] Files;
        static Dictionary<string, object> Map(object value)
        { var map = value as Dictionary<string, object>; Core.Require(map != null, "Settings contain an invalid object."); return map; }
        static byte[] Read(string path)
        { if (!File.Exists(path)) return null; Core.Require(new FileInfo(path).Length <= 1024 * 1024, "Settings file exceeds 1 MiB."); return File.ReadAllBytes(path); }
        static string Text(byte[] bytes) { return bytes == null ? "" : Encoding.UTF8.GetString(bytes).TrimStart('\ufeff'); }
        static string DefaultControls()
        { using (var s = Assembly.GetExecutingAssembly().GetManifestResourceStream("launcher-controls-default.json")) using (var r = new StreamReader(s)) return r.ReadToEnd(); }
        public static GameSettings Load(string root, string inheritedRuntime)
        {
            var model = new GameSettings(root);
            var Files = model.Files;
            foreach (string file in Files) model.originals[file] = Read(Core.PathIn(root, file));
            model.document = Json.Object(model.originals[Files[0]] == null ? DefaultControls() : Text(model.originals[Files[0]]));
            model.ReadController();
            string discord = Text(model.originals[Files[3]]);
            if (discord.Length > 0) {
                var presence = Regex.Match(discord, @"\ATSFP_DISCORD_V1\s+([01])\s+([0-9]{1,20})\s*\z");
                Core.Require(presence.Success, "Discord settings are invalid. No settings were changed.");
                model.DiscordEnabled = presence.Groups[1].Value == "1";
                model.DiscordApplicationId = presence.Groups[2].Value == "0" ? "" : presence.Groups[2].Value;
            }
            object value;
            if (model.document.TryGetValue("launcher", out value))
            {
                var launcher = Map(value);
                if (launcher.TryGetValue("enhanced_textures", out value))
                {
                    Core.Require(value is bool, "Enhanced textures must be true or false. No settings were changed.");
                    model.EnhancedTextures = (bool)value;
                }
                if (launcher.TryGetValue("skip_startup_splashes", out value))
                {
                    Core.Require(value is bool, "Skip startup splash screens must be true or false. No settings were changed.");
                    model.SkipStartupSplashes = (bool)value;
                }
            }
            if (model.document.TryGetValue("mouse", out value))
            {
                var mouse = Map(value);
                if (mouse.TryGetValue("sensitivity", out value)) model.Sensitivity = Convert.ToDouble(value, CultureInfo.InvariantCulture);
                if (mouse.TryGetValue("enabled", out value)) model.MouseEnabled = Convert.ToBoolean(value, CultureInfo.InvariantCulture);
                if (mouse.TryGetValue("invert_y", out value)) model.InvertY = Convert.ToBoolean(value, CultureInfo.InvariantCulture);
            }
            if(model.CanAutoResolution)model.Width=model.Height=1;
            string display = Text(model.originals[Files[1]]);
            if (display.Length > 0)
            {
                var match = Regex.Match(display, @"^\s*TSFP_DISPLAY_V(?<version>[12])\s+(\d+)\s+(\d+)(?:\s+(?<fov>\d+))?\s*$");
                Core.Require(match.Success && (match.Groups["version"].Value == "2") == match.Groups["fov"].Success, "The display settings file is invalid. No settings were changed.");
                if(match.Groups["fov"].Success) model.VerticalFov = Int32.Parse(match.Groups["fov"].Value, CultureInfo.InvariantCulture);
                model.Width = Int32.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture);
                model.Height = Int32.Parse(match.Groups[2].Value, CultureInfo.InvariantCulture);
            }
            string frameRate = Text(model.originals[Files[4]]);
            if (frameRate.Length > 0) {
                var match = Regex.Match(frameRate, @"\ATSFP_FRAME_RATE_V1\s+(0|120|144|180|240)\s*\z");
                Core.Require(match.Success, "Frame-rate settings are invalid. No settings were changed.");
                model.NativeFps = Int32.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture);
                if(model.NativeFps==144)model.NativeFps=120; // Previous preset; load without rewriting.
            }
            string audio = Text(model.originals[Files[5]]);
            if(audio.Length>0) {
                var match=Regex.Match(audio,@"\ATSFP_AUDIO_V1\s+([01])\s*\z");
                Core.Require(match.Success,"Enhanced Audio settings are invalid. No settings were changed.");
                model.EnhancedAudio=match.Groups[1].Value=="1";
            }
            model.runtime = Text(model.originals[Files[2]]);
            if (model.originals[Files[2]] == null && !String.IsNullOrEmpty(inheritedRuntime)) model.runtime = Text(Read(inheritedRuntime));
            var settings = ParseRuntime(model.runtime);
            model.Scale = Number(settings, "resolution_scale", 1);
            model.Anisotropy = Number(settings, "anisotropy", 1);
            model.Fullscreen = Number(settings, "fullscreen", 0) != 0;
            model.Vrr = Number(settings, "vrr", 1) != 0;
            model.FpsOverlay = Number(settings, "fps_overlay", 0) != 0;
            model.Validate(true);
            return model;
        }
        GameSettings(string path) { root = Path.GetFullPath(path); Files = LauncherPlatform.Settings(root); }
        void ReadController()
        {
            object value;
            Core.Require(document.TryGetValue("controllers", out value) && value is IEnumerable, "Controls file has no controller profiles.");
            int index = 0;
            foreach (object item in (IEnumerable)value)
            {
                var profile = Map(item); object port;
                if ((profile.TryGetValue("port", out port) && Convert.ToInt32(port) == 1) || (!profile.ContainsKey("port") && index == 0))
                { Core.Require(controller == null, "Controls file contains duplicate Player 1 profiles."); controller = profile; }
                index++;
            }
            Core.Require(controller != null, "Controls file has no Player 1 profile.");
            if (!controller.ContainsKey("device")) controller["device"] = "keyboard";
            if (!controller.TryGetValue("bindings", out value)) controller["bindings"] = value = new Dictionary<string, object>();
            bindings = Map(value);
            MigrateWeaponSlots();
        }
        void MigrateWeaponSlots()
        {
            if(Device!="keyboard")return;
            if(!bindings.ContainsKey("weapon_slot_1")) {
                foreach(int action in new[]{4,5}) {
                    var keys=Binding(action).Where(k=>k!="none").ToArray();
                    if(keys.Length==1 && KeyCode(keys[0])==(action==4?'2':'1')) {
                        int replacement=action==4?'X':'Z';
                        bindings[Actions[action]]=Conflict(action,replacement)<0?"key:#"+replacement:"none";
                    }
                }
            }
            for(int action=PadActions.Length;action<Actions.Length;action++) {
                if(bindings.ContainsKey(Actions[action]))continue;
                int slot=action-PadActions.Length, key=slot<9?'1'+slot:slot==9?'0':0;
                bindings[Actions[action]]=key!=0 && Conflict(action,key)<0?"key:#"+key:"none";
            }
        }
        public string[] Binding(int action)
        {
            object value; var list = new List<string>();
            if (bindings.TryGetValue(Actions[action], out value))
            {
                if (value is string) list.Add((string)value);
                else if (value is IEnumerable) foreach (object item in (IEnumerable)value) { Core.Require(item is string, "Invalid binding source."); list.Add((string)item); }
                else throw new InvalidDataException("Invalid binding value.");
            }
            Core.Require(list.Count <= 4, "This profile has more than four bindings on an action; it was left unchanged.");
            if (list.Count > BindingSlots) list.RemoveRange(BindingSlots, list.Count - BindingSlots);
            while (list.Count < BindingSlots) list.Add("none");
            return list.ToArray();
        }
        public const int WheelUpKey = 134, WheelDownKey = 135;
        public static int CaptureKey(int key) { return key==160||key==161?16:key==162||key==163?17:key==164||key==165?18:key; }
        public static string BindingLabel(string token)
        {
            if(String.IsNullOrEmpty(token)||token=="none")return "Unbound";
            int key=KeyCode(token);
            switch(key){case 1:return "Mouse 1";case 2:return "Mouse 2";case 4:return "Mouse 3";case 5:return "Mouse 4";case 6:return "Mouse 5";case 8:return "Backspace";case 13:return "Enter";case 16:return "Shift";case 17:return "Ctrl";case 18:return "Alt";case 27:return "Escape";case 32:return "Space";case WheelUpKey:return "Wheel up";case WheelDownKey:return "Wheel down";}
            if((key>=48&&key<=57)||(key>=65&&key<=90))return ((char)key).ToString();
            if(key>=112&&key<=135)return "F"+(key-111);
            return token.StartsWith("key:",StringComparison.OrdinalIgnoreCase)?token.Substring(4):token;
        }
        public static bool KeyAllowed(int key)
        { return key >= 0 && key <= 255 && key != 112 && key != 113 && !(key >= 120 && key <= 123) && key != 91 && key != 92; }
        public static int KeyCode(string token)
        {
            if (!token.StartsWith("key:", StringComparison.OrdinalIgnoreCase)) return 0;
            string name = token.Substring(4); int number;
            if (name.StartsWith("#") && Int32.TryParse(name.Substring(1), out number)) return number;
            if (name.Length == 1 && Char.IsLetterOrDigit(name[0])) return Char.ToUpperInvariant(name[0]);
            var names = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase) {
                {"LBUTTON",1},{"RBUTTON",2},{"MBUTTON",4},{"XBUTTON1",5},{"XBUTTON2",6},
                {"UP",38},{"DOWN",40},{"LEFT",37},{"RIGHT",39},{"RETURN",13},{"ENTER",13},{"BACK",8},{"BACKSPACE",8},
                {"SPACE",32},{"TAB",9},{"ESCAPE",27},{"SHIFT",16},{"CTRL",17},{"CONTROL",17},{"ALT",18},
                {"LSHIFT",160},{"RSHIFT",161},{"LCTRL",162},{"RCTRL",163},{"LALT",164},{"RALT",165},
                {"CAPSLOCK",20},{"INSERT",45},{"DELETE",46},{"HOME",36},{"END",35},{"PAGEUP",33},{"PAGEDOWN",34} };
            if (names.TryGetValue(name, out number)) return number;
            if (name.StartsWith("F") && Int32.TryParse(name.Substring(1), out number) && number >= 1 && number <= 24) return 111 + number;
            return 0;
        }
        public int Conflict(int action, int key)
        { for (int i = 0; i < Actions.Length; i++) if (i != action && Binding(i).Any(t => KeyCode(t) == key && key != 0)) return i; return -1; }
        public void Assign(int action, int slot, int key, bool move)
        {
            Core.Require(action >= 0 && action < Actions.Length && slot >= 0 && slot < BindingSlots && KeyAllowed(key), "That key is reserved by the game. Choose another key.");
            Core.Require(move || Conflict(action, key) < 0, "This key is already assigned to another action.");
            if (key != 0) for (int i = 0; i < Actions.Length; i++)
            {
                var keys = Binding(i); bool changed = false;
                for (int j = 0; j < BindingSlots; j++) if (KeyCode(keys[j]) == key) { keys[j] = "none"; changed = true; }
                if (changed) bindings[Actions[i]] = keys;
            }
            var assigned = Binding(action); assigned[slot] = key == 0 ? "none" : "key:#" + key;
            bindings[Actions[action]] = assigned;
        }
        public void ResetControls()
        {
            var defaults = Json.Object(DefaultControls());
            var profiles = (IEnumerable)defaults["controllers"];
            foreach (object profile in profiles) { bindings = Map(Map(profile)["bindings"]); break; }
            controller["bindings"] = bindings; Device = "keyboard";
            Sensitivity = .10; MouseEnabled = true; InvertY = false;
        }
        public static Dictionary<string, string> ParseRuntime(string text)
        {
            var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (string line in text.Split('\n'))
            { var m = Regex.Match(line, @"^\s*([a-zA-Z_]+)\s*=\s*([^#;\r]*?)\s*(?:[#;].*)?$"); if (m.Success) values[m.Groups[1].Value] = m.Groups[2].Value; }
            return values;
        }
        static int Number(Dictionary<string, string> values, string key, int fallback)
        { string v; int n; if (!values.TryGetValue(key, out v)) return fallback; if (Int32.TryParse(v, out n)) return n; if (v == "true" || v == "on" || v == "yes") return 1; if (v == "false" || v == "off" || v == "no") return 0; throw new InvalidDataException("Invalid runtime setting: " + key); }
        public void Validate(bool loading=false)
        {
            Core.Require(NativeFrameRates.Contains(NativeFps), "Choose 60 FPS (Original), 120 FPS (Beta), 180 FPS (Beta), or 240 FPS (Beta).");
            ulong discordId;
            Core.Require((!DiscordEnabled && DiscordApplicationId.Length == 0) ||
                (Regex.IsMatch(DiscordApplicationId, @"\A[1-9][0-9]{16,19}\z") && UInt64.TryParse(DiscordApplicationId, out discordId)),
                "Enter the public numeric Discord Application ID (17–20 digits), or turn off Rich Presence and clear the ID.");
            Core.Require(!Double.IsNaN(Sensitivity) && !Double.IsInfinity(Sensitivity) && Sensitivity >= .001 && Sensitivity <= 10, "Mouse sensitivity must be between 0.001 and 10.");
            Core.Require(CanAdjustFov || VerticalFov == FovDefault, "Custom FOV requires a game release newer than v0.2.0-alpha.4. Reset FOV to 55 degrees before saving.");
            Core.Require(VerticalFov >= FovMin && VerticalFov <= FovMax, "Vertical FOV must be between 40 and 100 degrees.");
            Core.Require(loading || Width!=1 || Height!=1 || CanAutoResolution, "Auto resolution requires a newer game build. Choose a manual resolution or update the game.");
            Core.Require((loading && Width==1 && Height==1) || SupportsResolution(Width,Height), "Choose a supported landscape resolution from 640×480, with an aspect ratio no wider than 4:1, or 0 × 0 for the console default. Maximum: " + (SupportsExtendedDisplay ? "7680×4320." : "7680×2160 for this game release."));
            Core.Require(Scale >= 1 && Scale <= 8 && Anisotropy >= 1 && Anisotropy <= 16, "A graphics setting is outside the supported range.");
            Core.Require(new[] { "keyboard", "none", "gamepad:0", "gamepad:1", "gamepad:2", "gamepad:3", "xinput:0", "xinput:1", "xinput:2", "xinput:3" }.Contains(Device), "Unsupported Player 1 input device.");
            for (int i = 0; i < Actions.Length; i++) Binding(i);
        }
        public static bool Running(string root)
        {
            bool running = false; string exe = Core.PathIn(root, LauncherPlatform.GamePath(root));
            foreach (var p in Process.GetProcessesByName("futureperfect_recomp")) using (p)
                try { if (!p.HasExited && String.Equals(p.MainModule.FileName, exe, StringComparison.OrdinalIgnoreCase)) running = true; }
                catch (InvalidOperationException) { } catch (System.ComponentModel.Win32Exception) { running = true; }
            return running;
        }
        static string UpdateRuntime(string text, Dictionary<string, string> values)
        {
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            string changed = Regex.Replace(text, @"(?m)^\s*([a-zA-Z_]+)[ \t]*=[^\r\n]*", m => {
                string key = m.Groups[1].Value; if (!values.ContainsKey(key)) return m.Value;
                seen.Add(key); return key + " = " + values[key]; });
            foreach (var pair in values) if (!seen.Contains(pair.Key)) changed += "\n" + pair.Key + " = " + pair.Value;
            return changed.TrimEnd('\r','\n') + "\n";
        }
        static void Replace(string path, byte[] data, bool backup)
        {
            string temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                using (var f = new FileStream(temp, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { f.Write(data, 0, data.Length); f.Flush(true); }
                if (File.Exists(path)) Core.AtomicReplace(temp, path, backup ? path + ".bak" : null);
                else File.Move(temp, path);
            }
            finally { if (File.Exists(temp)) File.Delete(temp); }
        }
        public void Save()
        {
            Validate(); Core.Require(!Running(root), "Close the game before saving settings.");
            foreach (string file in Files)
            {
                string path = Core.PathIn(root, file); byte[] now = Read(path), old = originals[file];
                Core.PathIn(root, file + ".bak");
                Core.Require(!Directory.Exists(path), "A settings path is a directory. No settings were saved.");
                Core.Require((now == null && old == null) || (now != null && old != null && now.SequenceEqual(old)), "Settings changed outside the launcher. Close and reopen Settings before saving.");
                Core.Require(!File.Exists(path) || (File.GetAttributes(path) & FileAttributes.ReadOnly) == 0, "A settings file is read-only. No settings were saved.");
            }
            if (Device == "keyboard") foreach (int action in Enumerable.Range(0, Actions.Length)) bindings[Actions[action]] = Binding(action);
            object value;
            if (!document.TryGetValue("mouse", out value)) document["mouse"] = value = new Dictionary<string, object>();
            var mouse = Map(value); mouse["sensitivity"] = Sensitivity; mouse["enabled"] = MouseEnabled; mouse["invert_y"] = InvertY;
            if (!document.TryGetValue("launcher", out value)) document["launcher"] = value = new Dictionary<string, object>();
            Map(value)["skip_startup_splashes"] = SkipStartupSplashes;
            Map(value)["enhanced_textures"] = EnhancedTextures;
            var graphics = new Dictionary<string, string> { {"resolution_scale",Scale.ToString()}, {"anisotropy",Anisotropy.ToString()}, {"frame_interp",Interpolation.ToString()}, {"fullscreen",Fullscreen?"1":"0"}, {"vrr",Vrr?"1":"0"}, {"fps_overlay",FpsOverlay?"1":"0"}, {"frame_cap",FrameCap} };
            graphics["frame_target"] = PresentationFps.ToString();
            graphics["refresh_rate"] = RefreshRate.ToString();
            var payload = new[] { Json.Encode(document) + "\n", (VerticalFov == FovDefault ? "TSFP_DISPLAY_V1 " + Width + " " + Height : "TSFP_DISPLAY_V2 " + Width + " " + Height + " " + VerticalFov) + "\n", UpdateRuntime(runtime, graphics),
                "TSFP_DISCORD_V1 " + (DiscordEnabled ? "1" : "0") + " " + (DiscordApplicationId.Length == 0 ? "0" : DiscordApplicationId) + "\n",
                "TSFP_FRAME_RATE_V1 " + NativeFps.ToString(CultureInfo.InvariantCulture) + "\n",
                "TSFP_AUDIO_V1 " + (EnhancedAudio ? "1" : "0") + "\n" };
            int committed = 0;
            try { for (int i = 0; i < Files.Length; i++) { Replace(Core.PathIn(root, Files[i]), Encoding.UTF8.GetBytes(payload[i]), true); committed++; } }
            catch
            {
                for (int i = committed - 1; i >= 0; i--) { string path = Core.PathIn(root, Files[i]); if (originals[Files[i]] == null) File.Delete(path); else Replace(path, originals[Files[i]], false); }
                throw;
            }
            foreach (string file in Files) originals[file] = Read(Core.PathIn(root, file));
            runtime = payload[2];
        }
        public static ProcessStartInfo StartInfo(string root)
        {
            var model = Load(root, null); model.Validate();
            SaveLocations.Resolve(root); // Fail visibly before launching if the chosen drive is missing.
            var Files = model.Files;
            var info = new ProcessStartInfo(Core.PathIn(root, LauncherPlatform.GamePath(root))) { WorkingDirectory = Core.PathIn(root, "Bin"), UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
            foreach (string key in info.EnvironmentVariables.Keys.Cast<string>().ToArray())
                if (key.StartsWith("RECOMP_", StringComparison.OrdinalIgnoreCase) || key.StartsWith("TSFP_", StringComparison.OrdinalIgnoreCase)) info.EnvironmentVariables.Remove(key);
            foreach (var pair in new Dictionary<string, string> {
                {"TSFP_CRASH_DIR","Logs/Crashes"}, {"RECOMP_GAME_DIR",LauncherPlatform.Data(root)}, {"RECOMP_SAVE_DIR",LauncherPlatform.Saves(root)}, {"RECOMP_INPUT_CONFIG",Files[0]}, {"RECOMP_FFMPEG_DIR","Bin"}, {"RECOMP_DISPLAY_CONFIG",Files[2]}, {"TSFP_DISPLAY_CONFIG",Files[1]}, {"TSFP_TEXTURE_PACK_DIR",LauncherPlatform.Textures(root)}, {"TSFP_ONLINE_SETTINGS","Settings/pc-online.json"} }) info.EnvironmentVariables[pair.Key] = Core.PathIn(root, pair.Value);
            info.EnvironmentVariables["TSFP_CRASH_EXTENDED"] = CrashReports.ExtendedForNextLaunch ? "1" : "0";
            // Explicit zero also overrides an old native_fps runtime preference.
            info.EnvironmentVariables["TSFP_NATIVE_FPS"] = model.NativeFps.ToString(CultureInfo.InvariantCulture);
            info.EnvironmentVariables["RECOMP_FRAME_INTERP"] = "1";
            info.EnvironmentVariables["RECOMP_FRAME_TARGET"] = "0";
            info.EnvironmentVariables["RECOMP_FPS_CAP"] = "60";
            info.EnvironmentVariables["RECOMP_REFRESH_RATE"] = "0";
            info.EnvironmentVariables["RECOMP_TRACE_BUDGET"] = "0";
            info.EnvironmentVariables["TSFP_DISCORD_CONFIG"] = Core.PathIn(root, Files[3]);
            // The existing XMV HLE completes skipped movies normally. Story
            // cutscenes are rendered by the game and do not use this player.
            info.EnvironmentVariables["RECOMP_XMV_PLAY"] = model.SkipStartupSplashes ? "0" : "1";
            info.EnvironmentVariables["RECOMP_PC_MOUSE"] = model.MouseEnabled ? "1" : "0";
            info.EnvironmentVariables["RECOMP_MOUSE_INVERT_Y"] = model.InvertY ? "1" : "0";
            info.EnvironmentVariables["RECOMP_MOUSE_SENSITIVITY"] = model.Sensitivity.ToString("R", CultureInfo.InvariantCulture);
            info.EnvironmentVariables["TSFP_TEXTURE_MODE"] = model.EnhancedTextures ? "4k" : "classic";
            #if MACOS
            NativeHost.Configure(info,root);
#endif
            return info;
        }
    }
}
