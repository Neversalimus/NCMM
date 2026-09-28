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
    private readonly CheckBox hostComponent = new CheckBox();
    private readonly CheckedListBox moduleComponents = new CheckedListBox();
    private readonly string payloadRoot;
    private int detectedInstallations;

    internal MainForm()
    {
        Text = "NCMM 0.8.0 Setup";
        Width = 920;
        Height = 680;
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(800, 600);

        payloadRoot = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "payload");

        Label title = new Label();
        title.Text = "Neversalimus Code Mod Manager";
        title.Font = new Font(Font.FontFamily, 16, FontStyle.Bold);
        title.AutoSize = true;
        title.Left = 18;
        title.Top = 18;
        Controls.Add(title);

        Label hint = new Label();
        hint.Text = "Choose the exact CDDA installation, then select the NCMM components you want. The Host/runtime is required; gameplay modules are independent and optional.";
        hint.AutoSize = false;
        hint.Left = 20;
        hint.Top = 58;
        hint.Width = 860;
        hint.Height = 42;
        hint.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        Controls.Add(hint);

        pathBox.Left = 20;
        pathBox.Top = 108;
        pathBox.Width = 750;
        pathBox.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        pathBox.DropDownStyle = ComboBoxStyle.DropDownList;
        pathBox.SelectedIndexChanged += delegate { UpdateTargetInfo(); };

        List<DetectedInstallation> detected = SetupCore.DetectInstallations();
        detectedInstallations = detected.Count;
        foreach (DetectedInstallation installation in detected) pathBox.Items.Add(installation);
        Controls.Add(pathBox);

        browseButton.Text = "Browse...";
        browseButton.Left = 780;
        browseButton.Top = 106;
        browseButton.Width = 100;
        browseButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        browseButton.Click += delegate { Browse(); };
        Controls.Add(browseButton);

        targetInfo.Left = 20;
        targetInfo.Top = 142;
        targetInfo.Width = 860;
        targetInfo.Height = 38;
        targetInfo.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        targetInfo.AutoEllipsis = true;
        Controls.Add(targetInfo);

        GroupBox components = new GroupBox();
        components.Text = "Components";
        components.Left = 20;
        components.Top = 184;
        components.Width = 860;
        components.Height = 142;
        components.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;

        hostComponent.Text = "NCMM Host / Runtime " + SetupCore.RuntimeVersion + "  (required)";
        hostComponent.Left = 18;
        hostComponent.Top = 25;
        hostComponent.Width = 390;
        hostComponent.Checked = true;
        hostComponent.Enabled = false;
        components.Controls.Add(hostComponent);

        moduleComponents.Left = 18;
        moduleComponents.Top = 50;
        moduleComponents.Width = 390;
        moduleComponents.Height = 74;
        moduleComponents.CheckOnClick = true;
        moduleComponents.IntegralHeight = false;
        List<SetupBundledModule> bundledModules =
            SetupCore.DiscoverBundledModules(Path.Combine(payloadRoot, "code_mods"));
        foreach (SetupBundledModule module in bundledModules)
        {
            moduleComponents.Items.Add(module, true);
        }
        components.Controls.Add(moduleComponents);

        Label componentHint = new Label();
        componentHint.Text = "Unchecking a previously installed bundled module removes only its NCMM-managed DLL and mod.json. User markers/state files are preserved.";
        componentHint.Left = 430;
        componentHint.Top = 30;
        componentHint.Width = 405;
        componentHint.Height = 72;
        componentHint.AutoSize = false;
        components.Controls.Add(componentHint);
        Controls.Add(components);

        installButton.Text = "Install / Repair selected";
        installButton.Left = 20;
        installButton.Top = 340;
        installButton.Width = 210;
        installButton.Height = 34;
        installButton.Click += delegate { Install(); };
        Controls.Add(installButton);

        restoreButton.Text = "Restore vanilla EXE";
        restoreButton.Left = 240;
        restoreButton.Top = 340;
        restoreButton.Width = 160;
        restoreButton.Height = 34;
        restoreButton.Click += delegate { Restore(); };
        Controls.Add(restoreButton);

        diagnosticsButton.Text = "Diagnostics 2.0";
        diagnosticsButton.Left = 410;
        diagnosticsButton.Top = 340;
        diagnosticsButton.Width = 150;
        diagnosticsButton.Height = 34;
        diagnosticsButton.Click += delegate { Diagnostics(); };
        Controls.Add(diagnosticsButton);

        repairStateButton.Text = "Repair NCMM State";
        repairStateButton.Left = 570;
        repairStateButton.Top = 340;
        repairStateButton.Width = 180;
        repairStateButton.Height = 34;
        repairStateButton.Click += delegate { RepairState(); };
        Controls.Add(repairStateButton);

        log.Left = 20;
        log.Top = 390;
        log.Width = 860;
        log.Height = 235;
        log.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        log.Multiline = true;
        log.ScrollBars = ScrollBars.Vertical;
        log.ReadOnly = true;
        Controls.Add(log);

        Append("NCMM runtime does not require Git, CMake, MSYS2 or a compiler.");
        Append("Bundled native modules are independently selectable.");
        Append("If a matching certified Host is unavailable, CDDA starts vanilla.");

        if (detectedInstallations > 1)
        {
            Append("Multiple CDDA installations detected. No target was selected automatically.");
            Append("Choose the exact build from the list or use Browse.");
        }
        else if (detectedInstallations == 0)
        {
            Append("No CatLauncher installation was detected automatically. Use Browse.");
        }
        else
        {
            pathBox.SelectedIndex = 0;
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

    private void SetAllModuleChecks(bool value)
    {
        for (int i = 0; i < moduleComponents.Items.Count; i++)
            moduleComponents.SetItemChecked(i, value);
    }

    private void SyncComponentSelection(DetectedInstallation selected)
    {
        if (selected == null)
        {
            SetAllModuleChecks(true);
            return;
        }

        bool ncmmAlreadyInstalled =
            File.Exists(Path.Combine(selected.PathValue, "ncmm", "bootstrap.sha256"));
        if (!ncmmAlreadyInstalled)
        {
            SetAllModuleChecks(true);
            return;
        }

        for (int i = 0; i < moduleComponents.Items.Count; i++)
        {
            SetupBundledModule module = moduleComponents.Items[i] as SetupBundledModule;
            bool installed = module != null && module.Manifest != null &&
                SetupCore.IsModuleInstalled(
                    selected.PathValue, module.DirectoryName, module.Manifest.id);
            moduleComponents.SetItemChecked(i, installed);
        }
    }

    private List<string> SelectedModuleIds()
    {
        List<string> ids = new List<string>();
        foreach (object item in moduleComponents.CheckedItems)
        {
            SetupBundledModule module = item as SetupBundledModule;
            if (module != null && module.Manifest != null &&
                !String.IsNullOrWhiteSpace(module.Manifest.id))
                ids.Add(module.Manifest.id);
        }
        return ids;
    }

    private string SelectedComponentSummary()
    {
        List<string> names = new List<string>();
        foreach (object item in moduleComponents.CheckedItems)
        {
            SetupBundledModule module = item as SetupBundledModule;
            if (module == null || module.Manifest == null) continue;
            names.Add(String.IsNullOrWhiteSpace(module.Manifest.name)
                ? module.Manifest.id
                : module.Manifest.name);
        }
        return names.Count == 0
            ? "NCMM Host only"
            : "NCMM Host + " + String.Join(" + ", names.ToArray());
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

        targetInfo.Text = "Target: " + selected.BuildLabel + " | " +
            selected.ShortCommit() + " | " + selected.PathValue;
        SyncComponentSelection(selected);
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
                MessageBox.Show(this, ex.Message, "Invalid CDDA folder",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
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
            string selection = SelectedComponentSummary();
            if (!ConfirmTarget(target, "Install / Repair: " + selection)) return;

            InstallResult result = SetupCore.Install(target.PathValue, payloadRoot, SelectedModuleIds());
            Append("Installed successfully: " + selection + ".");
            Append("Unselected bundled modules were safely deactivated; user-owned files were preserved.");
            Append("Target: " + result.BuildLabel + " | " + result.GameRoot);
            Append("Bootstrap SHA256: " + result.BootstrapSha256.ToUpperInvariant());
            Append("Vanilla SHA256: " + result.VanillaSha256.ToUpperInvariant());

            string modules = result.InstalledModuleIds == null || result.InstalledModuleIds.Count == 0
                ? "none (Host only)"
                : String.Join(", ", result.InstalledModuleIds.ToArray());

            string message =
                "NCMM installed successfully.\n\n" +
                "Target build: " + result.BuildLabel + "\n" +
                "Path: " + result.GameRoot + "\n" +
                "Optional modules: " + modules + "\n\n" +
                "You can launch CDDA normally.";

            MessageBox.Show(this, message, "NCMM " + SetupCore.RuntimeVersion,
                MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Append("INSTALL FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM install failed",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
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
                "NCMM " + SetupCore.RuntimeVersion, MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Append("RESTORE FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM restore failed",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void Diagnostics()
    {
        try
        {
            DetectedInstallation target = SelectedInstallation();
            DiagnosticsReport report = SetupCore.Diagnose(target.PathValue);
            Append("=== NCMM Diagnostics 2.0 ===");
            foreach (string line in report.Text.Replace("\r\n", "\n").Split('\n'))
            {
                if (line.Length > 0) Append(line);
            }

            if (!String.IsNullOrEmpty(report.SavedPath))
                Append("Diagnostics report saved: " + report.SavedPath);

            MessageBoxIcon icon = report.Errors > 0 ? MessageBoxIcon.Error :
                                  report.Warnings > 0 ? MessageBoxIcon.Warning :
                                  MessageBoxIcon.Information;
            MessageBox.Show(this,
                "Diagnostics 2.0 finished: " + report.Summary +
                (String.IsNullOrEmpty(report.SavedPath) ? "" : "\n\nSaved report:\n" + report.SavedPath),
                "NCMM " + SetupCore.RuntimeVersion + " Diagnostics 2.0", MessageBoxButtons.OK, icon);
        }
        catch (Exception ex)
        {
            Append("DIAGNOSTICS FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM diagnostics failed",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
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
                "NCMM " + SetupCore.RuntimeVersion, MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            Append("STATE REPAIR FAILED: " + ex.Message);
            MessageBox.Show(this, ex.Message, "NCMM state repair failed",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
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
