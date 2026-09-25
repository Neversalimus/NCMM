using System;
using System.Diagnostics;
using System.IO;
using System.Text;

internal static class BootstrapFailureChild
{
    private static bool HasArg(string[] args, string value)
    {
        return Array.IndexOf(args, value) >= 0;
    }

    private static int Main(string[] args)
    {
        string self = Process.GetCurrentProcess().MainModule.FileName;
        string root = Path.GetDirectoryName(self);
        string name = Path.GetFileName(self);
        File.AppendAllText(Path.Combine(root, "child.log"),
            name + "|" + String.Join(" ", args) + Environment.NewLine, Encoding.UTF8);

        bool host = name.IndexOf(".ncmm.exe", StringComparison.OrdinalIgnoreCase) >= 0;
        if (!host) return 0;

        string ncmm = Path.Combine(root, "ncmm");
        string pending = Path.Combine(ncmm, "boot.pending");
        string ready = Path.Combine(ncmm, "boot.ready");

        if (HasArg(args, "--test-host-crash"))
        {
            return 77;
        }

        if (HasArg(args, "--test-host-ready-leave-pending"))
        {
            Directory.CreateDirectory(ncmm);
            File.WriteAllText(ready, "ready\n", Encoding.ASCII);
            return 0;
        }

        if (HasArg(args, "--test-host-ready"))
        {
            Directory.CreateDirectory(ncmm);
            File.WriteAllText(ready, "ready\n", Encoding.ASCII);
            if (File.Exists(pending)) File.Delete(pending);
            return 0;
        }

        return 0;
    }
}
