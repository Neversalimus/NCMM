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
    public string patch_revision { get; set; }
    public string ncmm_version { get; set; }
    public int loader_api { get; set; }
    public string installed_utc { get; set; }
}


internal sealed class SetupRuntimeState
{
    public int schema { get; set; }
    public string runtime_version { get; set; }
    public int loader_api { get; set; }
    public string updated_utc { get; set; }
    public string source_commit { get; set; }
    public string vanilla_sha256 { get; set; }
    public string host_sha256 { get; set; }
    public string binding_host_sha256 { get; set; }
    public bool host_valid { get; set; }
    public string host_status { get; set; }
    public string feed_status { get; set; }
    public string selected_mode { get; set; }
    public string reason { get; set; }
    public bool manual_disabled { get; set; }
    public bool auto_disabled { get; set; }
    public bool boot_pending { get; set; }
    public bool offline { get; set; }
    public bool refresh_requested { get; set; }
    public bool diagnostics_only { get; set; }
    public int? last_exit_code { get; set; }
}

internal sealed class SetupModuleStateEntry
{
    public string id { get; set; }
    public string name { get; set; }
    public string version { get; set; }
    public string state { get; set; }
    public string lifecycle { get; set; }
    public string reason { get; set; }
    public string default_hotkey { get; set; }
    public string directory { get; set; }
}

internal sealed class SetupModulesState
{
    public int schema { get; set; }
    public string host_version { get; set; }
    public int loader_api { get; set; }
    public string[] capabilities { get; set; }
    public List<SetupModuleStateEntry> modules { get; set; }
}

internal sealed class SetupModuleManifest
{
    public string id { get; set; }
    public string name { get; set; }
    public string version { get; set; }
    public int loader_api { get; set; }
    public string[] requires { get; set; }
    public string failure_policy { get; set; }
    public string ui_hotkey { get; set; }
}

internal sealed class DiagnosticsReport
{
    internal string Summary { get; set; }
    internal string Text { get; set; }
    internal int Errors { get; set; }
    internal int Warnings { get; set; }
    internal string SavedPath { get; set; }
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
        string ready = Path.Combine(ncmm, "boot.ready");
        string readyTmp = ready + ".tmp";
        if (File.Exists(autoDisabled)) File.Delete(autoDisabled);
        if (File.Exists(pending)) File.Delete(pending);
        if (File.Exists(ready)) File.Delete(ready);
        if (File.Exists(readyTmp)) File.Delete(readyTmp);

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


    private static T ReadJsonBounded<T>(string path, long maxBytes, out string error) where T : class
    {
        error = null;
        try
        {
            if (!File.Exists(path))
            {
                error = "missing";
                return null;
            }
            FileInfo info = new FileInfo(path);
            if (info.Length <= 0 || info.Length > maxBytes)
            {
                error = "size_invalid";
                return null;
            }
            string text = File.ReadAllText(path);
            T value = new JavaScriptSerializer().Deserialize<T>(text);
            if (value == null) error = "null_json";
            return value;
        }
        catch (Exception ex)
        {
            error = ex.Message;
            return null;
        }
    }

    private static string ShortSha(string value)
    {
        if (String.IsNullOrEmpty(value)) return "none";
        return value.Length <= 12 ? value : value.Substring(0, 12);
    }

    private static string SafeUrlForReport(string value)
    {
        if (String.IsNullOrWhiteSpace(value)) return "default";
        try
        {
            Uri uri;
            if (!Uri.TryCreate(value.Trim(), UriKind.Absolute, out uri)) return "invalid";
            return uri.GetLeftPart(UriPartial.Path);
        }
        catch
        {
            return "invalid";
        }
    }

    private static string DescribeModuleReason(string reason)
    {
        if (String.IsNullOrEmpty(reason)) return "none";
        if (reason == "ok") return "loaded normally";
        if (reason == "user_disabled") return "disabled by user marker";
        if (reason == "duplicate_module_id") return "duplicate active module id";
        if (reason == "duplicate_module_id_runtime") return "duplicate id reached runtime guard";
        if (reason == "manifest_json_invalid") return "malformed JSON manifest";
        if (reason.StartsWith("manifest_duplicate_key:", StringComparison.Ordinal)) return "duplicate manifest key";
        if (reason.StartsWith("manifest_unknown_field:", StringComparison.Ordinal)) return "unsupported manifest field";
        if (reason.StartsWith("manifest_type_error:", StringComparison.Ordinal)) return "wrong manifest field type";
        if (reason.StartsWith("manifest_missing_field:", StringComparison.Ordinal)) return "required manifest field missing";
        if (reason.StartsWith("missing_capability:", StringComparison.Ordinal)) return "required host capability missing";
        if (reason == "manifest_descriptor_mismatch") return "mod.json and DLL descriptor disagree";
        if (reason == "capability_contract_mismatch") return "manifest and DLL capability lists disagree";
        if (reason == "turn_exception") return "turn callback quarantined after exception";
        if (reason == "locale_exception") return "locale callback quarantined after exception";
        if (reason == "ui_exception") return "UI callback quarantined after exception";
        if (reason == "api_version_mismatch") return "module requires an incompatible NCMM semantic API";
        if (reason == "migration_entrypoint_missing") return "state contract declared without migration callback";
        if (reason == "state_schema_invalid") return "stored state schema is invalid";
        if (reason == "state_schema_unsupported") return "stored state schema is outside the supported migration range";
        if (reason == "state_migration_exception") return "state migration threw and was suspended";
        if (reason == "state_migration_failed") return "state migration declined and was suspended";
        if (reason == "state_migration_uncommitted") return "state migration did not commit its target schema";
        return reason;
    }

    private static void AddFileInfo(StringBuilder sb, ref int errors, ref int warnings,
        string label, string path)
    {
        if (!File.Exists(path))
        {
            AddCheck(sb, ref errors, ref warnings, "INFO", label + ": absent");
            return;
        }
        try
        {
            FileInfo info = new FileInfo(path);
            AddCheck(sb, ref errors, ref warnings, "INFO",
                label + ": present | bytes=" + info.Length.ToString() +
                " | modified_utc=" + info.LastWriteTimeUtc.ToString("o"));
        }
        catch (Exception ex)
        {
            AddCheck(sb, ref errors, ref warnings, "WARN", label + ": metadata read failed: " + ex.Message);
        }
    }
    internal static DiagnosticsReport Diagnose(string gameRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        DetectedInstallation target = DescribeInstallation(gameRoot);

        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        string host = Path.Combine(gameRoot, "cataclysm-tiles.ncmm.exe");
        string ncmm = Path.Combine(gameRoot, "ncmm");
        string mods = Path.Combine(gameRoot, "code_mods");
        string bootstrapHashFile = Path.Combine(ncmm, "bootstrap.sha256");
        string vanillaHashFile = Path.Combine(ncmm, "vanilla.sha256");
        string bindingPath = Path.Combine(ncmm, "host.binding.json");
        string runtimeStatePath = Path.Combine(ncmm, "runtime.state.json");
        string modulesStatePath = Path.Combine(ncmm, "modules.state.json");
        string feedOverridePath = Path.Combine(ncmm, "feed.url");

        StringBuilder sb = new StringBuilder();
        int errors = 0;
        int warnings = 0;

        sb.AppendLine("NCMM v0.7.1 Diagnostics 2.0");
        sb.AppendLine("Generated UTC: " + DateTime.UtcNow.ToString("o"));
        sb.AppendLine("Target: " + target.BuildLabel);
        sb.AppendLine("Path: " + target.PathValue);
        sb.AppendLine("Source commit: " + (target.SourceCommit ?? "unknown"));
        sb.AppendLine();

        sb.AppendLine("=== Executables / Certification ===");
        string activeSha = Sha256(exe).ToLowerInvariant();
        string expectedBootstrap = ReadExpectedSha(bootstrapHashFile);
        string vanillaSha = File.Exists(vanilla) ? Sha256(vanilla).ToLowerInvariant() : null;
        string expectedVanilla = ReadExpectedSha(vanillaHashFile);
        string hostSha = File.Exists(host) ? Sha256(host).ToLowerInvariant() : null;

        AddCheck(sb, ref errors, ref warnings, "INFO", "Launch EXE SHA256: " + activeSha);
        AddCheck(sb, ref errors, ref warnings, "INFO", "Vanilla SHA256: " + (vanillaSha ?? "missing"));
        AddCheck(sb, ref errors, ref warnings, "INFO", "Host SHA256: " + (hostSha ?? "missing"));

        if (expectedBootstrap == null)
            AddCheck(sb, ref errors, ref warnings, "WARN", "ncmm/bootstrap.sha256 is missing or invalid.");
        else if (String.Equals(activeSha, expectedBootstrap, StringComparison.OrdinalIgnoreCase))
            AddCheck(sb, ref errors, ref warnings, "OK", "Launch-path EXE matches the installed NCMM bootstrap.");
        else if (vanillaSha != null && String.Equals(activeSha, vanillaSha, StringComparison.OrdinalIgnoreCase))
            AddCheck(sb, ref errors, ref warnings, "WARN", "Vanilla executable is restored in the launch path; bootstrap is not active.");
        else
            AddCheck(sb, ref errors, ref warnings, "ERROR", "Launch-path EXE matches neither saved bootstrap SHA nor vanilla backup.");

        if (vanillaSha == null)
            AddCheck(sb, ref errors, ref warnings, "ERROR", "cataclysm-tiles.vanilla.exe is missing.");
        else if (expectedVanilla == null)
            AddCheck(sb, ref errors, ref warnings, "WARN", "ncmm/vanilla.sha256 is missing or invalid.");
        else if (!String.Equals(vanillaSha, expectedVanilla, StringComparison.OrdinalIgnoreCase))
            AddCheck(sb, ref errors, ref warnings, "ERROR", "Vanilla backup SHA does not match ncmm/vanilla.sha256.");
        else
            AddCheck(sb, ref errors, ref warnings, "OK", "Vanilla backup SHA matches saved metadata.");

        SetupHostBinding binding = null;
        string bindingError;
        binding = ReadJsonBounded<SetupHostBinding>(bindingPath, 256 * 1024, out bindingError);
        if (binding == null)
        {
            if (bindingError == "missing")
                AddCheck(sb, ref errors, ref warnings, "WARN", "host.binding.json is absent; no local certified-host binding is available.");
            else
                AddCheck(sb, ref errors, ref warnings, "ERROR", "host.binding.json could not be parsed safely: " + bindingError);
        }
        else if (String.IsNullOrEmpty(binding.host_sha256) ||
                 String.IsNullOrEmpty(binding.vanilla_sha256) ||
                 String.IsNullOrEmpty(binding.patch_revision) ||
                 String.IsNullOrEmpty(binding.ncmm_version) ||
                 binding.loader_api != 1)
        {
            binding = null;
            AddCheck(sb, ref errors, ref warnings, "ERROR", "host.binding.json is incomplete or incompatible.");
        }
        else
        {
            AddCheck(sb, ref errors, ref warnings, "INFO",
                "Binding: NCMM=" + binding.ncmm_version +
                " | loader_api=" + binding.loader_api.ToString() +
                " | patch=" + ShortSha(binding.patch_revision) +
                " | upstream=" + (binding.upstream_tag ?? "unknown"));
            if (!String.Equals(binding.ncmm_version, "0.7.1", StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "WARN", "Local binding belongs to a different NCMM runtime version.");
            else
                AddCheck(sb, ref errors, ref warnings, "OK", "Local binding version matches NCMM 0.7.1.");

            if (hostSha == null)
                AddCheck(sb, ref errors, ref warnings, "WARN", "Certified host executable is absent.");
            else if (!String.Equals(hostSha, binding.host_sha256, StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "ERROR", "Host executable SHA does not match binding.");
            else
                AddCheck(sb, ref errors, ref warnings, "OK", "Certified host SHA matches binding.");

            if (vanillaSha != null &&
                !String.Equals(vanillaSha, binding.vanilla_sha256, StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "ERROR", "Binding belongs to a different vanilla executable SHA.");

            if (!String.IsNullOrEmpty(target.SourceCommit) &&
                !String.IsNullOrEmpty(binding.source_commit) &&
                !String.Equals(target.SourceCommit, binding.source_commit, StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "ERROR", "Binding source commit does not match VERSION.txt.");
        }

        sb.AppendLine();
        sb.AppendLine("=== Bootstrap Runtime State ===");
        string runtimeError;
        SetupRuntimeState runtime = ReadJsonBounded<SetupRuntimeState>(runtimeStatePath, 512 * 1024, out runtimeError);
        if (runtime == null)
        {
            if (runtimeError == "missing")
                AddCheck(sb, ref errors, ref warnings, "WARN", "runtime.state.json is absent; bootstrap has not produced a runtime snapshot yet.");
            else
                AddCheck(sb, ref errors, ref warnings, "ERROR", "runtime.state.json parse/read failed: " + runtimeError);
        }
        else
        {
            AddCheck(sb, ref errors, ref warnings, "INFO",
                "Runtime state: version=" + (runtime.runtime_version ?? "unknown") +
                " | loader_api=" + runtime.loader_api.ToString() +
                " | mode=" + (runtime.selected_mode ?? "unknown") +
                " | reason=" + (runtime.reason ?? "unknown"));
            AddCheck(sb, ref errors, ref warnings, "INFO",
                "Host state: valid=" + runtime.host_valid.ToString() +
                " | host_status=" + (runtime.host_status ?? "unknown") +
                " | feed_status=" + (runtime.feed_status ?? "unknown") +
                " | last_exit=" + (runtime.last_exit_code.HasValue ? runtime.last_exit_code.Value.ToString() : "none"));
            AddCheck(sb, ref errors, ref warnings, "INFO",
                "Flags: manual_disabled=" + runtime.manual_disabled.ToString() +
                " | auto_disabled=" + runtime.auto_disabled.ToString() +
                " | boot_pending=" + runtime.boot_pending.ToString() +
                " | offline=" + runtime.offline.ToString() +
                " | diagnostics_only=" + runtime.diagnostics_only.ToString());

            if (runtime.schema != 1)
                AddCheck(sb, ref errors, ref warnings, "WARN", "runtime.state.json schema is not 1.");
            if (!String.Equals(runtime.runtime_version, "0.7.1", StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "WARN", "runtime.state.json was produced by a different NCMM runtime version.");
            if (runtime.loader_api != 1)
                AddCheck(sb, ref errors, ref warnings, "ERROR", "runtime.state.json loader_api is incompatible.");
            if (vanillaSha != null && !String.IsNullOrEmpty(runtime.vanilla_sha256) &&
                !String.Equals(vanillaSha, runtime.vanilla_sha256, StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "ERROR", "Runtime-state vanilla SHA does not match the current vanilla backup.");
            if (hostSha != null && !String.IsNullOrEmpty(runtime.host_sha256) &&
                !String.Equals(hostSha, runtime.host_sha256, StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "WARN", "Runtime-state host SHA differs from the current host file.");
            if (binding != null && !String.IsNullOrEmpty(runtime.binding_host_sha256) &&
                !String.Equals(binding.host_sha256, runtime.binding_host_sha256, StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "WARN", "Runtime-state binding SHA differs from host.binding.json.");

            DateTime updated;
            if (DateTime.TryParse(runtime.updated_utc, null,
                System.Globalization.DateTimeStyles.RoundtripKind, out updated))
                AddCheck(sb, ref errors, ref warnings, "INFO", "Runtime state updated UTC: " + updated.ToUniversalTime().ToString("o"));
            else
                AddCheck(sb, ref errors, ref warnings, "WARN", "runtime.state.json updated_utc is missing or invalid.");
        }

        sb.AppendLine();
        sb.AppendLine("=== Manifest / Duplicate-ID Scan ===");
        Dictionary<string, List<string>> activeIds =
            new Dictionary<string, List<string>>(StringComparer.Ordinal);
        int installedModuleDirs = 0;
        if (!Directory.Exists(mods))
        {
            AddCheck(sb, ref errors, ref warnings, "WARN", "code_mods directory is absent.");
        }
        else
        {
            string[] moduleDirectories;
            try
            {
                moduleDirectories = Directory.GetDirectories(mods);
            }
            catch (Exception ex)
            {
                moduleDirectories = new string[0];
                AddCheck(sb, ref errors, ref warnings, "ERROR", "code_mods enumeration failed: " + ex.Message);
            }
            foreach (string dir in moduleDirectories.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                if (!File.Exists(Path.Combine(dir, "ncmm_mod.dll"))) continue;
                installedModuleDirs++;
                string folder = new DirectoryInfo(dir).Name;
                bool disabledModule = File.Exists(Path.Combine(dir, "disabled"));
                string manifestPath = Path.Combine(dir, "mod.json");
                string manifestError;
                SetupModuleManifest manifest =
                    ReadJsonBounded<SetupModuleManifest>(manifestPath, 64 * 1024, out manifestError);

                if (manifest == null)
                {
                    AddCheck(sb, ref errors, ref warnings, disabledModule ? "WARN" : "ERROR",
                        "Module " + folder + ": mod.json missing/invalid (" + manifestError + ").");
                    continue;
                }

                string id = manifest.id ?? "";
                AddCheck(sb, ref errors, ref warnings, "INFO",
                    "Module " + folder + ": id=" + (id.Length == 0 ? "<missing>" : id) +
                    " | version=" + (manifest.version ?? "unknown") +
                    " | loader_api=" + manifest.loader_api.ToString() +
                    " | disabled=" + disabledModule.ToString());

                if (id.Length == 0)
                {
                    AddCheck(sb, ref errors, ref warnings, disabledModule ? "WARN" : "ERROR",
                        "Module " + folder + " has no manifest id.");
                    continue;
                }
                if (!disabledModule)
                {
                    List<string> folders;
                    if (!activeIds.TryGetValue(id, out folders))
                    {
                        folders = new List<string>();
                        activeIds[id] = folders;
                    }
                    folders.Add(folder);
                }
            }
        }

        foreach (KeyValuePair<string, List<string>> pair in activeIds.OrderBy(x => x.Key, StringComparer.Ordinal))
        {
            if (pair.Value.Count > 1)
                AddCheck(sb, ref errors, ref warnings, "ERROR",
                    "Duplicate active module id '" + pair.Key + "' in: " + String.Join(", ", pair.Value.ToArray()) +
                    ". Disable/remove all but one copy.");
        }
        AddCheck(sb, ref errors, ref warnings, "INFO",
            "Installed NCMM module directories with DLL: " + installedModuleDirs.ToString() +
            " | unique active ids=" + activeIds.Count.ToString());

        sb.AppendLine();
        sb.AppendLine("=== Host Module State ===");
        string modulesError;
        SetupModulesState moduleState =
            ReadJsonBounded<SetupModulesState>(modulesStatePath, 1024 * 1024, out modulesError);
        if (moduleState == null)
        {
            if (modulesError == "missing")
                AddCheck(sb, ref errors, ref warnings, "WARN", "modules.state.json is absent; the NCMM host has not published module state yet.");
            else
                AddCheck(sb, ref errors, ref warnings, "ERROR", "modules.state.json parse/read failed: " + modulesError);
        }
        else
        {
            AddCheck(sb, ref errors, ref warnings, "INFO",
                "Host module state: schema=" + moduleState.schema.ToString() +
                " | host_version=" + (moduleState.host_version ?? "unknown") +
                " | loader_api=" + moduleState.loader_api.ToString());
            if (moduleState.schema < 1 || moduleState.schema > 3)
                AddCheck(sb, ref errors, ref warnings, "WARN", "modules.state.json schema is unknown.");
            if (!String.Equals(moduleState.host_version, "0.7.1", StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "WARN", "modules.state.json belongs to a different/stale host version.");
            if (moduleState.loader_api != 1)
                AddCheck(sb, ref errors, ref warnings, "ERROR", "modules.state.json loader_api is incompatible.");

            int loadedCount = 0, disabledCount = 0, rejectedCount = 0, failedCount = 0, faultCount = 0, suspendedCount = 0;
            Dictionary<string, int> stateIds = new Dictionary<string, int>(StringComparer.Ordinal);
            if (moduleState.modules != null)
            {
                foreach (SetupModuleStateEntry entry in moduleState.modules)
                {
                    if (entry == null) continue;
                    string id = entry.id ?? "<missing>";
                    string state = entry.state ?? "unknown";
                    string reason = entry.reason ?? "";
                    if (state != "disabled")
                    {
                        int seen = 0;
                        stateIds.TryGetValue(id, out seen);
                        stateIds[id] = seen + 1;
                    }

                    if (state == "loaded") loadedCount++;
                    else if (state == "disabled") disabledCount++;
                    else if (state == "rejected") rejectedCount++;
                    else if (state == "failed") failedCount++;
                    else if (state == "runtime_fault") faultCount++;
                    else if (state == "suspended") suspendedCount++;

                    string moduleLabel = id +
                        (String.IsNullOrEmpty(entry.directory) ? "" : " [" + entry.directory + "]") +
                        " | " + state +
                        (String.IsNullOrEmpty(entry.lifecycle) ? "" : " / lifecycle=" + entry.lifecycle) +
                        " | " + DescribeModuleReason(reason);
                    string status = state == "loaded" ? "OK" :
                                    state == "disabled" ? "INFO" :
                                    state == "runtime_fault" || state == "suspended" || state == "failed" || state == "rejected" ? "WARN" : "INFO";
                    AddCheck(sb, ref errors, ref warnings, status, "Module state: " + moduleLabel);
                }
            }

            foreach (KeyValuePair<string, int> pair in stateIds)
            {
                if (pair.Key != "<missing>" && pair.Value > 1)
                    AddCheck(sb, ref errors, ref warnings, "ERROR",
                        "modules.state.json contains duplicate module id '" + pair.Key + "' " +
                        pair.Value.ToString() + " times.");
            }
            AddCheck(sb, ref errors, ref warnings, "INFO",
                "Module summary: loaded=" + loadedCount.ToString() +
                " | disabled=" + disabledCount.ToString() +
                " | rejected=" + rejectedCount.ToString() +
                " | failed=" + failedCount.ToString() +
                " | runtime_fault=" + faultCount.ToString() +
                " | suspended=" + suspendedCount.ToString());
        }

        sb.AppendLine();
        sb.AppendLine("=== Recovery / Feed / Files ===");
        bool pendingPresent = File.Exists(Path.Combine(ncmm, "boot.pending"));
        bool readyPresent = File.Exists(Path.Combine(ncmm, "boot.ready"));
        if (pendingPresent && readyPresent)
            AddCheck(sb, ref errors, ref warnings, "WARN", "boot.pending + boot.ready: host reached ready state but pending cleanup did not complete.");
        else if (pendingPresent)
            AddCheck(sb, ref errors, ref warnings, "WARN", "boot.pending without boot.ready: previous/current host launch did not reach ready state.");
        else
            AddCheck(sb, ref errors, ref warnings, "OK", "boot.pending is clear.");

        if (File.Exists(Path.Combine(ncmm, "ncmm.auto_disabled")))
            AddCheck(sb, ref errors, ref warnings, "WARN", "ncmm.auto_disabled exists: crash-loop protection is active.");
        else
            AddCheck(sb, ref errors, ref warnings, "OK", "Crash-loop auto-disable is clear.");

        if (File.Exists(Path.Combine(ncmm, "ncmm.disabled")))
            AddCheck(sb, ref errors, ref warnings, "WARN", "ncmm.disabled exists: NCMM is manually disabled.");

        string feedValue = null;
        try
        {
            if (File.Exists(feedOverridePath)) feedValue = File.ReadAllText(feedOverridePath).Trim();
        }
        catch (Exception ex)
        {
            AddCheck(sb, ref errors, ref warnings, "WARN", "feed.url could not be read: " + ex.Message);
        }
        if (!String.IsNullOrEmpty(feedValue) &&
            !feedValue.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
            AddCheck(sb, ref errors, ref warnings, "WARN", "feed.url override is not HTTPS and will be ignored by bootstrap.");
        AddCheck(sb, ref errors, ref warnings, "INFO",
            "Feed source: " + SafeUrlForReport(feedValue));

        AddFileInfo(sb, ref errors, ref warnings, "bootstrap.log", Path.Combine(ncmm, "bootstrap.log"));
        AddFileInfo(sb, ref errors, ref warnings, "ncmm.log", Path.Combine(ncmm, "ncmm.log"));
        AddFileInfo(sb, ref errors, ref warnings, "last-feed-check.txt", Path.Combine(ncmm, "last-feed-check.txt"));

        DiagnosticsReport report = new DiagnosticsReport();
        report.Errors = errors;
        report.Warnings = warnings;
        report.Summary = errors > 0 ? "ERROR" : warnings > 0 ? "WARNING" : "HEALTHY";
        sb.AppendLine();
        sb.AppendLine("Summary: " + report.Summary + " | errors=" + errors + " | warnings=" + warnings);
        report.Text = sb.ToString();

        try
        {
            Directory.CreateDirectory(ncmm);
            string reportPath = Path.Combine(ncmm, "diagnostics-latest.txt");
            File.WriteAllText(reportPath, report.Text, Encoding.UTF8);
            report.SavedPath = reportPath;
        }
        catch
        {
            report.SavedPath = null;
        }

        return report;
    }

    internal static string RepairState(string gameRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        DescribeInstallation(gameRoot);

        string ncmm = Path.Combine(gameRoot, "ncmm");
        Directory.CreateDirectory(ncmm);
        string pending = Path.Combine(ncmm, "boot.pending");
        string ready = Path.Combine(ncmm, "boot.ready");
        string readyTmp = ready + ".tmp";
        string autoDisabled = Path.Combine(ncmm, "ncmm.auto_disabled");
        string autoDisabledTmp = autoDisabled + ".tmp";
        StringBuilder result = new StringBuilder();

        result.AppendLine(DateTime.UtcNow.ToString("o") + " NCMM v0.7.1 safe state repair");
        result.AppendLine("Target: " + gameRoot);

        foreach (string marker in new string[] { pending, ready, readyTmp, autoDisabled, autoDisabledTmp })
        {
            string label = Path.GetFileName(marker);
            if (File.Exists(marker))
            {
                File.Delete(marker);
                result.AppendLine("Removed: " + label);
            }
            else result.AppendLine("Already clear: " + label);
        }

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
        Text = "NCMM 0.7.1 Setup";
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

        diagnosticsButton.Text = "Diagnostics 2.0";
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

            MessageBox.Show(this, message, "NCMM 0.7.1", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
                "NCMM 0.7.1", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
                "NCMM 0.7.1 Diagnostics 2.0", MessageBoxButtons.OK, icon);
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
                "NCMM 0.7.1", MessageBoxButtons.OK, MessageBoxIcon.Information);
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
