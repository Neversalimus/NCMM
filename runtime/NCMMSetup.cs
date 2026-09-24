using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

internal sealed class DetectedInstallation
{
    internal string PathValue { get; private set; }
    internal string BuildLabel { get; private set; }
    internal string SourceCommit { get; private set; }

    internal DetectedInstallation(string pathValue, string buildLabel, string sourceCommit)
    {
        PathValue = pathValue;
        BuildLabel = buildLabel;
        SourceCommit = sourceCommit;
    }

    internal string ShortCommit()
    {
        if (String.IsNullOrEmpty(SourceCommit)) return "commit unknown";
        return SourceCommit.Length <= 12 ? SourceCommit : SourceCommit.Substring(0, 12);
    }

    public override string ToString()
    {
        return BuildLabel + " | " + ShortCommit() + " | " + PathValue;
    }
}

internal sealed class InstallResult
{
    internal string GameRoot { get; set; }
    internal string BuildLabel { get; set; }
    internal string SourceCommit { get; set; }
    internal string BootstrapSha256 { get; set; }
    internal string VanillaSha256 { get; set; }
}

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

    internal static string ReadSourceCommit(string gameRoot)
    {
        try
        {
            string version = Path.Combine(gameRoot, "VERSION.txt");
            if (!File.Exists(version)) return null;
            foreach (string line in File.ReadAllLines(version))
            {
                const string prefix = "commit sha:";
                if (line.TrimStart().StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                {
                    string value = line.Substring(line.IndexOf(':') + 1).Trim();
                    if (value.Length >= 7) return value.ToLowerInvariant();
                }
            }
        }
        catch { }
        return null;
    }

    internal static DetectedInstallation DescribeInstallation(string gameRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        if (!File.Exists(exe))
            throw new InvalidOperationException("cataclysm-tiles.exe not found in selected folder.");

        string buildLabel = new DirectoryInfo(gameRoot).Name;
        return new DetectedInstallation(gameRoot, buildLabel, ReadSourceCommit(gameRoot));
    }

    internal static InstallResult Install(string gameRoot, string payloadRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        DetectedInstallation target = DescribeInstallation(gameRoot);

        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        string ncmm = Path.Combine(gameRoot, "ncmm");
        string mods = Path.Combine(gameRoot, "code_mods");
        string bootstrap = Path.Combine(payloadRoot, "cataclysm-tiles.ncmm-bootstrap.exe");
        string awsDll = Path.Combine(payloadRoot, "code_mods", "AdvancedWorldSettings", "ncmm_mod.dll");
        string awsJson = Path.Combine(payloadRoot, "code_mods", "AdvancedWorldSettings", "mod.json");

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

        string vanillaHash = Sha256(vanilla);
        File.WriteAllText(installedHashFile, bootstrapHash.ToLowerInvariant() + Environment.NewLine, Encoding.ASCII);
        File.WriteAllText(Path.Combine(ncmm, "vanilla.sha256"), vanillaHash.ToLowerInvariant() + Environment.NewLine, Encoding.ASCII);

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

        InstallResult result = new InstallResult();
        result.GameRoot = gameRoot;
        result.BuildLabel = target.BuildLabel;
        result.SourceCommit = target.SourceCommit;
        result.BootstrapSha256 = bootstrapHash;
        result.VanillaSha256 = vanillaHash;
        return result;
    }

    internal static void RestoreVanilla(string gameRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        if (!File.Exists(vanilla)) throw new InvalidOperationException("cataclysm-tiles.vanilla.exe not found. Nothing safe to restore.");
        File.Copy(vanilla, exe, true);
    }

    internal static List<DetectedInstallation> DetectInstallations()
    {
        List<DetectedInstallation> result = new List<DetectedInstallation>();
        try
        {
            string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            string root = Path.Combine(local, "com.munetmo.cat-launcher", "Assets", "DarkDaysAhead");
            if (Directory.Exists(root))
            {
                foreach (string dir in Directory.GetDirectories(root))
                {
                    try
                    {
                        if (File.Exists(Path.Combine(dir, "cataclysm-tiles.exe")))
                            result.Add(DescribeInstallation(dir));
                    }
                    catch { }
                }
            }
        }
        catch { }

        return result.OrderByDescending(x => x.BuildLabel, StringComparer.OrdinalIgnoreCase).ToList();
    }
}

internal sealed class MainForm : Form
{
    private readonly ComboBox pathBox = new ComboBox();
    private readonly Label targetInfo = new Label();
    private readonly TextBox log = new TextBox();
    private readonly Button installButton = new Button();
    private readonly Button restoreButton = new Button();
    private readonly Button browseButton = new Button();
    private readonly string payloadRoot;
    private int detectedInstallations;

    internal MainForm()
    {
        Text = "NCMM 0.3.2 Setup";
        Width = 900;
        Height = 500;
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(760, 430);

        payloadRoot = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "payload");

        Label title = new Label();
        title.Text = "Neversalimus Code Mod Manager";
        title.Font = new Font(Font.FontFamily, 16, FontStyle.Bold);
        title.AutoSize = true;
        title.Left = 18;
        title.Top = 18;
        Controls.Add(title);

        Label hint = new Label();
        hint.Text = "Choose the exact CDDA installation. NCMM preserves the original executable and falls back to vanilla when no certified host is available.";
        hint.AutoSize = false;
        hint.Left = 20;
        hint.Top = 58;
        hint.Width = 840;
        hint.Height = 42;
        hint.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        Controls.Add(hint);

        pathBox.Left = 20;
        pathBox.Top = 108;
        pathBox.Width = 730;
        pathBox.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        pathBox.DropDownStyle = ComboBoxStyle.DropDownList;
        pathBox.SelectedIndexChanged += delegate { UpdateTargetInfo(); };

        List<DetectedInstallation> detected = SetupCore.DetectInstallations();
        detectedInstallations = detected.Count;
        foreach (DetectedInstallation installation in detected) pathBox.Items.Add(installation);
        if (detected.Count == 1) pathBox.SelectedIndex = 0;
        Controls.Add(pathBox);

        browseButton.Text = "Browse...";
        browseButton.Left = 760;
        browseButton.Top = 106;
        browseButton.Width = 100;
        browseButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        browseButton.Click += delegate { Browse(); };
        Controls.Add(browseButton);

        targetInfo.Left = 20;
        targetInfo.Top = 142;
        targetInfo.Width = 840;
        targetInfo.Height = 42;
        targetInfo.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        targetInfo.AutoEllipsis = true;
        Controls.Add(targetInfo);

        installButton.Text = "Install / Repair NCMM + AWS";
        installButton.Left = 20;
        installButton.Top = 192;
        installButton.Width = 220;
        installButton.Height = 34;
        installButton.Click += delegate { Install(); };
        Controls.Add(installButton);

        restoreButton.Text = "Restore vanilla EXE";
        restoreButton.Left = 250;
        restoreButton.Top = 192;
        restoreButton.Width = 180;
        restoreButton.Height = 34;
        restoreButton.Click += delegate { Restore(); };
        Controls.Add(restoreButton);

        log.Left = 20;
        log.Top = 242;
        log.Width = 840;
        log.Height = 200;
        log.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        log.Multiline = true;
        log.ScrollBars = ScrollBars.Vertical;
        log.ReadOnly = true;
        Controls.Add(log);

        Append("NCMM runtime does not require Git, CMake, MSYS2 or a compiler.");
        Append("If a matching certified host is unavailable, CDDA starts vanilla.");

        if (detectedInstallations > 1)
        {
            Append("Multiple CDDA installations detected. No target was selected automatically.");
            Append("Choose the exact build from the list or use Browse.");
        }
        else if (detectedInstallations == 0)
        {
            Append("No CatLauncher installation was detected automatically. Use Browse.");
        }

        UpdateTargetInfo();
    }

    private void Append(string text)
    {
        log.AppendText(DateTime.Now.ToString("HH:mm:ss") + "  " + text + Environment.NewLine);
    }

    private DetectedInstallation SelectedInstallation()
    {
        DetectedInstallation selected = pathBox.SelectedItem as DetectedInstallation;
        if (selected == null)
            throw new InvalidOperationException("Choose the exact target CDDA installation first.");
        return selected;
    }

    private void UpdateTargetInfo()
    {
        DetectedInstallation selected = pathBox.SelectedItem as DetectedInstallation;
        if (selected == null)
        {
            if (detectedInstallations > 1)
                targetInfo.Text = "Target: none selected — multiple installations detected.";
            else
                targetInfo.Text = "Target: none selected — use Browse to choose a CDDA folder.";
            return;
        }

        targetInfo.Text = "Target: " + selected.BuildLabel + " | " + selected.ShortCommit() + " | " + selected.PathValue;
    }

    private void SelectInstallation(DetectedInstallation installation)
    {
        for (int i = 0; i < pathBox.Items.Count; i++)
        {
            DetectedInstallation existing = pathBox.Items[i] as DetectedInstallation;
            if (existing != null &&
                String.Equals(existing.PathValue, installation.PathValue, StringComparison.OrdinalIgnoreCase))
            {
                pathBox.SelectedIndex = i;
                return;
            }
        }

        pathBox.Items.Add(installation);
        pathBox.SelectedIndex = pathBox.Items.Count - 1;
    }

    private void Browse()
    {
        using (FolderBrowserDialog dialog = new FolderBrowserDialog())
        {
            dialog.Description = "Select the exact CDDA folder containing cataclysm-tiles.exe";
            if (dialog.ShowDialog(this) != DialogResult.OK) return;

            try
            {
                SelectInstallation(SetupCore.DescribeInstallation(dialog.SelectedPath));
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "Invalid CDDA folder", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }

    private bool ConfirmTarget(DetectedInstallation target, string action)
    {
        if (pathBox.Items.Count <= 1) return true;

        string message =
            "Multiple CDDA installations are available.\n\n" +
            "Action: " + action + "\n" +
            "Target build: " + target.BuildLabel + "\n" +
            "Commit: " + target.ShortCommit() + "\n" +
            "Path: " + target.PathValue + "\n\n" +
            "Continue with this exact target?";

        return MessageBox.Show(this, message, "Confirm NCMM target",
            MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes;
    }

    private void Install()
    {
        try
        {
            DetectedInstallation target = SelectedInstallation();
            if (!ConfirmTarget(target, "Install / Repair NCMM + AWS")) return;

            InstallResult result = SetupCore.Install(target.PathValue, payloadRoot);
            Append("Installed successfully. Advanced World Settings enabled.");
            Append("Target: " + result.BuildLabel + " | " + result.GameRoot);
            Append("Bootstrap SHA256: " + result.BootstrapSha256.ToUpperInvariant());
            Append("Vanilla SHA256: " + result.VanillaSha256.ToUpperInvariant());

            string message =
                "NCMM + Advanced World Settings installed.\n\n" +
                "Target build: " + result.BuildLabel + "\n" +
                "Path: " + result.GameRoot + "\n" +
                "Bootstrap SHA256:\n" + result.BootstrapSha256.ToUpperInvariant() + "\n\n" +
                "You can launch CDDA normally.";

            MessageBox.Show(this, message, "NCMM 0.3.2", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
            DetectedInstallation target = SelectedInstallation();
            if (!ConfirmTarget(target, "Restore vanilla EXE")) return;

            SetupCore.RestoreVanilla(target.PathValue);
            Append("Vanilla executable restored.");
            Append("Target: " + target.BuildLabel + " | " + target.PathValue);
            Append("NCMM files were left on disk for possible repair/reinstall.");

            MessageBox.Show(this,
                "Vanilla cataclysm-tiles.exe restored.\n\nTarget:\n" + target.PathValue,
                "NCMM 0.3.2", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
