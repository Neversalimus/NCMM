// Shared by Setup, Bootstrap and recovery. No game/save data is rewritten here.
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

// Explicit target prevents old MAX_PATH defaults in Framework csc-built executables.
// All Setup/Bootstrap/harness programs compile this common file; .NET 4.6.2+ required.
[assembly: System.Runtime.Versioning.TargetFramework(".NETFramework,Version=v4.6.2")]

internal sealed class NcmmInstallLock : IDisposable
{
    private sealed class Held { internal FileStream Stream; internal int Depth; }
    [ThreadStatic] private static Dictionary<string, Held> held;
    private readonly string root;
    private bool disposed;
    private NcmmInstallLock(string rootValue) { root = rootValue; }

    internal static NcmmInstallLock Acquire(string gameRoot)
    {
        string root = NcmmRuntimeIO.Root(gameRoot);
        NcmmRuntimeIO.GuardPath(root);
        if (!Directory.Exists(root)) throw new DirectoryNotFoundException(root);
        if (held == null) held = new Dictionary<string, Held>(StringComparer.OrdinalIgnoreCase);
        Held entry;
        if (held.TryGetValue(root, out entry)) { ++entry.Depth; return new NcmmInstallLock(root); }
        // A retained file lock also coordinates aliases and different Windows sessions.
        // Never unlink the lock file: that would permit two independent lock owners.
        string path = Path.Combine(root, ".ncmm-install.lock");
        NcmmRuntimeIO.GuardPath(path);
        try
        {
            entry = new Held { Stream = new FileStream(path, FileMode.OpenOrCreate,
                FileAccess.ReadWrite, FileShare.None), Depth = 1 };
        }
        catch (IOException ex)
        {
            throw new IOException("NCMM: this installation is busy (game, setup or recovery). Close it and retry.", ex);
        }
        held.Add(root, entry);
        return new NcmmInstallLock(root);
    }

    public void Dispose()
    {
        if (disposed) return;
        disposed = true;
        Held entry = held[root];
        if (--entry.Depth == 0) { entry.Stream.Dispose(); held.Remove(root); }
    }
}

internal sealed class NcmmHostSnapshot
{
    public string path { get; set; }
    public bool existed { get; set; }
    public string sha256 { get; set; }
}
internal sealed class NcmmHostJournal
{
    public int schema { get; set; }
    public string id { get; set; }
    public string phase { get; set; }
    public string host_sha256 { get; set; }
    public string binding_sha256 { get; set; }
    public List<NcmmHostSnapshot> files { get; set; }
}

internal static class NcmmRuntimeIO
{
    private const string HostJournalName = ".ncmm-host.pending.json";
    private static readonly string[] HostFiles = {
        "cataclysm-tiles.ncmm.exe", "ncmm/host.binding.json"
    };

    internal static string Root(string path)
    {
        string full = Path.GetFullPath(path.Trim());
        return full.Length == Path.GetPathRoot(full).Length ? full :
            full.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
    }

    internal static bool SourceMatches(string fullCommit, string versionCommit)
    {
        if (fullCommit == null || !Regex.IsMatch(fullCommit, "^[0-9a-fA-F]{40}$")) return false;
        if (String.IsNullOrWhiteSpace(versionCommit)) return true; // exact vanilla digest still required
        Match m = Regex.Match(versionCommit.Trim(), "^([0-9a-fA-F]{7,40})(?:-dirty)?$");
        return m.Success && fullCommit.StartsWith(m.Groups[1].Value, StringComparison.OrdinalIgnoreCase);
    }

    internal static void GuardPath(string path)
    {
        string current = Path.GetFullPath(path);
        while (!String.IsNullOrEmpty(current))
        {
            try
            {
                if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
                    throw new IOException("NCMM refuses a reparse point in a managed path: " + current);
            }
            catch (FileNotFoundException) { }
            catch (DirectoryNotFoundException) { }
            current = Path.GetDirectoryName(current);
        }
    }

    internal static void GuardTree(string path)
    {
        GuardPath(path);
        if (!Directory.Exists(path)) return;
        foreach (string child in Directory.GetFileSystemEntries(path))
        {
            GuardPath(child); // check before descending, not after following a junction
            if (Directory.Exists(child)) GuardTree(child);
        }
    }

    internal static string Hash(string path)
    {
        GuardPath(path);
        using (FileStream stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (SHA256 hash = SHA256.Create())
            return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }

    internal static void CopyDurable(string source, string target)
    {
        GuardPath(source); GuardPath(target);
        Directory.CreateDirectory(Path.GetDirectoryName(target));
        using (FileStream input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (FileStream output = new FileStream(target, FileMode.CreateNew, FileAccess.Write,
                                                  FileShare.None, 65536, FileOptions.WriteThrough))
        { input.CopyTo(output); output.Flush(true); }
    }

    internal static void PublishFile(string staged, string target)
    {
        GuardPath(staged); GuardPath(target);
        Directory.CreateDirectory(Path.GetDirectoryName(target));
        if (File.Exists(target)) File.Replace(staged, target, null);
        else File.Move(staged, target);
    }

    internal static void WriteTextAtomic(string path, string text)
    {
        GuardPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        string staged = path + ".tmp-" + Guid.NewGuid().ToString("N");
        try
        {
            byte[] bytes = new UTF8Encoding(false).GetBytes(text);
            using (FileStream output = new FileStream(staged, FileMode.CreateNew, FileAccess.Write,
                                                      FileShare.None, 4096, FileOptions.WriteThrough))
            { output.Write(bytes, 0, bytes.Length); output.Flush(true); }
            PublishFile(staged, path);
        }
        finally { if (File.Exists(staged)) File.Delete(staged); }
    }

    internal static Dictionary<string, string> TreeIdentity(string directory)
    {
        GuardTree(directory);
        Dictionary<string, string> result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        string prefix = Root(directory) + Path.DirectorySeparatorChar;
        foreach (string child in Directory.GetFileSystemEntries(directory, "*", SearchOption.AllDirectories))
        {
            string relative = Path.GetFullPath(child).Substring(prefix.Length).Replace('\\', '/');
            result.Add(relative, Directory.Exists(child) ? "directory" : Hash(child));
        }
        return result;
    }

    internal static bool SameIdentity(Dictionary<string, string> expected, Dictionary<string, string> actual)
    {
        if (expected == null || expected.Count != actual.Count) return false;
        foreach (KeyValuePair<string, string> item in expected)
        {
            string value;
            if (!actual.TryGetValue(item.Key, out value) ||
                !String.Equals(item.Value, value, StringComparison.OrdinalIgnoreCase)) return false;
        }
        return true;
    }

    internal static bool HasSaveCriticalData(string gameRoot)
    {
        string mods = Path.Combine(gameRoot, "code_mods");
        GuardPath(mods);
        if (!Directory.Exists(mods)) return false;
        foreach (string dir in Directory.GetDirectories(mods))
        {
            string data = Path.Combine(dir, "persistent_data");
            GuardTree(data);
            if (Directory.Exists(data) && Directory.GetFiles(data, "*.json", SearchOption.AllDirectories).Length != 0)
                return true;
        }
        return false;
    }

    private static string HostBackup(string root, string id)
    {
        if (id == null || !Regex.IsMatch(id, "^[0-9a-f]{32}$"))
            throw new IOException("Invalid NCMM Host transaction identity; no files changed.");
        return Path.Combine(root, ".ncmm-host-tx-" + id);
    }

    private static void HostPhase(string root, NcmmHostJournal journal, string phase)
    {
        journal.phase = phase;
        WriteTextAtomic(Path.Combine(root, HostJournalName), new JavaScriptSerializer().Serialize(journal));
        if (Environment.GetEnvironmentVariable("NCMM_HOST_MATRIX_TEST_MODE") != "1") return;
        if (Environment.GetEnvironmentVariable("NCMM_HOST_MATRIX_ABORT_PHASE") == phase) Environment.Exit(86);
        if (Environment.GetEnvironmentVariable("NCMM_HOST_MATRIX_THROW_PHASE") == phase)
            throw new IOException("Injected Host transaction failure: " + phase);
    }

    private static void FinishHost(string root, NcmmHostJournal journal)
    {
        // Keep the rollback material even after commit, independently of download success.
        // Backup directories are immutable recovery archives. Only the active journal is removed.
        File.Delete(Path.Combine(root, HostJournalName));
    }

    internal static bool RecoverHost(string gameRoot)
    {
        string root = Root(gameRoot);
        using (NcmmInstallLock gate = NcmmInstallLock.Acquire(root))
        {
            string path = Path.Combine(root, HostJournalName);
            GuardPath(path);
            if (!File.Exists(path)) return false;
            NcmmHostJournal journal = new JavaScriptSerializer().Deserialize<NcmmHostJournal>(File.ReadAllText(path));
            if (journal == null || journal.schema != 1 || journal.files == null || journal.files.Count != 2 ||
                (journal.phase != "prepared" && journal.phase != "host_published" &&
                 journal.phase != "binding_published" && journal.phase != "committed"))
                throw new IOException("Invalid NCMM Host recovery journal; live files preserved.");
            string backup = HostBackup(root, journal.id);
            GuardTree(backup);
            for (int i = 0; i < HostFiles.Length; ++i)
            {
                NcmmHostSnapshot file = journal.files[i];
                if (file == null || file.path != HostFiles[i]) throw new IOException("Invalid Host recovery path.");
                GuardPath(Path.Combine(root, file.path));
            }
            if (journal.phase == "committed")
            {
                if (Hash(Path.Combine(root, HostFiles[0])) != journal.host_sha256 ||
                    Hash(Path.Combine(root, HostFiles[1])) != journal.binding_sha256)
                    throw new IOException("Committed Host transaction no longer matches its files; recovery preserved.");
                FinishHost(root, journal);
                return true;
            }
            // Validate BOTH snapshots in full before any live file is replaced.
            foreach (NcmmHostSnapshot file in journal.files)
                if (file.existed && Hash(Path.Combine(backup, file.path)) != file.sha256)
                    throw new IOException("Damaged Host recovery snapshot; live files preserved: " + file.path);
            string recovery = Path.Combine(backup, "recovery-" + Guid.NewGuid().ToString("N"));
            foreach (NcmmHostSnapshot file in journal.files)
                if (file.existed) CopyDurable(Path.Combine(backup, file.path), Path.Combine(recovery, file.path));
            foreach (NcmmHostSnapshot file in journal.files)
            {
                string target = Path.Combine(root, file.path);
                if (file.existed) PublishFile(Path.Combine(recovery, file.path), target);
                else if (File.Exists(target))
                {
                    string displaced = Path.Combine(recovery, "displaced-" + Path.GetFileName(target));
                    Directory.CreateDirectory(recovery);
                    File.Move(target, displaced);
                }
            }
            FinishHost(root, journal);
            return true;
        }
    }

    internal static void PublishHost(string gameRoot, string stagedHost, string bindingJson, string expectedHash)
    {
        string root = Root(gameRoot);
        using (NcmmInstallLock gate = NcmmInstallLock.Acquire(root))
        {
            RecoverHost(root);
            if (Hash(stagedHost) != expectedHash.ToLowerInvariant()) throw new IOException("Staged Host SHA256 mismatch.");
            NcmmHostJournal journal = new NcmmHostJournal { schema = 1, id = Guid.NewGuid().ToString("N"),
                host_sha256 = expectedHash.ToLowerInvariant(), files = new List<NcmmHostSnapshot>() };
            string backup = HostBackup(root, journal.id);
            Directory.CreateDirectory(backup);
            foreach (string relative in HostFiles)
            {
                string target = Path.Combine(root, relative);
                GuardPath(target);
                if (Directory.Exists(target)) throw new IOException("Host destination is a directory: " + target);
                NcmmHostSnapshot file = new NcmmHostSnapshot { path = relative, existed = File.Exists(target) };
                if (file.existed)
                {
                    file.sha256 = Hash(target);
                    CopyDurable(target, Path.Combine(backup, relative));
                    if (Hash(Path.Combine(backup, relative)) != file.sha256) throw new IOException("Host snapshot changed while copying.");
                }
                journal.files.Add(file);
            }
            string stagedBinding = Path.Combine(backup, "new-binding.json");
            WriteTextAtomic(stagedBinding, bindingJson);
            journal.binding_sha256 = Hash(stagedBinding);
            try
            {
                HostPhase(root, journal, "prepared");
                PublishFile(stagedHost, Path.Combine(root, HostFiles[0]));
                HostPhase(root, journal, "host_published");
                PublishFile(stagedBinding, Path.Combine(root, HostFiles[1]));
                HostPhase(root, journal, "binding_published");
                if (Hash(Path.Combine(root, HostFiles[0])) != journal.host_sha256 ||
                    Hash(Path.Combine(root, HostFiles[1])) != journal.binding_sha256)
                    throw new IOException("Host transaction post-write verification failed.");
                HostPhase(root, journal, "committed");
                FinishHost(root, journal);
            }
            catch
            {
                RecoverHost(root); // if damaged, refuses recovery without deleting live files
                throw;
            }
        }
    }
}
