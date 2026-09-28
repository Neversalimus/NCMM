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

internal static partial class SetupCore
{
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

        sb.AppendLine("NCMM v" + RuntimeVersion + " Diagnostics 2.0");
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
            if (!String.Equals(binding.ncmm_version, RuntimeVersion, StringComparison.OrdinalIgnoreCase))
                AddCheck(sb, ref errors, ref warnings, "WARN", "Local binding belongs to a different NCMM runtime version.");
            else
                AddCheck(sb, ref errors, ref warnings, "OK", "Local binding version matches NCMM " + RuntimeVersion + ".");

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
            if (!String.Equals(runtime.runtime_version, RuntimeVersion, StringComparison.OrdinalIgnoreCase))
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
            if (!String.Equals(moduleState.host_version, RuntimeVersion, StringComparison.OrdinalIgnoreCase))
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
}
