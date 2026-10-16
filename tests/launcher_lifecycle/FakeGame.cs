using System;
using System.Threading;
using System.Windows;
using System.Windows.Threading;

public static class FakeGame
{
    [STAThread]
    public static int Main(string[] args)
    {
        Thread.Sleep(600);
        if (args.Length > 0 && args[0] == "fail") return 7;
        var app = new Application();
        var window = new Window { Title = "Synthetic launch lifecycle fixture", Width = 80,
            Height = 80, Left = -10000, Top = 0, ShowActivated = false };
        var timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(1300) };
        timer.Tick += (s, e) => { timer.Stop(); window.Close(); };
        window.Loaded += (s, e) => timer.Start();
        app.Run(window);
        return 0;
    }
}
