using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using TimeSplitters.Launcher;

public static class LifecycleSmoke
{
    static int result;
    static string root;
    [STAThread]
    public static int Main(string[] args)
    {
        root = args[0];
        var app = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
        app.Startup += async (s, e) => {
            try {
                await Check(WindowState.Normal, false);
                await Check(WindowState.Maximized, false);
                await Check(WindowState.Normal, true);
            }
            catch (Exception error) { Console.WriteLine(error); result = 1; }
            finally { app.Shutdown(); }
        };
        app.Run();
        return result;
    }
    static void Require(bool condition, string message)
    { if (!condition) throw new Exception(message); }
    static async Task Check(WindowState initial, bool fail)
    {
        var ui = new LauncherWindow(root, true);
        ui.Window.WindowState = initial;
        var probe = typeof(LauncherWindow).GetMethod("GameWindowReady", BindingFlags.Static | BindingFlags.NonPublic);
        Require(!(bool)probe.Invoke(null, new object[] { root }), "Another installation's process counted as ready.");
        using (var game = Process.Start(new ProcessStartInfo(Path.Combine(root, "Bin/futureperfect_recomp.exe"), fail ? "fail" : "") {
            UseShellExecute = false, CreateNoWindow = true }))
        {
            var method = typeof(LauncherWindow).GetMethod("MonitorGame", BindingFlags.Instance | BindingFlags.NonPublic);
            var monitor = (Task)method.Invoke(ui, new object[] { game });
            bool minimized = false;
            var deadline = Stopwatch.StartNew();
            while (!monitor.IsCompleted)
            {
                if (ui.Window.WindowState == WindowState.Minimized) minimized = true;
                if (deadline.ElapsedMilliseconds < 300)
                    Require(ui.Window.WindowState == initial, "Launcher minimized before the game window existed.");
                Require(deadline.ElapsedMilliseconds < 8000, "Monitor did not finish.");
                await Task.Delay(40);
            }
            await monitor;
            Require(minimized == !fail, "Unexpected minimization state.");
            Require(ui.Window.WindowState == initial, "Original window state was not restored.");
            Require(game.ExitCode == (fail ? 7 : 0), "Wrong fixture result.");
            Console.WriteLine("PASS " + (fail ? "startup failure stays visible" : initial + " waits for game window, minimizes, restores"));
        }
        ui.Window.Close();
    }
}
