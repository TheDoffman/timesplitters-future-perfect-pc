#if !MACOS
[assembly: System.Runtime.Versioning.TargetFramework(".NETFramework,Version=v4.8", FrameworkDisplayName = ".NET Framework 4.8")]
#endif
[assembly: System.Reflection.AssemblyVersion("1.2.24.0")]
[assembly: System.Reflection.AssemblyFileVersion("1.2.24.0")]
[assembly: System.Reflection.AssemblyInformationalVersion(TimeSplitters.Launcher.LauncherVersion.Value)]

namespace TimeSplitters.Launcher
{
    public static class LauncherVersion
    {
        // Shared by bootstrap, application, update manifest and release packaging.
        public const string Value = "1.2.24";
    }
}
