using System;
using File = NcmmFile;
using Directory = NcmmDirectory;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

internal sealed class FirstPersonBundle
{
    public int schema { get; set; }
    public string source_commit { get; set; }
    public string upstream_tag { get; set; }
    public string vanilla_sha256 { get; set; }
    public string[] supported_vanilla_sha256 { get; set; }
    public string host_sha256 { get; set; }
    public string patch_revision { get; set; }
    public string ncmm_source_commit { get; set; }
    public Dictionary<string, string> files { get; set; }
}

// Compiled only into the explicitly labelled preview installer. The normal
// installer and bootstrap do not accept or interpret experimental bundles.
internal static partial class SetupCore
{
    private static bool PreviewHash(string value)
    {
        return value != null && Regex.IsMatch(value, "^[0-9a-f]{64}$");
    }

    private static bool FirstPersonTarget(FirstPersonBundle bundle)
    {
        return bundle != null &&
            ((bundle.source_commit == "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83" &&
              bundle.upstream_tag == "cdda-experimental-2026-10-01-1040") ||
             (bundle.source_commit == "074aa98bd5be3de4c35f154082db32a0e63bb0f1" &&
              bundle.upstream_tag == "cdda-experimental-2026-10-06-1807"));
    }

    private static bool FirstPersonVanillaMatches(FirstPersonBundle bundle, string hash)
    {
        if (bundle.supported_vanilla_sha256 == null) return hash == bundle.vanilla_sha256;
        return Array.IndexOf(bundle.supported_vanilla_sha256, hash) >= 0;
    }

    private static bool FirstPersonReceiptMatches(string path, string currentHash)
    {
        return File.Exists(path) && String.Equals(File.ReadAllText(path).Trim(), currentHash,
            StringComparison.OrdinalIgnoreCase);
    }

    private static FirstPersonBundle ReadFirstPersonBundle(string payloadRoot)
    {
        string path = Path.Combine(payloadRoot, "first-person-bundle.json");
        FirstPersonBundle bundle = new JavaScriptSerializer().Deserialize<FirstPersonBundle>(File.ReadAllText(path));
        if (!FirstPersonTarget(bundle) || bundle.schema != 1 ||
            !PreviewHash(bundle.vanilla_sha256) || !PreviewHash(bundle.host_sha256) ||
            String.IsNullOrWhiteSpace(bundle.patch_revision) ||
            bundle.ncmm_source_commit == null || !Regex.IsMatch(bundle.ncmm_source_commit, "^[0-9a-f]{40}$") ||
            bundle.files == null || bundle.files.Count < 4)
            throw new InvalidOperationException("Incomplete First Person View test package.");
        if (bundle.supported_vanilla_sha256 != null &&
            (bundle.supported_vanilla_sha256.Length < 1 || bundle.supported_vanilla_sha256.Length > 2 ||
             Array.Exists(bundle.supported_vanilla_sha256, value => !PreviewHash(value)) ||
             !FirstPersonVanillaMatches(bundle, bundle.vanilla_sha256)))
            throw new InvalidOperationException("Invalid official executable identities in the First Person View package.");
        string recordedHost;
        if (!bundle.files.TryGetValue("host/cataclysm-tiles.ncmm.exe", out recordedHost) || recordedHost != bundle.host_sha256 ||
            !bundle.files.ContainsKey("cataclysm-tiles.ncmm-bootstrap.exe") ||
            !bundle.files.ContainsKey("code_mods/FirstPersonView/ncmm_mod.dll") ||
            !bundle.files.ContainsKey("code_mods/FirstPersonView/mod.json") ||
            !bundle.files.ContainsKey("code_mods/FirstPersonView/held-assets.manifest.json") ||
            !bundle.files.ContainsKey("code_mods/FirstPersonView/held-assets-licenses.txt"))
            throw new InvalidOperationException("First Person View package inventory is incomplete.");
        return bundle;
    }

    private static string ValidateFirstPersonBundle(string gameRoot, string payloadRoot)
    {
        FirstPersonBundle bundle = ReadFirstPersonBundle(payloadRoot);
        HashSet<string> paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (KeyValuePair<string, string> entry in bundle.files)
        {
            if (String.IsNullOrEmpty(entry.Key) || entry.Key.Contains("..") || entry.Key.Contains("\\") ||
                entry.Key.Contains(":") || entry.Key.StartsWith("/") || !PreviewHash(entry.Value) || !paths.Add(entry.Key))
                throw new InvalidOperationException("Unsafe First Person View package inventory.");
            string file = Path.Combine(payloadRoot, entry.Key.Replace('/', Path.DirectorySeparatorChar));
            NcmmRuntimeIO.GuardPath(file);
            if (!File.Exists(file) || Sha256(file) != entry.Value)
                throw new InvalidOperationException("First Person View package SHA256 mismatch: " + entry.Key);
        }
        foreach (string file in Directory.GetFiles(payloadRoot, "*", SearchOption.AllDirectories))
        {
            string relative = file.Substring(payloadRoot.Length + 1).Replace(Path.DirectorySeparatorChar, '/');
            if (relative != "first-person-bundle.json" && !paths.Contains(relative))
                throw new InvalidOperationException("Unrecorded First Person View package file: " + relative);
        }
        // An existing bootstrap is identified by its receipt, never by its name.
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        string receipt = Path.Combine(gameRoot, "ncmm", "bootstrap.sha256");
        string legacyReceipt = Path.Combine(gameRoot, "cml", "bootstrap.sha256");
        string currentHash = Sha256(exe);
        bool knownBootstrap = currentHash == bundle.files["cataclysm-tiles.ncmm-bootstrap.exe"] ||
            FirstPersonReceiptMatches(receipt, currentHash) || FirstPersonReceiptMatches(legacyReceipt, currentHash);
        string target = knownBootstrap ? vanilla : exe;
        string build = bundle.upstream_tag.Substring("cdda-experimental-".Length);
        if (!File.Exists(target))
            throw new InvalidOperationException("An NCMM/CML bootstrap was recognized, but the original game backup is missing: " +
                target + ". Selected executable was not changed.");
        string targetHash = Sha256(target);
        if (!FirstPersonVanillaMatches(bundle, targetHash))
            throw new InvalidOperationException("First Person View TEST for CDDA " + build +
                " requires an official Windows x64 graphics or graphics-and-sounds executable.\r\n" +
                "Checked: " + target + "\r\nSHA256: " + targetHash +
                "\r\nSelected executable was not changed.");
        string source = ReadSourceCommit(gameRoot);
        if (!NcmmRuntimeIO.SourceMatches(bundle.source_commit, source))
            throw new InvalidOperationException("The executable matches CDDA " + build +
                ", but VERSION.txt identifies a different source: " + source +
                ". Selected executable was not changed.");
        return targetHash;
    }

    private static void InstallFirstPersonHost(string gameRoot, string payloadRoot)
    {
        // Recheck inside the production setup transaction; rollback includes the
        // Host executable, binding, bootstrap, vanilla backup and selected modules.
        string vanillaHash = ValidateFirstPersonBundle(gameRoot, payloadRoot);
        FirstPersonBundle bundle = ReadFirstPersonBundle(payloadRoot);
        // Bind this installation to its own verified official variant. The
        // bootstrap uses this exact hash when launching the offline preview Host.
        bundle.vanilla_sha256 = vanillaHash;
        string host = Path.Combine(gameRoot, "cataclysm-tiles.ncmm.exe");
        string staged = Path.Combine(gameRoot, "ncmm", "host.preview-" + Guid.NewGuid().ToString("N") + ".exe");
        try
        {
            NcmmRuntimeIO.CopyDurable(Path.Combine(payloadRoot, "host", "cataclysm-tiles.ncmm.exe"), staged);
            if (Sha256(staged) != bundle.host_sha256)
                throw new IOException("Staged First Person View Host SHA256 mismatch.");
            NcmmRuntimeIO.PublishFile(staged, host);
        }
        finally { if (File.Exists(staged)) File.Delete(staged); }
        WriteJsonAtomic(Path.Combine(gameRoot, "ncmm", "host.binding.json"), new SetupHostBinding {
            vanilla_sha256 = bundle.vanilla_sha256, host_sha256 = bundle.host_sha256,
            source_commit = bundle.source_commit, upstream_tag = bundle.upstream_tag,
            patch_revision = bundle.patch_revision, ncmm_version = RuntimeVersion,
            loader_api = 1, installed_utc = DateTime.UtcNow.ToString("o")
        });
        WriteJsonAtomic(Path.Combine(gameRoot, "ncmm", "first-person-preview.json"), bundle);
        if (!FirstPersonHostStatus(gameRoot).Ready)
            throw new InvalidOperationException("Experimental Host post-install verification failed.");
        UpdateSetupTransactionPhase(gameRoot, "preview_host_installed");
    }

    private static SetupHostSyncResult FirstPersonHostStatus(string gameRoot)
    {
        string root = NcmmRuntimeIO.Root(gameRoot);
        NcmmRuntimeIO.GuardTree(Path.Combine(root, "ncmm"));
        FirstPersonBundle bundle = new JavaScriptSerializer().Deserialize<FirstPersonBundle>(
            File.ReadAllText(Path.Combine(root, "ncmm", "first-person-preview.json")));
        SetupHostBinding binding = new JavaScriptSerializer().Deserialize<SetupHostBinding>(
            File.ReadAllText(Path.Combine(root, "ncmm", "host.binding.json")));
        bool ready = FirstPersonTarget(bundle) && bundle.schema == 1 && binding != null &&
            PreviewHash(bundle.vanilla_sha256) && FirstPersonVanillaMatches(bundle, bundle.vanilla_sha256) &&
            binding.host_sha256 == bundle.host_sha256 && binding.vanilla_sha256 == bundle.vanilla_sha256 &&
            binding.source_commit == bundle.source_commit && binding.upstream_tag == bundle.upstream_tag &&
            binding.patch_revision == bundle.patch_revision &&
            binding.loader_api == 1 && binding.ncmm_version == RuntimeVersion &&
            NcmmRuntimeIO.SourceMatches(binding.source_commit, ReadSourceCommit(root)) &&
            Sha256(Path.Combine(root, "cataclysm-tiles.ncmm.exe")) == binding.host_sha256 &&
            Sha256(Path.Combine(root, "cataclysm-tiles.vanilla.exe")) == binding.vanilla_sha256;
        return new SetupHostSyncResult {
            Ready = ready, HostSha256 = binding == null ? null : binding.host_sha256,
            PatchRevision = binding == null ? null : binding.patch_revision,
            Message = ready ? "First Person View TEST Host verified locally for CDDA " +
                bundle.upstream_tag.Substring("cdda-experimental-".Length) +
                ". F6 toggles the view; F7/F8 turn the camera." : "Experimental Host identity verification failed."
        };
    }
}
