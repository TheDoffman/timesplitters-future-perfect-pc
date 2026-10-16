using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace TimeSplitters.Launcher
{
    public sealed class GameVersion : IComparable<GameVersion>
    {
        public long Major, Minor, Patch;
        public string Pre;
        public static GameVersion Parse(string value)
        {
            var m = Regex.Match(value ?? "", @"^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z.-]+)?$");
            long major, minor, patch;
            if (!m.Success || !long.TryParse(m.Groups[1].Value, out major) ||
                !long.TryParse(m.Groups[2].Value, out minor) || !long.TryParse(m.Groups[3].Value, out patch))
                throw new InvalidDataException("Release tags must use versions such as v1.2.3 or v1.2.3-beta.1.");
            string pre = m.Groups[4].Value;
            foreach (string part in pre.Split('.'))
                if (part.Length > 1 && part.All(Char.IsDigit) && part[0] == '0')
                    throw new InvalidDataException("Invalid numeric prerelease identifier.");
            return new GameVersion { Major = major, Minor = minor, Patch = patch, Pre = pre };
        }
        public int CompareTo(GameVersion other)
        {
            int c = Major.CompareTo(other.Major); if (c != 0) return c;
            c = Minor.CompareTo(other.Minor); if (c != 0) return c;
            c = Patch.CompareTo(other.Patch); if (c != 0) return c;
            if (Pre == other.Pre) return 0;
            if (Pre.Length == 0) return 1;
            if (other.Pre.Length == 0) return -1;
            var a = Pre.Split('.'); var b = other.Pre.Split('.');
            for (int i = 0; i < Math.Min(a.Length, b.Length); i++)
            {
                bool an = a[i].All(Char.IsDigit), bn = b[i].All(Char.IsDigit);
                c = an && bn ? (a[i].Length == b[i].Length ? String.CompareOrdinal(a[i], b[i]) : a[i].Length.CompareTo(b[i].Length)) :
                    an != bn ? (an ? -1 : 1) : String.CompareOrdinal(a[i], b[i]);
                if (c != 0) return c;
            }
            return a.Length.CompareTo(b.Length);
        }
    }

    public sealed class Release
    {
        public string Tag, Notes, Url, AssetName, AssetUrl, ChecksumUrl, Digest;
        public long Size;
    }
    public sealed class LauncherRelease
    {
        public Release Package;
        public string ManifestUrl;
    }

    public static class Json
    {
        public static JavaScriptSerializer Serializer() { return new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024 }; }
        public static Dictionary<string, object> Object(string text) { return Serializer().Deserialize<Dictionary<string, object>>(text); }
        public static string String(Dictionary<string, object> obj, string key) { object v; return obj.TryGetValue(key, out v) && v != null ? Convert.ToString(v, CultureInfo.InvariantCulture) : ""; }
        public static bool Flag(Dictionary<string, object> obj, string key) { object v; return obj.TryGetValue(key, out v) && v is bool && (bool)v; }
        public static string Encode(object value) { return Serializer().Serialize(value); }
    }

    public sealed class ReleaseClient : IDisposable
    {
        public const string Repository = "TheDoffman/timesplitters-future-perfect-pc";
        public const string ReleasesPage = "https://github.com/" + Repository + "/releases";
        readonly HttpClient http;
        public ReleaseClient() : this(null) { }
        internal ReleaseClient(HttpMessageHandler handler)
        {
            ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
            http = handler == null ? new HttpClient() : new HttpClient(handler);
            http.Timeout = TimeSpan.FromMinutes(20);
            http.DefaultRequestHeaders.UserAgent.ParseAdd("TimeSplitters-Launcher/" + LauncherVersion.Value);
            http.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
            http.DefaultRequestHeaders.Add("X-GitHub-Api-Version", "2022-11-28");
        }
        public void Dispose() { http.Dispose(); }
        public static void ValidateDownloadUrl(string url)
        {
            Uri uri;
            if (!Uri.TryCreate(url, UriKind.Absolute, out uri) || uri.Scheme != "https" || uri.Host != "github.com" ||
                !uri.AbsolutePath.StartsWith("/" + Repository + "/releases/download/", StringComparison.Ordinal) ||
                uri.UserInfo.Length != 0 || !uri.IsDefaultPort)
                throw new InvalidDataException("The release asset is not hosted by this project's GitHub repository.");
        }
        public static Release Select(string json, bool preview)
        {
            var parsed = Json.Serializer().DeserializeObject(json);
            var list = parsed is Dictionary<string, object> ? new object[] { parsed } : (object[])parsed;
            Release best = null;
            foreach (var item in list)
            {
                var r = (Dictionary<string, object>)item;
                if (Json.Flag(r, "draft") || (!preview && Json.Flag(r, "prerelease"))) continue;
                string tag = Json.String(r, "tag_name");
                GameVersion version;
                try { version = GameVersion.Parse(tag); } catch (InvalidDataException) { continue; }
                if (!preview && version.Pre.Length != 0) continue;
                string expected = "TimeSplitters-Future-Perfect-PC-" + tag + "-windows-x64.zip";
                object assets;
                if (!r.TryGetValue("assets", out assets)) continue;
                var files = ((object[])assets).Cast<Dictionary<string, object>>().ToList();
                var asset = files.FirstOrDefault(a => Json.String(a, "name") == expected && Json.String(a, "state") == "uploaded");
                if (asset == null) continue;
                var checksum = files.FirstOrDefault(a => Json.String(a, "name") == expected + ".sha256" && Json.String(a, "state") == "uploaded");
                string digest = Json.String(asset, "digest");
                if (!Regex.IsMatch(digest, "^sha256:[0-9a-fA-F]{64}$") && checksum == null) continue;
                string assetUrl = Json.String(asset, "browser_download_url");
                ValidateDownloadUrl(assetUrl);
                string checkUrl = checksum == null ? "" : Json.String(checksum, "browser_download_url");
                if (checkUrl.Length > 0) ValidateDownloadUrl(checkUrl);
                long size;
                if (!long.TryParse(Json.String(asset, "size"), out size) || size <= 0 || size > 4L * 1024 * 1024 * 1024) continue;
                if (best == null || version.CompareTo(GameVersion.Parse(best.Tag)) > 0)
                    best = new Release { Tag = tag, Notes = Json.String(r, "body"), Url = ReleasesPage + "/tag/" + Uri.EscapeDataString(tag),
                        AssetName = expected, AssetUrl = assetUrl, Size = size, ChecksumUrl = checkUrl, Digest = digest };
            }
            return best;
        }
        async Task<string> Text(string url, int limit, CancellationToken token)
        {
            using (var response = await http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, token).ConfigureAwait(false))
            {
                if (response.StatusCode == HttpStatusCode.NotFound) return "";
                if (response.StatusCode == HttpStatusCode.Forbidden || (int)response.StatusCode == 429)
                    throw new IOException("GitHub's request limit was reached. You can still play; check again later.");
                response.EnsureSuccessStatusCode();
                using (var input = await response.Content.ReadAsStreamAsync().ConfigureAwait(false))
                using (var output = new MemoryStream())
                {
                    var buffer = new byte[8192]; int got;
                    while ((got = await input.ReadAsync(buffer, 0, buffer.Length, token).ConfigureAwait(false)) > 0)
                    {
                        if (output.Length + got > limit) throw new InvalidDataException("GitHub returned an oversized response.");
                        output.Write(buffer, 0, got);
                    }
                    return System.Text.Encoding.UTF8.GetString(output.ToArray());
                }
            }
        }
        public async Task<Release> Check(bool preview, CancellationToken token)
        {
            string url = "https://api.github.com/repos/" + Repository + "/releases?per_page=100";
            string json = await Text(url, 4 * 1024 * 1024, token).ConfigureAwait(false);
            return json.Length == 0 ? null : Select(json, preview);
        }
        public static LauncherRelease SelectLauncher(string json, bool preview)
        {
            LauncherRelease best = null;
            foreach (var item in (object[])Json.Serializer().DeserializeObject(json))
            {
                var record = (Dictionary<string, object>)item;
                if (Json.Flag(record, "draft") || (!preview && Json.Flag(record, "prerelease"))) continue;
                object entries;
                if (!record.TryGetValue("assets", out entries)) continue;
                var files = ((object[])entries).Cast<Dictionary<string, object>>().Where(a => Json.String(a, "state") == "uploaded").ToList();
                var manifest = files.FirstOrDefault(a => Json.String(a, "name") == "launcher-update.json");
                if (manifest == null) continue;
                foreach (var asset in files)
                {
                    string name = Json.String(asset, "name");
                    var match = Regex.Match(name, @"^TimeSplitters-Launcher-v(.+)-windows-x64\.zip$");
                    if (!match.Success) continue;
                    string version = match.Groups[1].Value;
                    GameVersion parsed;
                    try { parsed = GameVersion.Parse(version); } catch (InvalidDataException) { continue; }
                    if ((!preview && parsed.Pre.Length != 0) || (best != null && parsed.CompareTo(GameVersion.Parse(best.Package.Tag)) <= 0)) continue;
                    long size;
                    if (!long.TryParse(Json.String(asset, "size"), out size) || size <= 0 || size > 16 * 1024 * 1024) continue;
                    string url = Json.String(asset, "browser_download_url"), manifestUrl = Json.String(manifest, "browser_download_url");
                    ValidateDownloadUrl(url); ValidateDownloadUrl(manifestUrl);
                    best = new LauncherRelease { ManifestUrl = manifestUrl, Package = new Release { Tag = version, AssetName = name,
                        Size = size, AssetUrl = url, Digest = Json.String(asset, "digest"), ChecksumUrl = "" } };
                }
            }
            return best;
        }
        public static Release VerifyLauncherMetadata(LauncherRelease candidate, string text)
        {
            var metadata = Json.Object(text);
            var release = candidate.Package;
            Core.Require(Json.String(metadata, "schema") == "1" && Json.String(metadata, "version") == release.Tag &&
                Json.String(metadata, "asset_name") == release.AssetName && Json.String(metadata, "size") == release.Size.ToString(CultureInfo.InvariantCulture),
                "Launcher update metadata does not match its release asset.");
            string hash = Json.String(metadata, "sha256");
            Core.Require(Regex.IsMatch(hash, "^[0-9a-fA-F]{64}$"), "Launcher archive checksum is missing.");
            if (release.Digest.Length > 0)
                Core.Require(release.Digest.Equals("sha256:" + hash, StringComparison.OrdinalIgnoreCase), "GitHub's launcher checksum differs from the update manifest.");
            release.Digest = "sha256:" + hash;
            return release;
        }
        public async Task<Release> CheckLauncher(bool preview, CancellationToken token)
        {
            // List releases so a newer game-only release cannot hide the last
            // launcher-only release. Launcher and game versions are independent.
            string json = await Text("https://api.github.com/repos/" + Repository + "/releases?per_page=100", 4 * 1024 * 1024, token).ConfigureAwait(false);
            if (json.Length == 0) return null;
            var candidate = SelectLauncher(json, preview);
            if (candidate == null) return null;
            return VerifyLauncherMetadata(candidate, await Text(candidate.ManifestUrl, 16384, token).ConfigureAwait(false));
        }
        public async Task Download(Release release, string destination, IProgress<double> progress, CancellationToken token)
        {
            ValidateDownloadUrl(release.AssetUrl);
            string expected = release.Digest.StartsWith("sha256:", StringComparison.Ordinal) ? release.Digest.Substring(7) : "";
            if (expected.Length == 0)
            {
                ValidateDownloadUrl(release.ChecksumUrl);
                string sum = (await Text(release.ChecksumUrl, 4096, token).ConfigureAwait(false)).Trim();
                var match = Regex.Match(sum, @"^([0-9a-fA-F]{64})\s+\*?" + Regex.Escape(release.AssetName) + "$", RegexOptions.CultureInvariant);
                if (!match.Success) throw new InvalidDataException("The release checksum is missing or invalid.");
                expected = match.Groups[1].Value;
            }
            using (var response = await http.GetAsync(release.AssetUrl, HttpCompletionOption.ResponseHeadersRead, token).ConfigureAwait(false))
            {
                response.EnsureSuccessStatusCode();
                if (response.RequestMessage.RequestUri.Scheme != "https") throw new InvalidDataException("Insecure asset redirect.");
                using (var input = await response.Content.ReadAsStreamAsync().ConfigureAwait(false))
                using (var output = new FileStream(destination, FileMode.CreateNew, FileAccess.Write, FileShare.None, 65536, true))
                {
                    byte[] buffer = new byte[65536]; long count = 0; int got;
                    while ((got = await input.ReadAsync(buffer, 0, buffer.Length, token).ConfigureAwait(false)) > 0)
                    {
                        count += got;
                        if (count > release.Size) throw new InvalidDataException("The download exceeds its published size.");
                        await output.WriteAsync(buffer, 0, got, token).ConfigureAwait(false);
                        progress.Report(100.0 * count / release.Size);
                    }
                    if (count != release.Size) throw new InvalidDataException("The release download is incomplete. Retry the update.");
                }
            }
            if (!Core.Hash(destination).Equals(expected, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Checksum mismatch. The update has not been installed; download it again.");
        }
    }
}
