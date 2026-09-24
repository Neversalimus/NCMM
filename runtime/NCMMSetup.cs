using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;
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

internal sealed class SetupHostBinding
{
    public string vanilla_sha256 { get; set; }
    public string host_sha256 { get; set; }
    public string source_commit { get; set; }
    public string upstream_tag { get; set; }
    public string installed_utc { get; set; }
}

internal sealed class DiagnosticsReport
{
    internal string Summary { get; set; }
    internal string Text { get; set; }
    internal int Errors { get; set; }
    internal int Warnings { get; set; }
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
        string payloadMods = Path.Combine(payloadRoot, "code_mods");

        if (!File.Exists(bootstrap)) throw new InvalidOperationException("Installer payload is incomplete: bootstrap missing.");
        if (!Directory.Exists(payloadMods)) throw new InvalidOperationException("Installer payload is incomplete: code_mods missing.");

        string[] bundledModules = Directory.GetDirectories(payloadMods)
            .Where(dir => File.Exists(Path.Combine(dir, "ncmm_mod.dll")) &&
                          File.Exists(Path.Combine(dir, "mod.json")))
            .ToArray();
        if (bundledModules.Length == 0)
            throw new InvalidOperationException("Installer payload contains no complete NCMM code-mods.");

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

        foreach (string sourceModule in bundledModules)
        {
            string moduleName = new DirectoryInfo(sourceModule).Name;
            string destination = Path.Combine(mods, moduleName);
            Directory.CreateDirectory(destination);
            File.Copy(Path.Combine(sourceModule, "ncmm_mod.dll"), Path.Combine(destination, "ncmm_mod.dll"), true);
            File.Copy(Path.Combine(sourceModule, "mod.json"), Path.Combine(destination, "mod.json"), true);
            // Preserve an existing user-created "disabled" marker during repair/update.
        }

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

    private static string ReadExpectedSha(string path)
    {
        if (!File.Exists(path)) return null;
        string value = File.ReadAllText(path).Trim().ToLowerInvariant();
        if (value.Length != 64 || value.Any(c => !Uri.IsHexDigit(c))) return null;
        return value;
    }

    private static void AddCheck(StringBuilder sb, ref int errors, ref int warnings,
        string status, string message)
    {
        if (status == "ERROR") errors++;
        else if (status == "WARN") warnings++;
        sb.Append('[').Append(status).Append("] ").AppendLine(message);
    }

    internal static DiagnosticsReport Diagnose(string gameRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        DetectedInstallation target = DescribeInstallation(gameRoot);

        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        string host = Path.Combine(gameRoot, "cataclysm-tiles.ncmm.exe");
        string ncmm = Path.Combine(gameRoot, "ncmm");
        string bootstrapHashFile = Path.Combine(ncmm, "bootstrap.sha256");
        string vanillaHashFile = Path.Combine(ncmm, "vanilla.sha256");
        string bindingPath = Path.Combine(ncmm, "host.binding.json");

        StringBuilder sb = new StringBuilder();
        int errors = 0;
        int warnings = 0;

        sb.AppendLine("NCMM v0.6.1 Diagnostics");
        sb.AppendLine("Target: " + target.BuildLabel);
        sb.AppendLine("Path: " + target.PathValue);
        sb.AppendLine("Source commit: " + (target.SourceCommit ?? "unknown"));
        sb.AppendLine();

        string activeSha = Sha256(exe).ToLowerInvariant();
        string expectedBootstrap = ReadExpectedSha(bootstrapHashFile);
        string vanillaSha = File.Exists(vanilla) ? Sha256(vanilla).ToLowerInvariant() : null;
        string expectedVanilla = ReadExpectedSha(vanillaHashFile);

        if (expectedBootstrap == null)
        {
            AddCheck(sb, ref errors, ref warnings, "WARN", "ncmm/bootstrap.sha256 is missing or invalid.");
        }
        else if (String.Equals(activeSha, expectedBootstrap, StringComparison.OrdinalIgnoreCase))
        {
            AddCheck(sb, ref errors, ref warnings, "OK", "Launch-path cataclysm-tiles.exe matches the installed NCMM bootstrap SHA.");
        }
        else if (vanillaSha != null && String.Equals(activeSha, vanillaSha, StringComparison.OrdinalIgnoreCase))
        {
            AddCheck(sb, ref errors, ref warnings, "WARN", "Vanilla executable is currently restored in the launch path; NCMM bootstrap is not active.");
        }
        else
        {
            AddCheck(sb, ref errors, ref warnings, "ERROR", "Launch-path executable matches neither saved bootstrap SHA nor vanilla backup.");
        }

        if (vanillaSha == null)
        {
            AddCheck(sb, ref errors, ref warnings, "ERROR", "cataclysm-tiles.vanilla.exe is missing.");
        }
        else if (expectedVanilla == null)
        {
            AddCheck(sb, ref errors, ref warnings, "WARN", "ncmm/vanilla.sha256 is missing or invalid.");
        }
        else if (!String.Equals(vanillaSha, expectedVanilla, StringComparison.OrdinalIgnoreCase))
        {
            AddCheck(sb, ref errors, ref warnings, "ERROR", "Vanilla backup SHA does not match ncmm/vanilla.sha256.");
        }
        else
        {
            AddCheck(sb, ref errors, ref warnings, "OK", "Vanilla backup SHA matches saved metadata.");
        }

        SetupHostBinding binding = null;
        if (!File.Exists(bindingPath))
        {
            AddCheck(sb, ref errors, ref warnings, "WARN", "host.binding.json is absent; a certified host may not have been downloaded yet.");
        }
        else
        {
            try
            {
                binding = new JavaScriptSerializer().Deserialize<SetupHostBinding>(File.ReadAllText(bindingPath));
                if (binding == null || String.IsNullOrEmpty(binding.host_sha256) ||
                    String.IsNullOrEmpty(binding.vanilla_sha256))
                {
                    binding = null;
                    AddCheck(sb, ref errors, ref warnings, "ERROR", "host.binding.json is incomplete.");
                }
                else
                {
                    AddCheck(sb, ref errors, ref warnings, "OK", "host.binding.json parsed successfully.");
                }
            }
            catch (Exception ex)
            {
                AddCheck(sb, ref errors, ref warnings, "ERROR", "host.binding.json parse failed: " + ex.Message);
            }
        }

        if (!File.Exists(host))
        {
            AddCheck(sb, ref errors, ref warnings, "WARN", "cataclysm-tiles.ncmm.exe is absent; bootstrap will need a certified host from the feed.");
        }
        else
        {
            string hostSha = Sha256(host).ToLowerInvariant();
            if (binding == null)
            {
                AddCheck(sb, ref errors, ref warnings, "WARN", "Host executable exists but cannot be validated without a valid binding.");
            }
            else if (!String.Equals(hostSha, binding.host_sha256, StringComparison.OrdinalIgnoreCase))
            {
                AddCheck(sb, ref errors, ref warnings, "ERROR", "Host executable SHA does not match binding.");
            }
            else
            {
                AddCheck(sb, ref errors, ref warnings, "OK", "Certified host SHA matches binding.");
            }

            if (binding != null && vanillaSha != null &&
                !String.Equals(vanillaSha, binding.vanilla_sha256, StringComparison.OrdinalIgnoreCase))
            {
                AddCheck(sb, ref errors, ref warnings, "ERROR", "Binding was created for a different vanilla executable SHA.");
            }

            if (binding != null && !String.IsNullOrEmpty(target.SourceCommit) &&
                !String.IsNullOrEmpty(binding.source_commit) &&
                !String.Equals(target.SourceCommit, binding.source_commit, StringComparison.OrdinalIgnoreCase))
            {
                AddCheck(sb, ref errors, ref warnings, "ERROR", "Binding source commit does not match VERSION.txt.");
            }
        }

        string awsDir = Path.Combine(gameRoot, "code_mods", "AdvancedWorldSettings");
        if (File.Exists(Path.Combine(awsDir, "ncmm_mod.dll")) && File.Exists(Path.Combine(awsDir, "mod.json")))
            AddCheck(sb, ref errors, ref warnings, "OK", "Advanced World Settings payload is present.");
        else
            AddCheck(sb, ref errors, ref warnings, "WARN", "Advanced World Settings payload is incomplete or absent.");

        if (File.Exists(Path.Combine(ncmm, "boot.pending")))
            AddCheck(sb, ref errors, ref warnings, "WARN", "boot.pending exists: previous/current host launch has not reached ready state.");
        else
            AddCheck(sb, ref errors, ref warnings, "OK", "boot.pending is clear.");

        if (File.Exists(Path.Combine(ncmm, "ncmm.auto_disabled")))
            AddCheck(sb, ref errors, ref warnings, "WARN", "ncmm.auto_disabled exists: crash-loop protection is active.");
        else
            AddCheck(sb, ref errors, ref warnings, "OK", "Crash-loop auto-disable is clear.");

        if (File.Exists(Path.Combine(ncmm, "ncmm.disabled")))
            AddCheck(sb, ref errors, ref warnings, "WARN", "ncmm.disabled exists: NCMM is manually disabled.");

        AddCheck(sb, ref errors, ref warnings, "INFO",
            "boot.ready: " + (File.Exists(Path.Combine(ncmm, "boot.ready")) ? "present" : "absent"));
        AddCheck(sb, ref errors, ref warnings, "INFO",
            "runtime.state.json: " + (File.Exists(Path.Combine(ncmm, "runtime.state.json")) ? "present" : "absent"));
        AddCheck(sb, ref errors, ref warnings, "INFO",
            "modules.state.json: " + (File.Exists(Path.Combine(ncmm, "modules.state.json")) ? "present" : "absent"));
        AddCheck(sb, ref errors, ref warnings, "INFO",
            "bootstrap.log: " + (File.Exists(Path.Combine(ncmm, "bootstrap.log")) ? "present" : "absent"));
        AddCheck(sb, ref errors, ref warnings, "INFO",
            "ncmm.log: " + (File.Exists(Path.Combine(ncmm, "ncmm.log")) ? "present" : "absent"));

        DiagnosticsReport report = new DiagnosticsReport();
        report.Errors = errors;
        report.Warnings = warnings;
        report.Summary = errors > 0 ? "ERROR" : warnings > 0 ? "WARNING" : "HEALTHY";
        sb.AppendLine();
        sb.AppendLine("Summary: " + report.Summary + " | errors=" + errors + " | warnings=" + warnings);
        report.Text = sb.ToString();
        return report;
    }

    internal static string RepairState(string gameRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        DescribeInstallation(gameRoot);

        string ncmm = Path.Combine(gameRoot, "ncmm");
        Directory.CreateDirectory(ncmm);
        string pending = Path.Combine(ncmm, "boot.pending");
        string autoDisabled = Path.Combine(ncmm, "ncmm.auto_disabled");
        StringBuilder result = new StringBuilder();

        result.AppendLine(DateTime.UtcNow.ToString("o") + " NCMM v0.6.1 safe state repair");
        result.AppendLine("Target: " + gameRoot);

        if (File.Exists(pending))
        {
            File.Delete(pending);
            result.AppendLine("Removed: boot.pending");
        }
        else result.AppendLine("Already clear: boot.pending");

        if (File.Exists(autoDisabled))
        {
            File.Delete(autoDisabled);
            result.AppendLine("Removed: ncmm.auto_disabled");
        }
        else result.AppendLine("Already clear: ncmm.auto_disabled");

        result.AppendLine("Preserved: ncmm.disabled, executables, binding, modules and feed settings.");
        result.AppendLine();

        File.AppendAllText(Path.Combine(ncmm, "repair.log"), result.ToString(), Encoding.UTF8);
        return result.ToString();
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
    private readonly Button diagnosticsButton = new Button();
    private readonly Button repairStateButton = new Button();
    private readonly Button browseButton = new Button();
    private readonly string payloadRoot;
    private int detectedInstallations;

    internal MainForm()
    {
        Text = "NCMM 0.6.1 Setup";
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

        installButton.Text = "Install / Repair NCMM + bundled mods";
        installButton.Left = 20;
        installButton.Top = 192;
        installButton.Width = 220;
        installButton.Height = 34;
        installButton.Click += delegate { Install(); };
        Controls.Add(installButton);

        restoreButton.Text = "Restore vanilla EXE";
        restoreButton.Left = 250;
        restoreButton.Top = 192;
        restoreButton.Width = 160;
        restoreButton.Height = 34;
        restoreButton.Click += delegate { Restore(); };
        Controls.Add(restoreButton);

        diagnosticsButton.Text = "Diagnostics";
        diagnosticsButton.Left = 420;
        diagnosticsButton.Top = 192;
        diagnosticsButton.Width = 150;
        diagnosticsButton.Height = 34;
        diagnosticsButton.Click += delegate { Diagnostics(); };
        Controls.Add(diagnosticsButton);

        repairStateButton.Text = "Repair NCMM State";
        repairStateButton.Left = 580;
        repairStateButton.Top = 192;
        repairStateButton.Width = 180;
        repairStateButton.Height = 34;
        repairStateButton.Click += delegate { RepairState(); };
        Controls.Add(repairStateButton);

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
            if (!ConfirmTarget(target, "Install / Repair NCMM + bundled mods")) return;

            InstallResult result = SetupCore.Install(target.PathValue, payloadRoot);
            Append("Installed successfully. Bundled NCMM code-mods deployed; existing disabled markers preserved.");
            Append("Target: " + result.BuildLabel + " | " + result.GameRoot);
            Append("Bootstrap SHA256: " + result.BootstrapSha256.ToUpperInvariant());
            Append("Vanilla SHA256: " + result.VanillaSha256.ToUpperInvariant());

            string message =
                "NCMM + bundled code-mods installed.\n\n" +
                "Target build: " + result.BuildLabel + "\n" +
                "Path: " + result.GameRoot + "\n" +
                "Bootstrap SHA256:\n" + result.BootstrapSha256.ToUpperInvariant() + "\n\n" +
                "You can launch CDDA normally.";

            MessageBox.Show(this, message, "NCMM 0.6.1", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
                "NCMM 0.6.1", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Append("RESTORE FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM restore failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void Diagnostics()
    {
        try
        {
            DetectedInstallation target = SelectedInstallation();
            DiagnosticsReport report = SetupCore.Diagnose(target.PathValue);
            Append("=== NCMM Diagnostics ===");
            foreach (string line in report.Text.Replace("\r\n", "\n").Split('\n'))
            {
                if (line.Length > 0) Append(line);
            }

            MessageBoxIcon icon = report.Errors > 0 ? MessageBoxIcon.Error :
                                  report.Warnings > 0 ? MessageBoxIcon.Warning :
                                  MessageBoxIcon.Information;
            MessageBox.Show(this,
                "Diagnostics finished: " + report.Summary + "\n\nFull report is in the Setup log.",
                "NCMM 0.6.1 Diagnostics", MessageBoxButtons.OK, icon);
        }
        catch (Exception ex)
        {
            Append("DIAGNOSTICS FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM diagnostics failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void RepairState()
    {
        try
        {
            DetectedInstallation target = SelectedInstallation();
            if (!ConfirmTarget(target, "Repair NCMM State")) return;

            string warning =
                "This safe repair removes ONLY:\n" +
                "  ncmm\\boot.pending\n" +
                "  ncmm\\ncmm.auto_disabled\n\n" +
                "It does NOT modify executables, vanilla backup, host binding, modules,\n" +
                "manual ncmm.disabled state, or feed settings.\n\nContinue?";

            if (MessageBox.Show(this, warning, "Repair NCMM State",
                MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;

            string result = SetupCore.RepairState(target.PathValue);
            foreach (string line in result.Replace("\r\n", "\n").Split('\n'))
            {
                if (line.Length > 0) Append(line);
            }
            MessageBox.Show(this,
                "Safe runtime state repair completed.\nSee ncmm\\repair.log for the audit trail.",
                "NCMM 0.6.1", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Append("STATE REPAIR FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM state repair failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
