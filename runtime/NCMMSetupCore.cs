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
    internal List<string> InstalledModuleIds { get; set; }
    internal bool CompletionVerified { get; set; }
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



internal sealed class SetupCertifiedHostFeed
{
    public int schema { get; set; }
    public int loader_api { get; set; }
    public string runtime_version { get; set; }
    public string patch_revision { get; set; }
    public Dictionary<string, SetupCertifiedHostEntry> hosts { get; set; }
}

internal sealed class SetupCertifiedHostEntry
{
    public string source_commit { get; set; }
    public string upstream_tag { get; set; }
    public string host_url { get; set; }
    public string host_sha256 { get; set; }
    public string patch_revision { get; set; }
    public string ncmm_version { get; set; }
    public int loader_api { get; set; }
}

internal sealed class SetupHostSyncResult
{
    internal bool Ready { get; set; }
    internal string Message { get; set; }
    internal string HostSha256 { get; set; }
    internal string PatchRevision { get; set; }
}

internal sealed class SetupDownloadClient : System.Net.WebClient
{
    protected override System.Net.WebRequest GetWebRequest(Uri address)
    {
        System.Net.WebRequest request = base.GetWebRequest(address);
        request.Timeout = 30000;
        System.Net.HttpWebRequest http = request as System.Net.HttpWebRequest;
        if (http != null) http.ReadWriteTimeout = 30000;
        return request;
    }
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

internal sealed class SetupBundledModule
{
    internal string DirectoryName { get; set; }
    internal string SourceDirectory { get; set; }
    internal SetupModuleManifest Manifest { get; set; }

    public override string ToString()
    {
        if (Manifest == null) return DirectoryName ?? "<invalid module>";
        string name = String.IsNullOrWhiteSpace(Manifest.name) ? Manifest.id : Manifest.name.Trim();
        string version = String.IsNullOrWhiteSpace(Manifest.version) ? "" : Manifest.version.Trim();
        return version.Length == 0 ? name : name + " " + version;
    }
}

internal sealed class SetupInstalledComponent
{
    public string id { get; set; }
    public string version { get; set; }
    public string directory { get; set; }
    // Files installed by NCMM, with hashes. Never infer ownership from directory contents.
    public Dictionary<string, string> files { get; set; }
}

internal sealed class SetupInstalledComponents
{
    public int schema { get; set; }
    public string runtime_version { get; set; }
    public string updated_utc { get; set; }
    public List<SetupInstalledComponent> components { get; set; }
}

internal sealed class SetupTransactionState
{
    public int schema { get; set; }
    public string transaction_id { get; set; }
    public string game_root { get; set; }
    public string backup_root { get; set; }
    public string phase { get; set; }
    public string created_utc { get; set; }
}

internal sealed class SetupSnapshotEntry
{
    public string relative_path { get; set; }
    public bool existed { get; set; }
    public bool directory { get; set; }
    public string sha256 { get; set; }
    public Dictionary<string, string> files { get; set; }
}

internal sealed class SetupSnapshotManifest
{
    public int schema { get; set; }
    public string game_root { get; set; }
    public List<SetupSnapshotEntry> entries { get; set; }
}

internal sealed class DiagnosticsReport
{
    internal string Summary { get; set; }
    internal string Text { get; set; }
    internal int Errors { get; set; }
    internal int Warnings { get; set; }
    internal string SavedPath { get; set; }
}

internal static partial class SetupCore
{
    internal const string RuntimeVersion = "0.8.2";

    internal static string Sha256(string path)
    {
        return NcmmRuntimeIO.Hash(path);
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

    private static SetupModuleManifest ReadModuleManifest(string path)
    {
        if (!File.Exists(path)) return null;
        try
        {
            return new JavaScriptSerializer().Deserialize<SetupModuleManifest>(File.ReadAllText(path));
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException("Invalid NCMM module manifest: " + path + " | " + ex.Message);
        }
    }

    internal static List<SetupBundledModule> DiscoverBundledModules(string payloadMods)
    {
        if (!Directory.Exists(payloadMods))
            throw new InvalidOperationException("Installer payload is incomplete: code_mods missing.");

        List<SetupBundledModule> result = new List<SetupBundledModule>();
        HashSet<string> ids = new HashSet<string>(StringComparer.Ordinal);
        foreach (string dir in Directory.GetDirectories(payloadMods))
        {
            string dll = Path.Combine(dir, "ncmm_mod.dll");
            string manifestPath = Path.Combine(dir, "mod.json");
            bool hasDll = File.Exists(dll);
            bool hasManifest = File.Exists(manifestPath);
            if (!hasDll && !hasManifest) continue;
            if (!hasDll || !hasManifest)
                throw new InvalidOperationException("Installer payload contains an incomplete module: " + dir);

            SetupModuleManifest manifest = ReadModuleManifest(manifestPath);
            if (manifest == null || String.IsNullOrWhiteSpace(manifest.id) ||
                String.IsNullOrWhiteSpace(manifest.version))
                throw new InvalidOperationException("Installer payload contains a module with invalid identity: " + dir);
            if (!ids.Add(manifest.id))
                throw new InvalidOperationException("Installer payload contains duplicate module id: " + manifest.id);

            SetupBundledModule module = new SetupBundledModule();
            module.DirectoryName = new DirectoryInfo(dir).Name;
            module.SourceDirectory = dir;
            module.Manifest = manifest;
            result.Add(module);
        }
        if (result.Count == 0)
            throw new InvalidOperationException("Installer payload contains no complete NCMM code-mods.");
        return result.OrderBy(module =>
            module.Manifest == null || String.IsNullOrWhiteSpace(module.Manifest.name)
                ? module.DirectoryName
                : module.Manifest.name,
            StringComparer.OrdinalIgnoreCase).ToList();
    }

    internal static bool IsModuleInstalled(string gameRoot, string directoryName, string expectedId)
    {
        try
        {
            string dir = Path.Combine(Path.GetFullPath(gameRoot.Trim()), "code_mods", directoryName);
            string dll = Path.Combine(dir, "ncmm_mod.dll");
            string manifestPath = Path.Combine(dir, "mod.json");
            if (!File.Exists(dll) || !File.Exists(manifestPath)) return false;
            SetupModuleManifest manifest = ReadModuleManifest(manifestPath);
            return manifest != null && String.Equals(manifest.id, expectedId, StringComparison.Ordinal);
        }
        catch
        {
            return false;
        }
    }

    private static void AssertSafeModuleDestination(string destination, string expectedId)
    {
        NcmmRuntimeIO.GuardTree(destination);
        string manifestPath = Path.Combine(destination, "mod.json");
        if (!File.Exists(manifestPath)) return;
        SetupModuleManifest existing = ReadModuleManifest(manifestPath);
        if (existing == null || !String.Equals(existing.id, expectedId, StringComparison.Ordinal))
            throw new InvalidOperationException(
                "Refusing to overwrite module directory owned by a different module: " + destination);
    }


    private const string SetupPendingFile = ".ncmm-setup.pending.json";
    private const string SetupTransactionPrefix = ".ncmm-setup-tx-";

    private static SetupInstalledComponents ReadPreviousInstalledComponents(string gameRoot)
    {
        string path = Path.Combine(gameRoot, "ncmm", "installed-components.json");
        if (!File.Exists(path)) return null;
        try
        {
            SetupInstalledComponents state = new JavaScriptSerializer()
                .Deserialize<SetupInstalledComponents>(File.ReadAllText(path));
            return state != null && state.schema == 1 ? state : null;
        }
        catch { return null; }
    }

    private static bool SafeModuleDirectoryName(string name)
    {
        return !String.IsNullOrWhiteSpace(name) && name != "." && name != ".." &&
               String.Equals(Path.GetFileName(name), name, StringComparison.Ordinal) &&
               name.IndexOfAny(Path.GetInvalidFileNameChars()) < 0;
    }

    private static Dictionary<string, string> PackagedFileHashes(string source)
    {
        NcmmRuntimeIO.GuardTree(source);
        string prefix = Path.GetFullPath(source).TrimEnd(Path.DirectorySeparatorChar) +
                        Path.DirectorySeparatorChar;
        Dictionary<string, string> hashes =
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (string file in Directory.GetFiles(source, "*", SearchOption.AllDirectories))
        {
            string relative = Path.GetFullPath(file).Substring(prefix.Length)
                .Replace(Path.DirectorySeparatorChar, '/');
            if (hashes.ContainsKey(relative))
                throw new InvalidOperationException("Duplicate packaged module file: " + relative);
            hashes.Add(relative, Sha256(file));
        }
        return hashes;
    }

    private static void RemoveStalePackagedFiles(string destination,
                                                  SetupInstalledComponent previous,
                                                  Dictionary<string, string> currentFiles)
    {
        // Legacy receipts contain no file inventory: do not guess which extras are
        // user files. The first upgrade records a reliable baseline for future cleanup.
        if (previous == null || previous.files == null) return;
        string root = Path.GetFullPath(destination).TrimEnd(
            Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string prefix = root + Path.DirectorySeparatorChar;
        foreach (KeyValuePair<string, string> pair in previous.files)
        {
            if (currentFiles.ContainsKey(pair.Key)) continue;
            string relative = pair.Key.Replace('/', Path.DirectorySeparatorChar);
            if (Path.IsPathRooted(relative) ||
                relative.Split(Path.DirectorySeparatorChar).Any(
                    part => part == ".." || part == "." || part.Length == 0))
                throw new InvalidOperationException("Unsafe recorded module file: " + pair.Key);
            string target = Path.GetFullPath(Path.Combine(root, relative));
            if (!target.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Recorded module file escaped directory: " + pair.Key);
            string parent = Path.GetDirectoryName(target);
            while (!String.IsNullOrEmpty(parent) &&
                   parent.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            {
                if ((File.GetAttributes(parent) & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidOperationException("Reparse point in managed module path: " + parent);
                parent = Path.GetDirectoryName(parent);
            }
            if (!File.Exists(target)) continue;
            if (!String.Equals(Sha256(target), pair.Value, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException(
                    "Former packaged file was modified; refusing to remove: " + target);
            File.Delete(target);
        }
        foreach (string dir in Directory.GetDirectories(destination, "*", SearchOption.AllDirectories)
                     .OrderByDescending(x => x.Length))
        {
            if (Directory.GetFileSystemEntries(dir).Length == 0) Directory.Delete(dir);
        }
    }

    private static void AssertGameNotRunning(string gameRoot)
    {
        string root = Path.GetFullPath(gameRoot).TrimEnd(
            Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) +
            Path.DirectorySeparatorChar;
        foreach (Process process in Process.GetProcesses())
        {
            try
            {
                string name = process.ProcessName;
                if (!name.StartsWith("cataclysm", StringComparison.OrdinalIgnoreCase)) continue;
                string executable = process.MainModule.FileName;
                if (executable.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException(
                        "Close the running CDDA instance before installing or restoring vanilla.");
            }
            catch (System.ComponentModel.Win32Exception) { }
            catch (InvalidOperationException ex)
            {
                if (ex.Message.StartsWith("Close the running CDDA", StringComparison.Ordinal))
                    throw;
            }
            finally { process.Dispose(); }
        }
    }

    private static void CopyDirectoryTree(string source, string destination)
    {
        NcmmRuntimeIO.GuardTree(source);
        NcmmRuntimeIO.GuardTree(destination);
        Directory.CreateDirectory(destination);
        foreach (string file in Directory.GetFiles(source))
        {
            string target = Path.Combine(destination, Path.GetFileName(file));
            string staged = target + ".copy-" + Guid.NewGuid().ToString("N");
            try { NcmmRuntimeIO.CopyDurable(file, staged); NcmmRuntimeIO.PublishFile(staged, target); }
            finally { if (File.Exists(staged)) File.Delete(staged); }
        }
        foreach (string directory in Directory.GetDirectories(source))
            CopyDirectoryTree(directory, Path.Combine(destination, Path.GetFileName(directory)));
    }

    private static void DeletePath(string path)
    {
        NcmmRuntimeIO.GuardTree(path);
        if (File.Exists(path))
        {
            File.SetAttributes(path, FileAttributes.Normal);
            File.Delete(path);
        }
        else if (Directory.Exists(path))
        {
            foreach (string file in Directory.GetFiles(path, "*", SearchOption.AllDirectories))
            {
                try { File.SetAttributes(file, FileAttributes.Normal); } catch { }
            }
            Directory.Delete(path, true);
        }
    }

    private static void WriteJsonAtomic(string path, object value)
    {
        NcmmRuntimeIO.WriteTextAtomic(path,
            new JavaScriptSerializer().Serialize(value) + Environment.NewLine);
    }

    private static string SetupPendingPath(string gameRoot)
    {
        return Path.Combine(gameRoot, SetupPendingFile);
    }

    private static void AssertTransactionPathSafe(string gameRoot, string backupRoot)
    {
        string root = Path.GetFullPath(gameRoot).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
                      + Path.DirectorySeparatorChar;
        string backup = Path.GetFullPath(backupRoot);
        if (!backup.StartsWith(root, StringComparison.OrdinalIgnoreCase) ||
            !Path.GetFileName(backup).StartsWith(SetupTransactionPrefix, StringComparison.Ordinal))
            throw new InvalidOperationException("Unsafe NCMM setup transaction backup path: " + backupRoot);
    }

    private static List<string> GetManagedSetupPaths(string gameRoot, string payloadRoot)
    {
        List<string> paths = new List<string>();
        paths.Add("cataclysm-tiles.exe");
        paths.Add("cataclysm-tiles.vanilla.exe");
#if NCMM_FIRST_PERSON_PREVIEW
        paths.Add("cataclysm-tiles.ncmm.exe");
#endif
        paths.Add("ncmm");

        string payloadMods = Path.Combine(payloadRoot, "code_mods");
        foreach (SetupBundledModule module in DiscoverBundledModules(payloadMods))
            paths.Add(Path.Combine("code_mods", module.DirectoryName));

        SetupInstalledComponents previous = ReadPreviousInstalledComponents(gameRoot);
        if (previous != null && previous.components != null)
            foreach (SetupInstalledComponent module in previous.components)
                if (module != null && module.id != "ncmm_host" &&
                    SafeModuleDirectoryName(module.directory))
                    paths.Add(Path.Combine("code_mods", module.directory));

        return paths.Distinct(StringComparer.OrdinalIgnoreCase).ToList();
    }

    private static SetupTransactionState BeginSetupTransaction(string gameRoot, string payloadRoot)
    {
        string transactionId = Guid.NewGuid().ToString("N");
        string backupRoot = Path.Combine(gameRoot, SetupTransactionPrefix + transactionId);
        string snapshotRoot = Path.Combine(backupRoot, "snapshot");
        // Reject bad source/destination trees before creating a transaction or copying files.
        NcmmRuntimeIO.GuardTree(payloadRoot);
        foreach (string relative in GetManagedSetupPaths(gameRoot, payloadRoot))
            NcmmRuntimeIO.GuardTree(Path.Combine(gameRoot, relative));
        NcmmRuntimeIO.GuardPath(SetupPendingPath(gameRoot));
        Directory.CreateDirectory(snapshotRoot);

        SetupSnapshotManifest snapshot = new SetupSnapshotManifest();
        snapshot.schema = 2;
        snapshot.game_root = gameRoot;
        snapshot.entries = new List<SetupSnapshotEntry>();

        foreach (string relativePath in GetManagedSetupPaths(gameRoot, payloadRoot))
        {
            string source = Path.Combine(gameRoot, relativePath);
            bool isDirectory = Directory.Exists(source);
            bool exists = isDirectory || File.Exists(source);
            SetupSnapshotEntry entry = new SetupSnapshotEntry();
            entry.relative_path = relativePath;
            entry.existed = exists;
            entry.directory = isDirectory;
            snapshot.entries.Add(entry);

            if (!exists) continue;
            string destination = Path.Combine(snapshotRoot, relativePath);
            if (isDirectory)
            {
                entry.files = NcmmRuntimeIO.TreeIdentity(source);
                CopyDirectoryTree(source, destination);
                if (!NcmmRuntimeIO.SameIdentity(entry.files, NcmmRuntimeIO.TreeIdentity(destination)))
                    throw new IOException("Setup directory changed while taking a snapshot: " + relativePath);
            }
            else
            {
                string parent = Path.GetDirectoryName(destination);
                if (!String.IsNullOrEmpty(parent)) Directory.CreateDirectory(parent);
                entry.sha256 = Sha256(source);
                NcmmRuntimeIO.CopyDurable(source, destination);
                if (Sha256(destination) != entry.sha256)
                    throw new IOException("Setup file changed while taking a snapshot: " + relativePath);
            }
        }

        WriteJsonAtomic(Path.Combine(backupRoot, "snapshot.json"), snapshot);

        SetupTransactionState state = new SetupTransactionState();
        state.schema = 1;
        state.transaction_id = transactionId;
        state.game_root = gameRoot;
        state.backup_root = backupRoot;
        state.phase = "snapshot_complete";
        state.created_utc = DateTime.UtcNow.ToString("o");
        WriteJsonAtomic(SetupPendingPath(gameRoot), state);
        MaybeInjectSetupFailure(state.phase);
        return state;
    }

    private static SetupTransactionState ReadPendingSetupTransaction(string gameRoot)
    {
        string pending = SetupPendingPath(gameRoot);
        if (!File.Exists(pending)) return null;
        SetupTransactionState state;
        try
        {
            state = new JavaScriptSerializer().Deserialize<SetupTransactionState>(File.ReadAllText(pending));
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException("NCMM setup transaction marker is corrupt; refusing to continue.", ex);
        }
        if (state == null || state.schema != 1 || String.IsNullOrWhiteSpace(state.transaction_id) ||
            String.IsNullOrWhiteSpace(state.game_root) || String.IsNullOrWhiteSpace(state.backup_root))
            throw new InvalidOperationException("NCMM setup transaction marker is incomplete; refusing to continue.");

        string expectedRoot = Path.GetFullPath(gameRoot).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string markerRoot = Path.GetFullPath(state.game_root).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        if (!String.Equals(expectedRoot, markerRoot, StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("NCMM setup transaction marker targets a different game root.");
        AssertTransactionPathSafe(gameRoot, state.backup_root);
        return state;
    }

    private static void RestoreSetupTransaction(SetupTransactionState state)
    {
        AssertTransactionPathSafe(state.game_root, state.backup_root);
        NcmmRuntimeIO.GuardTree(state.backup_root);
        string manifestPath = Path.Combine(state.backup_root, "snapshot.json");
        if (!File.Exists(manifestPath))
            throw new InvalidOperationException("NCMM setup rollback snapshot is missing; live files preserved.");
        SetupSnapshotManifest snapshot = new JavaScriptSerializer()
            .Deserialize<SetupSnapshotManifest>(File.ReadAllText(manifestPath));
        if (snapshot == null || snapshot.schema != 2 || snapshot.entries == null || snapshot.entries.Count == 0)
            throw new InvalidOperationException("NCMM rollback snapshot has no verifiable inventory; live files preserved.");
        string root = NcmmRuntimeIO.Root(state.game_root);
        if (String.IsNullOrWhiteSpace(snapshot.game_root) ||
            !String.Equals(root, NcmmRuntimeIO.Root(snapshot.game_root), StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("NCMM rollback snapshot targets a different game root.");
        string snapshotRoot = Path.Combine(state.backup_root, "snapshot");
        HashSet<string> seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        // Complete validation before the FIRST mutation, including every file in directory snapshots.
        foreach (SetupSnapshotEntry entry in snapshot.entries)
        {
            if (entry == null || String.IsNullOrWhiteSpace(entry.relative_path))
                throw new InvalidOperationException("Invalid setup rollback path.");
            string rel = entry.relative_path.Replace('\\', '/');
            string[] parts = rel.Split('/');
            bool allowed = rel == "cataclysm-tiles.exe" || rel == "cataclysm-tiles.vanilla.exe" || rel == "ncmm" ||
                           (parts.Length == 2 && parts[0] == "code_mods" && SafeModuleDirectoryName(parts[1]));
#if NCMM_FIRST_PERSON_PREVIEW
            allowed = allowed || rel == "cataclysm-tiles.ncmm.exe";
#endif
            if (!allowed || !seen.Add(rel)) throw new InvalidOperationException("Unexpected or duplicate rollback path: " + rel);
            string target = Path.Combine(root, entry.relative_path);
            string source = Path.Combine(snapshotRoot, entry.relative_path);
            NcmmRuntimeIO.GuardTree(target);
            if (!entry.existed) continue;
            if (entry.directory)
            {
                if (!Directory.Exists(source) || !NcmmRuntimeIO.SameIdentity(entry.files, NcmmRuntimeIO.TreeIdentity(source)))
                    throw new IOException("Damaged setup directory snapshot; live files preserved: " + rel);
            }
            else if (!File.Exists(source) || String.IsNullOrEmpty(entry.sha256) || Sha256(source) != entry.sha256)
                throw new IOException("Damaged setup file snapshot; live files preserved: " + rel);
        }
        // Stage all replacements first. Originals displaced below stay in the transaction until commit.
        string recovery = Path.Combine(state.backup_root, "recovery-" + Guid.NewGuid().ToString("N"));
        foreach (SetupSnapshotEntry entry in snapshot.entries)
        {
            if (!entry.existed) continue;
            string staged = Path.Combine(recovery, "staged", entry.relative_path);
            string source = Path.Combine(snapshotRoot, entry.relative_path);
            if (entry.directory) CopyDirectoryTree(source, staged);
            else NcmmRuntimeIO.CopyDurable(source, staged);
        }
        foreach (SetupSnapshotEntry entry in snapshot.entries)
        {
            string target = Path.Combine(root, entry.relative_path);
            string staged = Path.Combine(recovery, "staged", entry.relative_path);
            string previous = Path.Combine(recovery, "previous", entry.relative_path);
            NcmmRuntimeIO.GuardTree(target);
            Directory.CreateDirectory(Path.GetDirectoryName(previous));
            bool hadDirectory = Directory.Exists(target);
            bool hadFile = File.Exists(target);
            if (entry.existed && !entry.directory && hadFile)
            {
                File.Replace(staged, target, previous);
                continue;
            }
            if (hadDirectory) Directory.Move(target, previous);
            else if (hadFile) File.Move(target, previous);
            try
            {
                if (!entry.existed) continue;
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                if (entry.directory) Directory.Move(staged, target);
                else File.Move(staged, target);
            }
            catch
            {
                if (!Directory.Exists(target) && !File.Exists(target))
                {
                    if (hadDirectory) Directory.Move(previous, target);
                    else if (hadFile) File.Move(previous, target);
                }
                throw; // durable snapshot and any displaced files remain recoverable
            }
        }
    }

    private static void CleanupSetupTransaction(SetupTransactionState state)
    {
        string pending = SetupPendingPath(state.game_root);
        if (File.Exists(pending)) File.Delete(pending);
        if (Directory.Exists(state.backup_root)) DeletePath(state.backup_root);
    }

    internal static bool RecoverPendingSetupTransaction(string gameRoot)
    {
        using (NcmmInstallLock gate = NcmmInstallLock.Acquire(gameRoot))
        {
            AssertGameNotRunning(gameRoot);

        gameRoot = Path.GetFullPath(gameRoot.Trim());
        SetupTransactionState state = ReadPendingSetupTransaction(gameRoot);
        if (state == null) return false;

        if (!String.Equals(state.phase, "committed", StringComparison.Ordinal))
            RestoreSetupTransaction(state);
        CleanupSetupTransaction(state);
        return true;
            }
    }

    private static void UpdateSetupTransactionPhase(string gameRoot, string phase)
    {
        SetupTransactionState state = ReadPendingSetupTransaction(gameRoot);
        if (state == null)
            throw new InvalidOperationException("NCMM setup transaction marker disappeared during install.");
        state.phase = phase;
        WriteJsonAtomic(SetupPendingPath(gameRoot), state);
        MaybeInjectSetupFailure(phase);
    }

    private static void MaybeInjectSetupFailure(string phase)
    {
        if (!String.Equals(Environment.GetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE"), "1",
                           StringComparison.Ordinal))
            return;

        string throwPhase = Environment.GetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE");
        if (String.Equals(throwPhase, phase, StringComparison.Ordinal))
            throw new IOException("Injected NCMM setup failure at phase: " + phase);

        string abortPhase = Environment.GetEnvironmentVariable("NCMM_SETUP_MATRIX_ABORT_PHASE");
        if (String.Equals(abortPhase, phase, StringComparison.Ordinal))
            Environment.Exit(86);
    }

    private static void CommitSetupTransaction(string gameRoot)
    {
        SetupTransactionState state = ReadPendingSetupTransaction(gameRoot);
        if (state == null)
            throw new InvalidOperationException("NCMM setup transaction marker disappeared before commit.");
        state.phase = "committed";
        WriteJsonAtomic(SetupPendingPath(gameRoot), state);
        CleanupSetupTransaction(state);
    }

    private static void RemoveManagedModuleFiles(string destination, string expectedId)
    {
        if (!Directory.Exists(destination)) return;
        string manifestPath = Path.Combine(destination, "mod.json");
        if (!File.Exists(manifestPath)) return;

        SetupModuleManifest existing = ReadModuleManifest(manifestPath);
        if (existing == null || !String.Equals(existing.id, expectedId, StringComparison.Ordinal))
            throw new InvalidOperationException(
                "Refusing to remove files from module directory owned by a different module: " + destination);

        string dll = Path.Combine(destination, "ncmm_mod.dll");
        NcmmRuntimeIO.GuardTree(destination);
        if (File.Exists(dll)) File.Delete(dll);
        if (Directory.Exists(Path.Combine(destination, "persistent_data")))
        {
            // Keep the Host discovery entry for save-critical definitions, without runnable code.
            string disabled = Path.Combine(destination, "disabled");
            if (!File.Exists(disabled)) File.WriteAllText(disabled, "disabled-by-ncmm-setup");
        }
        else File.Delete(manifestPath);

        // Preserve user-created disabled markers, notes and any future module state files.
        if (Directory.GetFileSystemEntries(destination).Length == 0)
            Directory.Delete(destination);
    }

    private static void VerifyPackagedDirectory(string sourceRoot, string destinationRoot)
    {
        string sourcePrefix = Path.GetFullPath(sourceRoot)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) +
            Path.DirectorySeparatorChar;
        foreach (string sourceFile in Directory.GetFiles(sourceRoot, "*", SearchOption.AllDirectories))
        {
            string fullSource = Path.GetFullPath(sourceFile);
            if (!fullSource.StartsWith(sourcePrefix, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Installer verification source escaped payload root.");

            string relative = fullSource.Substring(sourcePrefix.Length);
            string destinationFile = Path.Combine(destinationRoot, relative);
            if (!File.Exists(destinationFile))
                throw new InvalidOperationException(
                    "Post-install verification failed; packaged module file is missing: " + relative);

            FileInfo sourceInfo = new FileInfo(sourceFile);
            FileInfo destinationInfo = new FileInfo(destinationFile);
            if (sourceInfo.Length != destinationInfo.Length ||
                !String.Equals(Sha256(sourceFile), Sha256(destinationFile), StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException(
                    "Post-install verification failed; packaged module file differs: " + relative);
        }
    }

    private static void VerifyInstalledPayload(string gameRoot, string payloadRoot, InstallResult result)
    {
        if (result == null)
            throw new InvalidOperationException("Post-install verification received no install result.");

        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        string ncmm = Path.Combine(gameRoot, "ncmm");
        string payloadMods = Path.Combine(payloadRoot, "code_mods");

        if (!File.Exists(exe) ||
            !String.Equals(Sha256(exe), result.BootstrapSha256, StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Post-install verification failed for NCMM bootstrap.");
        if (!File.Exists(vanilla) ||
            !String.Equals(Sha256(vanilla), result.VanillaSha256, StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Post-install verification failed for vanilla backup.");

        string recordedBootstrap = Path.Combine(ncmm, "bootstrap.sha256");
        string recordedVanilla = Path.Combine(ncmm, "vanilla.sha256");
        if (!File.Exists(recordedBootstrap) ||
            !String.Equals(File.ReadAllText(recordedBootstrap).Trim(), result.BootstrapSha256,
                           StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Post-install verification failed for bootstrap.sha256.");
        if (!File.Exists(recordedVanilla) ||
            !String.Equals(File.ReadAllText(recordedVanilla).Trim(), result.VanillaSha256,
                           StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Post-install verification failed for vanilla.sha256.");

        HashSet<string> expectedIds = new HashSet<string>(
            result.InstalledModuleIds ?? new List<string>(), StringComparer.Ordinal);
        foreach (SetupBundledModule module in DiscoverBundledModules(payloadMods))
        {
            bool expected = expectedIds.Contains(module.Manifest.id);
            bool installed = IsModuleInstalled(gameRoot, module.DirectoryName, module.Manifest.id);
            if (installed != expected)
                throw new InvalidOperationException(
                    "Post-install verification failed for module state: " + module.Manifest.id);
            if (expected)
            {
                VerifyPackagedDirectory(
                    module.SourceDirectory,
                    Path.Combine(gameRoot, "code_mods", module.DirectoryName));
            }
        }

        Dictionary<string, string> activeDirectories = new Dictionary<string, string>(StringComparer.Ordinal);
        string modsRoot = Path.Combine(gameRoot, "code_mods");
        foreach (string directory in Directory.GetDirectories(modsRoot))
        {
            if (!File.Exists(Path.Combine(directory, "ncmm_mod.dll")) ||
                !File.Exists(Path.Combine(directory, "mod.json")) || File.Exists(Path.Combine(directory, "disabled"))) continue;
            SetupModuleManifest manifest = ReadModuleManifest(Path.Combine(directory, "mod.json"));
            if (manifest == null || !expectedIds.Contains(manifest.id)) continue;
            if (activeDirectories.ContainsKey(manifest.id))
                throw new InvalidOperationException("Duplicate active module ID after installation: " + manifest.id);
            activeDirectories.Add(manifest.id, directory);
        }

        string installedStatePath = Path.Combine(ncmm, "installed-components.json");
        if (!File.Exists(installedStatePath))
            throw new InvalidOperationException("Post-install verification failed; installed-components.json is missing.");

        SetupInstalledComponents installedState;
        try
        {
            installedState = new JavaScriptSerializer()
                .Deserialize<SetupInstalledComponents>(File.ReadAllText(installedStatePath));
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException(
                "Post-install verification failed; installed-components.json is invalid.", ex);
        }

        if (installedState == null || installedState.schema != 1 ||
            !String.Equals(installedState.runtime_version, RuntimeVersion, StringComparison.Ordinal) ||
            installedState.components == null)
            throw new InvalidOperationException("Post-install verification failed for installed component state.");

        HashSet<string> stateIds = new HashSet<string>(
            installedState.components.Where(component => component != null)
                .Select(component => component.id),
            StringComparer.Ordinal);
        if (!stateIds.Contains("ncmm_host") || stateIds.Count != expectedIds.Count + 1 ||
            expectedIds.Any(id => !stateIds.Contains(id)))
            throw new InvalidOperationException("Post-install verification found an installed component mismatch.");
    }

    private static void VerifySetupFinalized(string gameRoot, SetupTransactionState transaction)
    {
        if (File.Exists(SetupPendingPath(gameRoot)))
            throw new InvalidOperationException(
                "Post-install verification failed; setup transaction marker still exists.");
        if (transaction != null && Directory.Exists(transaction.backup_root))
            throw new InvalidOperationException(
                "Post-install verification failed; setup transaction backup still exists.");
    }

    internal static InstallResult Install(string gameRoot, string payloadRoot)
    {
        return Install(gameRoot, payloadRoot, null);
    }

    private static HashSet<string> ResolveSelectedModuleIds(
        List<SetupBundledModule> bundledModules, IEnumerable<string> selectedModuleIds)
    {
        HashSet<string> knownIds = new HashSet<string>(
            bundledModules.Select(module => module.Manifest.id), StringComparer.Ordinal);
        HashSet<string> selected = selectedModuleIds == null
            ? new HashSet<string>(knownIds, StringComparer.Ordinal)
            : new HashSet<string>(
                selectedModuleIds.Where(id => !String.IsNullOrWhiteSpace(id)).Select(id => id.Trim()),
                StringComparer.Ordinal);

        foreach (string id in selected)
        {
            if (!knownIds.Contains(id))
                throw new InvalidOperationException("Unknown bundled NCMM component selected: " + id);
        }
        return selected;
    }

    internal static InstallResult Install(string gameRoot, string payloadRoot, IEnumerable<string> selectedModuleIds)
    {
        // Read-only input validation precedes even creation of the persistent lock file.
        // Materialize a caller's lazy selection once; InstallCore revalidates that snapshot
        // after acquiring the lock, rather than enumerating external input a second time.
        gameRoot = NcmmRuntimeIO.Root(gameRoot);
        payloadRoot = NcmmRuntimeIO.Root(payloadRoot);
        NcmmRuntimeIO.GuardTree(payloadRoot);
        if (!File.Exists(Path.Combine(payloadRoot, "cataclysm-tiles.ncmm-bootstrap.exe")))
            throw new InvalidOperationException("Installer payload is incomplete: bootstrap missing.");
        HashSet<string> requested = ResolveSelectedModuleIds(
            DiscoverBundledModules(Path.Combine(payloadRoot, "code_mods")), selectedModuleIds);
#if NCMM_FIRST_PERSON_PREVIEW
        ValidateFirstPersonBundle(gameRoot, payloadRoot);
#endif
        using (NcmmInstallLock gate = NcmmInstallLock.Acquire(gameRoot))
        {

        gameRoot = Path.GetFullPath(gameRoot.Trim());
        payloadRoot = Path.GetFullPath(payloadRoot.Trim());

        AssertGameNotRunning(gameRoot);
        NcmmRuntimeIO.RecoverHost(gameRoot);
        RecoverPendingSetupTransaction(gameRoot);
        SetupTransactionState transaction = BeginSetupTransaction(gameRoot, payloadRoot);
        try
        {
            InstallResult result = InstallCore(gameRoot, payloadRoot, requested);
#if NCMM_FIRST_PERSON_PREVIEW
            InstallFirstPersonHost(gameRoot, payloadRoot);
#endif
            VerifyInstalledPayload(gameRoot, payloadRoot, result);
            UpdateSetupTransactionPhase(gameRoot, "ready_to_commit");
            CommitSetupTransaction(gameRoot);
            VerifySetupFinalized(gameRoot, transaction);
            result.CompletionVerified = true;
            return result;
        }
        catch (Exception installError)
        {
            try
            {
                SetupTransactionState pending = ReadPendingSetupTransaction(gameRoot);
                if (pending != null)
                {
                    RestoreSetupTransaction(pending);
                    CleanupSetupTransaction(pending);
                }
            }
            catch (Exception rollbackError)
            {
                throw new InvalidOperationException(
                    "NCMM setup failed and rollback also failed. Install error: " + installError.Message +
                    " | Rollback error: " + rollbackError.Message,
                    new AggregateException(installError, rollbackError));
            }
            throw;
        }
            }
    }

    private static InstallResult InstallCore(string gameRoot, string payloadRoot, IEnumerable<string> selectedModuleIds)
    {
        gameRoot = Path.GetFullPath(gameRoot.Trim());
        DetectedInstallation target = DescribeInstallation(gameRoot);

        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        string ncmm = Path.Combine(gameRoot, "ncmm");
        string mods = Path.Combine(gameRoot, "code_mods");
        string bootstrap = Path.Combine(payloadRoot, "cataclysm-tiles.ncmm-bootstrap.exe");
        string payloadMods = Path.Combine(payloadRoot, "code_mods");

        if (!File.Exists(bootstrap))
            throw new InvalidOperationException("Installer payload is incomplete: bootstrap missing.");

        List<SetupBundledModule> bundledModules = DiscoverBundledModules(payloadMods);
        SetupInstalledComponents previousState = ReadPreviousInstalledComponents(gameRoot);
        Dictionary<string, SetupInstalledComponent> previousById =
            new Dictionary<string, SetupInstalledComponent>(StringComparer.Ordinal);
        if (previousState != null && previousState.components != null)
            foreach (SetupInstalledComponent old in previousState.components)
                if (old != null && !String.IsNullOrEmpty(old.id) &&
                    !previousById.ContainsKey(old.id))
                    previousById[old.id] = old;
        HashSet<string> selected = ResolveSelectedModuleIds(bundledModules, selectedModuleIds);

        Directory.CreateDirectory(ncmm);
        Directory.CreateDirectory(mods);

        string bootstrapHash = Sha256(bootstrap);
        string currentHash = Sha256(exe);
        string installedHashFile = Path.Combine(ncmm, "bootstrap.sha256");
        string previousBootstrapHash = File.Exists(installedHashFile)
            ? File.ReadAllText(installedHashFile).Trim().ToLowerInvariant() : null;

        string legacyDir = Path.Combine(gameRoot, "cml");
        string legacyHashFile = Path.Combine(legacyDir, "bootstrap.sha256");
        string legacyBootstrapHash = File.Exists(legacyHashFile)
            ? File.ReadAllText(legacyHashFile).Trim().ToLowerInvariant() : null;
        bool legacyBootstrapInstalled = File.Exists(vanilla) && !String.IsNullOrEmpty(legacyBootstrapHash) &&
                                        String.Equals(currentHash, legacyBootstrapHash, StringComparison.OrdinalIgnoreCase);

        if (String.Equals(currentHash, bootstrapHash, StringComparison.OrdinalIgnoreCase))
        {
            if (!File.Exists(vanilla))
                throw new InvalidOperationException("NCMM bootstrap is present but vanilla backup is missing. Refusing to guess.");
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
            // Missing receipts cannot turn an unrecognized old bootstrap into a "vanilla" backup.
            if (File.Exists(vanilla) && String.IsNullOrWhiteSpace(previousBootstrapHash))
                throw new InvalidOperationException("Unknown executable with a missing bootstrap receipt. Existing vanilla backup preserved; repair the installation identity first.");
            try
            {
                System.Reflection.AssemblyName.GetAssemblyName(exe);
                throw new InvalidOperationException("Unrecognized managed launcher cannot be used as the native CDDA vanilla backup.");
            }
            catch (BadImageFormatException) { } // Native CDDA / synthetic test fixture, not a .NET launcher.
            if (File.Exists(vanilla))
            {
                string archive = Path.Combine(ncmm,
                    "cataclysm-tiles.vanilla.backup-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + ".exe");
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
        if (!File.Exists(vanilla))
            throw new InvalidOperationException("Vanilla backup post-install check failed.");

        string vanillaHash = Sha256(vanilla);
        File.WriteAllText(installedHashFile, bootstrapHash.ToLowerInvariant() + Environment.NewLine, Encoding.ASCII);
        File.WriteAllText(Path.Combine(ncmm, "vanilla.sha256"),
            vanillaHash.ToLowerInvariant() + Environment.NewLine, Encoding.ASCII);
        UpdateSetupTransactionPhase(gameRoot, "bootstrap_installed");

        List<string> installedIds = new List<string>();
        List<SetupInstalledComponent> componentState = new List<SetupInstalledComponent>();
        componentState.Add(new SetupInstalledComponent {
            id = "ncmm_host", version = RuntimeVersion, directory = null
        });

        foreach (SetupBundledModule module in bundledModules)
        {
            string destination = Path.Combine(mods, module.DirectoryName);
            if (selected.Contains(module.Manifest.id))
            {
                Directory.CreateDirectory(destination);
                AssertSafeModuleDestination(destination, module.Manifest.id);

                // A native module is a directory payload, not just a DLL + manifest.
                // Survivor, for example, ships data/ and persistent_data/ definitions
                // that must be present before Host module-data loading/finalization.
                // Copy the complete packaged module tree while preserving unrelated
                // user-created files already present in the destination.
                Dictionary<string, string> fileInventory = PackagedFileHashes(module.SourceDirectory);
                SetupInstalledComponent old;
                previousById.TryGetValue(module.Manifest.id, out old);
                bool sameDirectory = old != null && String.Equals(old.directory, module.DirectoryName, StringComparison.OrdinalIgnoreCase);
                RemoveStalePackagedFiles(destination, sameDirectory ? old : null, fileInventory);
                string disabled = Path.Combine(destination, "disabled");
                if (File.Exists(disabled) && File.ReadAllText(disabled) == "disabled-by-ncmm-setup") File.Delete(disabled);
                CopyDirectoryTree(module.SourceDirectory, destination);
                installedIds.Add(module.Manifest.id);
                componentState.Add(new SetupInstalledComponent {
                    id = module.Manifest.id,
                    version = module.Manifest.version,
                    directory = module.DirectoryName,
                    files = fileInventory
                });
            }
            else
            {
                RemoveManagedModuleFiles(destination, module.Manifest.id);
            }
        }

        // An upgrade may remove a previously bundled module entirely. Only touch
        // modules positively identified by the old NCMM installation receipt.
        foreach (SetupInstalledComponent old in previousById.Values)
        {
            if (old.id == "ncmm_host" || !SafeModuleDirectoryName(old.directory)) continue;
            SetupBundledModule replacement = bundledModules.FirstOrDefault(m => m.Manifest.id == old.id);
            if (replacement != null && String.Equals(replacement.DirectoryName, old.directory, StringComparison.OrdinalIgnoreCase)) continue;
            RemoveManagedModuleFiles(Path.Combine(mods, old.directory), old.id);
        }

        SetupInstalledComponents installedState = new SetupInstalledComponents();
        installedState.schema = 1;
        installedState.runtime_version = RuntimeVersion;
        installedState.updated_utc = DateTime.UtcNow.ToString("o");
        installedState.components = componentState;
        string installedStateJson = new JavaScriptSerializer().Serialize(installedState);
        File.WriteAllText(Path.Combine(ncmm, "installed-components.json"),
            installedStateJson + Environment.NewLine, new UTF8Encoding(false));
        UpdateSetupTransactionPhase(gameRoot, "modules_installed");

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
        result.InstalledModuleIds = installedIds;
        return result;
    }


    // Setup downloads a matching certified Host itself: module installation alone
    // does not prove that a new inventory renderer will be used by this CDDA build.
    // Kept separate from InstallCore: network failure cannot corrupt/roll back a
    // successfully installed local bootstrap and native modules.
    internal static SetupHostSyncResult SyncCertifiedHost(string gameRoot)
    {
#if NCMM_FIRST_PERSON_PREVIEW
        return FirstPersonHostStatus(gameRoot);
#endif
        using (NcmmInstallLock gate = NcmmInstallLock.Acquire(gameRoot))
        {
            AssertGameNotRunning(gameRoot);
            NcmmRuntimeIO.GuardTree(Path.Combine(gameRoot, "ncmm"));
            NcmmRuntimeIO.RecoverHost(gameRoot);

        gameRoot = Path.GetFullPath(gameRoot.Trim());
        AssertGameNotRunning(gameRoot);
        string ncmm = Path.Combine(gameRoot, "ncmm");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        if (!File.Exists(vanilla))
            throw new InvalidOperationException("Vanilla executable missing; refusing Host update.");
        string vanillaSha = Sha256(vanilla).ToLowerInvariant();
        string commit = ReadSourceCommit(gameRoot) ?? "";

        SetupCertifiedHostFeed feed;
        try
        {
            // Bypass caches that can otherwise keep the previous paper-doll Host
            // even after the public certified feed has advanced.
            string url = "https://raw.githubusercontent.com/Neversalimus/NCMM/main/feed/index.json" +
                         "?ncmm_setup_refresh=" + DateTime.UtcNow.Ticks.ToString();
            System.Net.ServicePointManager.SecurityProtocol |=
                (System.Net.SecurityProtocolType)3072; // TLS 1.2, .NET Framework compatible.
            using (SetupDownloadClient web = new SetupDownloadClient())
            {
                web.Headers[System.Net.HttpRequestHeader.UserAgent] = "NCMM-Setup/" + RuntimeVersion;
                feed = new JavaScriptSerializer().Deserialize<SetupCertifiedHostFeed>(
                    web.DownloadString(url));
            }
        }
        catch (Exception ex)
        {
            return new SetupHostSyncResult {
                Ready = false,
                Message = "Certified Host feed unavailable: " + ex.Message +
                          ". NCMM modules installed, but the newest renderer is NOT verified."
            };
        }

        SetupCertifiedHostEntry entry;
        string rejection;
        if (!TrySelectCertifiedHost(feed, vanillaSha, commit, out entry, out rejection))
            return new SetupHostSyncResult {
                Ready = false,
                Message = rejection + ". NCMM installed, but Host was not updated."
            };

        string target = Path.Combine(gameRoot, "cataclysm-tiles.ncmm.exe");
        string bindingPath = Path.Combine(ncmm, "host.binding.json");
        string expectedHash = entry.host_sha256.ToLowerInvariant();
        string actual = File.Exists(target) ? Sha256(target).ToLowerInvariant() : null;
        string newBindingJson = new JavaScriptSerializer().Serialize(new SetupHostBinding {
            vanilla_sha256 = vanillaSha,
            host_sha256 = expectedHash,
            source_commit = entry.source_commit,
            upstream_tag = entry.upstream_tag,
            patch_revision = entry.patch_revision,
            ncmm_version = entry.ncmm_version,
            loader_api = entry.loader_api,
            installed_utc = DateTime.UtcNow.ToString("o")
        }) + Environment.NewLine;

        string staged = Path.Combine(ncmm, "host.setup-" + Guid.NewGuid().ToString("N") + ".exe");
        bool changed = !String.Equals(actual, expectedHash, StringComparison.OrdinalIgnoreCase);
        try
        {
            if (changed)
            {
                using (SetupDownloadClient web = new SetupDownloadClient())
                {
                    web.Headers[System.Net.HttpRequestHeader.UserAgent] = "NCMM-Setup/" + RuntimeVersion;
                    web.DownloadFile(entry.host_url, staged);
                }
            }
            else NcmmRuntimeIO.CopyDurable(target, staged);
            AssertGameNotRunning(gameRoot);
            NcmmRuntimeIO.PublishHost(gameRoot, staged, newBindingJson, expectedHash);
            return new SetupHostSyncResult {
                Ready = true, HostSha256 = expectedHash, PatchRevision = entry.patch_revision,
                Message = (changed ? "Certified Host installed" : "Certified Host already current") +
                    ": " + entry.upstream_tag + ", patch " + entry.patch_revision.Substring(0, 12) +
                    ". Exact SHA256 and bootstrap binding VERIFIED."
            };
        }
        catch (Exception ex)
        {
            return new SetupHostSyncResult {
                Ready = false, Message = "Certified Host update was not verified: " + ex.Message +
                    ". Recovery material is preserved; do not assume the new renderer is active."
            };
        }
        finally { if (File.Exists(staged)) File.Delete(staged); }
            }
    }

    // Pure, deterministic validation also used by the installation-matrix tests.
    internal static bool TrySelectCertifiedHost(SetupCertifiedHostFeed feed, string vanillaSha,
                                                 string sourceCommit, out SetupCertifiedHostEntry entry,
                                                 out string reason)
    {
        entry = null;
        reason = "No compatible certified Host";
        if (feed == null || feed.schema != 1 || feed.loader_api != 1 ||
            feed.runtime_version != RuntimeVersion || feed.hosts == null ||
            String.IsNullOrEmpty(feed.patch_revision) ||
            !System.Text.RegularExpressions.Regex.IsMatch(feed.patch_revision, "^[0-9a-f]{64}$"))
        {
            reason = "Certified Host feed schema, runtime or revision mismatch";
            return false;
        }
        if (String.IsNullOrWhiteSpace(vanillaSha) ||
            !feed.hosts.TryGetValue(vanillaSha.ToLowerInvariant(), out entry) || entry == null)
        {
            reason = "No certified Host for the exact installed CDDA executable SHA256";
            entry = null;
            return false;
        }
        if (entry.loader_api != 1 || entry.ncmm_version != RuntimeVersion ||
            entry.patch_revision != feed.patch_revision ||
            String.IsNullOrEmpty(entry.host_sha256) ||
            !System.Text.RegularExpressions.Regex.IsMatch(entry.host_sha256, "^[0-9a-f]{64}$") ||
            String.IsNullOrWhiteSpace(entry.source_commit) ||
            !System.Text.RegularExpressions.Regex.IsMatch(entry.source_commit, "^[0-9a-f]{40}$"))
        {
            reason = "Certified Host entry identity or revision mismatch";
            entry = null;
            return false;
        }
        if (!NcmmRuntimeIO.SourceMatches(entry.source_commit, sourceCommit))
        {
            reason = "Certified Host source commit does not match this CDDA build";
            entry = null;
            return false;
        }
        Uri hostUri;
        if (!Uri.TryCreate(entry.host_url, UriKind.Absolute, out hostUri) ||
            hostUri.Scheme != Uri.UriSchemeHttps ||
            !String.Equals(hostUri.Host, "github.com", StringComparison.OrdinalIgnoreCase) ||
            !hostUri.AbsolutePath.StartsWith(
                "/Neversalimus/NCMM/releases/download/ncmm-host-", StringComparison.Ordinal) ||
            !hostUri.AbsolutePath.EndsWith("/cataclysm-tiles.ncmm.exe", StringComparison.Ordinal))
        {
            reason = "Certified Host URL is outside the trusted NCMM release channel";
            entry = null;
            return false;
        }
        reason = "OK";
        return true;
    }

    internal static void RestoreVanilla(string gameRoot)
    {
        using (NcmmInstallLock gate = NcmmInstallLock.Acquire(gameRoot))
        {
            if (NcmmRuntimeIO.HasSaveCriticalData(gameRoot))
                throw new InvalidOperationException("Native save-compatibility definitions are installed. Disable individual modules instead; restoring vanilla can make existing NCMM saves unreadable. No files changed.");
            NcmmRuntimeIO.RecoverHost(gameRoot);

        gameRoot = Path.GetFullPath(gameRoot.Trim());
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        AssertGameNotRunning(gameRoot);
        if (!File.Exists(vanilla))
            throw new InvalidOperationException("cataclysm-tiles.vanilla.exe not found. Nothing safe to restore.");
        string hashFile = Path.Combine(gameRoot, "ncmm", "vanilla.sha256");
        if (!File.Exists(hashFile))
            throw new InvalidOperationException("Vanilla checksum receipt missing; refusing unsafe restore.");
        string expected = File.ReadAllText(hashFile).Trim();
        if (expected.Length != 64 || !expected.All(Uri.IsHexDigit) ||
            !String.Equals(Sha256(vanilla), expected, StringComparison.OrdinalIgnoreCase))
            throw new InvalidOperationException("Vanilla backup SHA256 mismatch; refusing unsafe restore.");

        string staged = exe + ".ncmm-restore-" + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.Copy(vanilla, staged, false);
            if (!String.Equals(Sha256(staged), expected, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Staged vanilla SHA256 mismatch.");
            if (File.Exists(exe)) File.Replace(staged, exe, null);
            else File.Move(staged, exe);
            if (!String.Equals(Sha256(exe), expected, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Restored vanilla SHA256 mismatch.");
        }
        finally
        {
            if (File.Exists(staged)) File.Delete(staged);
        }
            }
    }

    internal static string RepairState(string gameRoot)
    {
        using (NcmmInstallLock gate = NcmmInstallLock.Acquire(gameRoot))
        {
            AssertGameNotRunning(gameRoot);
            NcmmRuntimeIO.GuardTree(Path.Combine(gameRoot, "ncmm"));

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

        result.AppendLine(DateTime.UtcNow.ToString("o") + " NCMM v" + RuntimeVersion + " safe state repair");
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
