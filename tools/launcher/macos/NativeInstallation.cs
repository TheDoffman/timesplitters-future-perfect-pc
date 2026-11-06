using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;

namespace TimeSplitters.Launcher
{
    // A translocated bundle cannot discover its portable installation by walking
    // parent folders. Ask for the real folder before logging, updating or locking.
    public static class NativeInstallation
    {
        public const string Guidance = "macOS opened this app in a temporary, read-only location. " +
            "Choose the extracted folder containing TimeSplitters Launcher.app and Support to continue. " +
            "Your game files, saves and settings stay in that folder.";

        public static bool Translocated(string path)
        {
#if LINUX
            return false;
#else
            var parts = Path.GetFullPath(path).Split(Path.DirectorySeparatorChar);
            for (int i = 0; i + 2 < parts.Length; i++)
                if (parts[i] == "AppTranslocation" && Guid.TryParse(parts[i + 1], out _) && parts[i + 2] == "d")
                    return true;
            return false;
#endif
        }

        public static string ValidateFolder(string folder, string runningExecutable)
        {
            string root = Path.GetFullPath(folder);
            Core.Require(!Translocated(root), "Choose the original extracted folder, outside the temporary AppTranslocation location.");
            Core.Require(Directory.Exists(root), "The selected folder no longer exists.");
            // Read directly: SelfUpdate.Complete logs invalid manifests, but a
            // rejected selection must not create files in an unrelated folder.
            string manifest = Core.PathIn(root, Core.SelfManifest);
            Core.Require(File.Exists(manifest), "Choose the folder containing both TimeSplitters Launcher.app and Support. Keep the whole extracted download together.");
            Core.Require(new FileInfo(manifest).Length <= LauncherPlatform.LauncherManifestLimit, "The launcher manifest is too large.");
            var files = SelfUpdate.Manifest(File.ReadAllText(manifest), LauncherVersion.Value);
            foreach (var pair in files) {
                string file = Core.PathIn(root, pair.Key);
                Core.Require(File.Exists(file) && Core.Hash(file).Equals(pair.Value, StringComparison.OrdinalIgnoreCase),
                    "The selected launcher is incomplete or modified. Extract the complete download again, preserving Data, Saves and Settings.");
            }
            Core.Require(Core.Hash(runningExecutable).Equals(files[Core.LauncherName], StringComparison.OrdinalIgnoreCase),
                "This folder contains a different launcher build. Open the app in that folder directly, or choose the folder for this download.");
            // Both the installation and any existing metadata directory must be
            // writable. Unique DeleteOnClose probes cannot replace user files.
            string metadata = Core.PathIn(root, ".launcher");
            Core.Require(!File.Exists(metadata), "A file named .launcher blocks this installation. Keep it for inspection and choose another extracted launcher folder.");
            try {
                foreach (string prefix in Directory.Exists(metadata) ? new[] { "", ".launcher/" } : new[] { "" }) {
                    string probe = Core.PathIn(root, prefix + ".tsfp-write-check-" + Guid.NewGuid().ToString("N"));
                    using (var stream = new FileStream(probe, FileMode.CreateNew, FileAccess.Write, FileShare.None, 1, FileOptions.DeleteOnClose))
                        stream.WriteByte(0);
                }
            } catch (IOException) {
                throw new IOException("This folder is read-only. Move the whole installation to a writable folder you own, then select it again.");
            } catch (UnauthorizedAccessException) {
                throw new IOException("This folder is not writable. Move the whole installation to a writable folder you own, then select it again.");
            }
            return root;
        }
    }

    public sealed class NativeLocationWindow : Window
    {
        readonly string[] args;
        readonly TextBlock status;
        readonly Button choose;
        public NativeLocationWindow(string[] args)
        {
            NativeApplication.ApplyWindowIcon(this);
            this.args = args;
            Title = "TimeSplitters · Choose installation folder";
            Width = 600; SizeToContent = SizeToContent.Height; CanResize = false;
            WindowStartupLocation = WindowStartupLocation.CenterScreen;
            Background = Brush.Parse("#0B1420");
            var panel = new StackPanel { Margin = new Thickness(28), Spacing = 12 };
            panel.Children.Add(NativeWindow.Text("CHOOSE YOUR LAUNCHER FOLDER", 22));
            panel.Children.Add(NativeWindow.Text(NativeInstallation.Guidance, 15));
            panel.Children.Add(NativeWindow.Text("If this happens again, quit the app, move the .app itself out of the installation folder and back using Finder, then reopen it. Keep Support beside the app."));
            status = NativeWindow.Text(""); panel.Children.Add(status);
            choose = NativeWindow.Button("Choose installation folder…", async () => await Choose());
            choose.Background = Brush.Parse("#21496B"); choose.Foreground = Brush.Parse("#FFFFFF");
            panel.Children.Add(choose);
            var quit = NativeWindow.Button("Quit", Close);
            quit.Background = Brush.Parse("#223041"); quit.Foreground = Brush.Parse("#E4EDF5");
            panel.Children.Add(quit);
            Content = new Border { Background = Background, Child = panel };
            int preview = Array.IndexOf(args, "--render-location-preview");
            if (preview >= 0) Opened += async (s, e) => {
                await Task.Delay(350);
                var visual = (Control)Content;
                using (var bitmap = new RenderTargetBitmap(PixelSize.FromSize(visual.Bounds.Size, 1), new Vector(96, 96))) {
                    bitmap.Render(visual); bitmap.Save(args[preview + 1]);
                }
                Close();
            };
        }

        async Task Choose()
        {
            choose.IsEnabled = false;
            try {
                var picked = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions {
                    Title = "Choose the folder containing TimeSplitters Launcher.app and Support",
                    AllowMultiple = false
                });
                string folder = picked.FirstOrDefault()?.TryGetLocalPath();
                if (folder == null) return;
                status.Text = "Verifying the launcher folder…";
                string root = await Task.Run(() => NativeInstallation.ValidateFolder(folder, Environment.ProcessPath));
                if (!IsVisible) return;
                Program.Root = root;
                var desktop = (IClassicDesktopStyleApplicationLifetime)Application.Current.ApplicationLifetime;
#if MACOS
                Program.InstallRoot=root;Program.Root=MacStorage.Resolve(args,root);
                var window=NativeApplication.StartupWindow(Program.Root,args);
#else
                var window = new NativeStartup(root, args);
#endif
                desktop.MainWindow = window; window.Show(); Close();
            } catch (Exception ex) {
                status.Text = ex.Message;
            } finally {
                choose.IsEnabled = true;
            }
        }
    }
}
