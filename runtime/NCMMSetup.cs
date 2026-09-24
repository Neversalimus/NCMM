using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

internal static class SetupCore
{
    internal static string Sha256(string path)
    {
        using (FileStream stream = File.OpenRead(path))
        using (SHA256 sha = SHA256.Create())
        {
            byte[] hash = sha.ComputeHash(stream);
            StringBuilder sb = new StringBuilder(hash.Length * 2);
            foreach (byte b in hash) sb.Append(b.ToString("x2"));
            return sb.ToString();
        }
    }

    internal static void Install(string gameRoot, string payloadRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        string ncmm = Path.Combine(gameRoot, "ncmm");
        string mods = Path.Combine(gameRoot, "code_mods");
        string bootstrap = Path.Combine(payloadRoot, "cataclysm-tiles.ncmm-bootstrap.exe");
        string awsDll = Path.Combine(payloadRoot, "code_mods", "AdvancedWorldSettings", "ncmm_mod.dll");
        string awsJson = Path.Combine(payloadRoot, "code_mods", "AdvancedWorldSettings", "mod.json");

        if (!File.Exists(exe)) throw new InvalidOperationException("cataclysm-tiles.exe not found in selected folder.");
        if (!File.Exists(bootstrap)) throw new InvalidOperationException("Installer payload is incomplete: bootstrap missing.");
        if (!File.Exists(awsDll)) throw new InvalidOperationException("Installer payload is incomplete: AWS module missing.");

        Directory.CreateDirectory(ncmm);
        Directory.CreateDirectory(mods);

        string bootstrapHash = Sha256(bootstrap);
        string currentHash = Sha256(exe);
        string installedHashFile = Path.Combine(ncmm, "bootstrap.sha256");
        string previousBootstrapHash = File.Exists(installedHashFile) ? File.ReadAllText(installedHashFile).Trim().ToLowerInvariant() : null;

        // Safe migration from the earlier CML prototype used during development.
        // If the current exe is exactly the legacy bootstrap and a vanilla backup exists,
        // preserve that backup and replace only the bootstrap.
        string legacyDir = Path.Combine(gameRoot, "cml");
        string legacyHashFile = Path.Combine(legacyDir, "bootstrap.sha256");
        string legacyBootstrapHash = File.Exists(legacyHashFile) ? File.ReadAllText(legacyHashFile).Trim().ToLowerInvariant() : null;
        bool legacyBootstrapInstalled = File.Exists(vanilla) && !String.IsNullOrEmpty(legacyBootstrapHash) &&
                                        String.Equals(currentHash, legacyBootstrapHash, StringComparison.OrdinalIgnoreCase);

        if (String.Equals(currentHash, bootstrapHash, StringComparison.OrdinalIgnoreCase))
        {
            if (!File.Exists(vanilla)) throw new InvalidOperationException("NCMM bootstrap is present but vanilla backup is missing. Refusing to guess.");
        }
        else if (legacyBootstrapInstalled)
        {
            File.Copy(bootstrap, exe, true);
            File.WriteAllText(Path.Combine(ncmm, "migrated-from-cml.txt"),
                "Legacy CML bootstrap replaced; existing vanilla backup preserved." + Environment.NewLine,
                Encoding.ASCII);
        }
        else if (File.Exists(vanilla) && !String.IsNullOrEmpty(previousBootstrapHash) &&
                 String.Equals(currentHash, previousBootstrapHash, StringComparison.OrdinalIgnoreCase))
        {
            File.Copy(bootstrap, exe, true);
        }
        else
        {
            if (File.Exists(vanilla))
            {
                string archive = Path.Combine(ncmm, "cataclysm-tiles.vanilla.backup-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + ".exe");
                File.Copy(vanilla, archive, true);
                File.Copy(exe, vanilla, true);
            }
            else
            {
                File.Move(exe, vanilla);
            }
            File.Copy(bootstrap, exe, true);
        }

        if (!String.Equals(Sha256(exe), bootstrapHash, StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Bootstrap post-install SHA256 check failed.");
        if (!File.Exists(vanilla)) throw new InvalidOperationException("Vanilla backup post-install check failed.");

        File.WriteAllText(installedHashFile, bootstrapHash.ToLowerInvariant() + Environment.NewLine, Encoding.ASCII);
        File.WriteAllText(Path.Combine(ncmm, "vanilla.sha256"), Sha256(vanilla).ToLowerInvariant() + Environment.NewLine, Encoding.ASCII);

        string awsDir = Path.Combine(mods, "AdvancedWorldSettings");
        Directory.CreateDirectory(awsDir);
        File.Copy(awsDll, Path.Combine(awsDir, "ncmm_mod.dll"), true);
        if (File.Exists(awsJson)) File.Copy(awsJson, Path.Combine(awsDir, "mod.json"), true);
        string disabled = Path.Combine(awsDir, "disabled");
        if (File.Exists(disabled)) File.Delete(disabled);

        string autoDisabled = Path.Combine(ncmm, "ncmm.auto_disabled");
        string pending = Path.Combine(ncmm, "boot.pending");
        if (File.Exists(autoDisabled)) File.Delete(autoDisabled);
        if (File.Exists(pending)) File.Delete(pending);
    }

    internal static void RestoreVanilla(string gameRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        if (!File.Exists(vanilla)) throw new InvalidOperationException("cataclysm-tiles.vanilla.exe not found. Nothing safe to restore.");
        File.Copy(vanilla, exe, true);
    }

    internal static List<string> DetectInstallations()
    {
        List<string> result = new List<string>();
        try
        {
            string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            string root = Path.Combine(local, "com.munetmo.cat-launcher", "Assets", "DarkDaysAhead");
            if (Directory.Exists(root))
            {
                foreach (string dir in Directory.GetDirectories(root))
                {
                    if (File.Exists(Path.Combine(dir, "cataclysm-tiles.exe"))) result.Add(dir);
                }
            }
        }
        catch { }
        return result.OrderByDescending(x => x, StringComparer.OrdinalIgnoreCase).ToList();
    }
}

internal sealed class MainForm : Form
{
    private readonly ComboBox pathBox = new ComboBox();
    private readonly TextBox log = new TextBox();
    private readonly Button installButton = new Button();
    private readonly Button restoreButton = new Button();
    private readonly Button browseButton = new Button();
    private readonly string payloadRoot;

    internal MainForm()
    {
        Text = "NCMM 0.3 Setup";
        Width = 760;
        Height = 420;
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(650, 360);

        payloadRoot = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "payload");

        Label title = new Label();
        title.Text = "Neversalimus Code Mod Manager";
        title.Font = new Font(Font.FontFamily, 16, FontStyle.Bold);
        title.AutoSize = true;
        title.Left = 18;
        title.Top = 18;
        Controls.Add(title);

        Label hint = new Label();
        hint.Text = "Choose a CDDA folder. NCMM keeps the original executable and falls back to vanilla when no certified host is available.";
        hint.AutoSize = false;
        hint.Left = 20;
        hint.Top = 58;
        hint.Width = 700;
        hint.Height = 42;
        Controls.Add(hint);

        pathBox.Left = 20;
        pathBox.Top = 108;
        pathBox.Width = 600;
        pathBox.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        pathBox.DropDownStyle = ComboBoxStyle.DropDown;
        foreach (string path in SetupCore.DetectInstallations()) pathBox.Items.Add(path);
        if (pathBox.Items.Count > 0) pathBox.SelectedIndex = 0;
        Controls.Add(pathBox);

        browseButton.Text = "Browse...";
        browseButton.Left = 630;
        browseButton.Top = 106;
        browseButton.Width = 100;
        browseButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        browseButton.Click += delegate { Browse(); };
        Controls.Add(browseButton);

        installButton.Text = "Install / Repair NCMM + AWS";
        installButton.Left = 20;
        installButton.Top = 150;
        installButton.Width = 220;
        installButton.Height = 34;
        installButton.Click += delegate { Install(); };
        Controls.Add(installButton);

        restoreButton.Text = "Restore vanilla EXE";
        restoreButton.Left = 250;
        restoreButton.Top = 150;
        restoreButton.Width = 180;
        restoreButton.Height = 34;
        restoreButton.Click += delegate { Restore(); };
        Controls.Add(restoreButton);

        log.Left = 20;
        log.Top = 200;
        log.Width = 710;
        log.Height = 160;
        log.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        log.Multiline = true;
        log.ScrollBars = ScrollBars.Vertical;
        log.ReadOnly = true;
        Controls.Add(log);

        Append("NCMM runtime does not require Git, CMake, MSYS2 or a compiler.");
        Append("If a matching certified host is unavailable, CDDA starts vanilla.");
    }

    private void Append(string text)
    {
        log.AppendText(DateTime.Now.ToString("HH:mm:ss") + "  " + text + Environment.NewLine);
    }

    private void Browse()
    {
        using (FolderBrowserDialog dialog = new FolderBrowserDialog())
        {
            dialog.Description = "Select the CDDA folder containing cataclysm-tiles.exe";
            if (dialog.ShowDialog(this) == DialogResult.OK) pathBox.Text = dialog.SelectedPath;
        }
    }

    private void Install()
    {
        try
        {
            SetupCore.Install(pathBox.Text, payloadRoot);
            Append("Installed successfully. Advanced World Settings enabled.");
            Append("Launch CDDA normally from CatLauncher/Catapult/shortcut. Host will be fetched only if exact SHA is certified.");
            MessageBox.Show(this, "NCMM + Advanced World Settings installed.\n\nYou can launch CDDA normally.", "NCMM", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Append("INSTALL FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM install failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void Restore()
    {
        try
        {
            SetupCore.RestoreVanilla(pathBox.Text);
            Append("Vanilla executable restored. NCMM files were left on disk for possible repair/reinstall.");
            MessageBox.Show(this, "Vanilla cataclysm-tiles.exe restored.", "NCMM", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Append("RESTORE FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM restore failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new MainForm());
    }
}
