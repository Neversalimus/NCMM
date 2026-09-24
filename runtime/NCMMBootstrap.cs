using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Cache;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

internal sealed class TimeoutWebClient : WebClient
{
    public int TimeoutMs = 3500;
    protected override WebRequest GetWebRequest(Uri address)
    {
        WebRequest request = base.GetWebRequest(address);
        request.Timeout = TimeoutMs;
        request.CachePolicy = new RequestCachePolicy(RequestCacheLevel.NoCacheNoStore);
        return request;
    }
}

internal sealed class FeedHostEntry
{
    public string source_commit { get; set; }
    public string upstream_tag { get; set; }
    public string host_url { get; set; }
    public string host_sha256 { get; set; }
    public string patch_revision { get; set; }
    public string ncmm_version { get; set; }
    public int loader_api { get; set; }
}

internal sealed class FeedIndex
{
    public int schema { get; set; }
    public int loader_api { get; set; }
    public Dictionary<string, FeedHostEntry> hosts { get; set; }
}

internal sealed class HostBinding
{
    public string vanilla_sha256 { get; set; }
    public string host_sha256 { get; set; }
    public string source_commit { get; set; }
    public string upstream_tag { get; set; }
    public string installed_utc { get; set; }
}

internal sealed class RuntimeState
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

internal static class NCMMBootstrap
{
    private const int LoaderApi = 1;
    private const string RuntimeVersion = "0.6.1";
    private const string DefaultFeedUrl = "https://raw.githubusercontent.com/Neversalimus/Cataclysm/master/ncmm-platform/feed/index.json";

    private static string Root;
    private static string NcmmDir;
    private static string LogPath;
    private static string FeedStatus = "not_checked";
    private static readonly RuntimeState State = new RuntimeState();
    private static readonly JavaScriptSerializer Json = new JavaScriptSerializer();

    private static void Log(string message)
    {
        try
        {
            Directory.CreateDirectory(NcmmDir);
            File.AppendAllText(LogPath,
                DateTime.UtcNow.ToString("o") + " " + message + Environment.NewLine,
                Encoding.UTF8);
        }
        catch { }
    }

    private static string Sha256(string path)
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

    private static string TrySha256(string path)
    {
        try
        {
            return File.Exists(path) ? Sha256(path).ToLowerInvariant() : null;
        }
        catch
        {
            return null;
        }
    }

    private static void PublishFileAtomic(string staged, string destination)
    {
        if (!File.Exists(staged))
            throw new FileNotFoundException("Staged file is missing.", staged);

        if (File.Exists(destination))
        {
            // Same-volume File.Replace is atomic on the supported Windows runtime.
            // If replacement fails, the existing destination remains untouched.
            File.Replace(staged, destination, null);
        }
        else
        {
            File.Move(staged, destination);
        }
    }

    private static void RefreshStateFiles()
    {
        State.runtime_version = RuntimeVersion;
        State.loader_api = LoaderApi;
        State.updated_utc = DateTime.UtcNow.ToString("o");
        State.feed_status = FeedStatus;
        State.manual_disabled = File.Exists(Path.Combine(NcmmDir, "ncmm.disabled"));
        State.auto_disabled = File.Exists(Path.Combine(NcmmDir, "ncmm.auto_disabled"));
        State.boot_pending = File.Exists(Path.Combine(NcmmDir, "boot.pending"));

        HostBinding binding = ReadBinding();
        State.binding_host_sha256 = binding == null ? null : binding.host_sha256;
    }

    private static void WriteRuntimeState()
    {
        try
        {
            RefreshStateFiles();
            string path = Path.Combine(NcmmDir, "runtime.state.json");
            string temp = path + ".tmp";
            File.WriteAllText(temp, Json.Serialize(State), Encoding.UTF8);
            PublishFileAtomic(temp, path);
        }
        catch (Exception ex)
        {
            Log("Could not write runtime.state.json: " + ex.Message);
        }
    }

    private static string ReadSourceCommit()
    {
        try
        {
            string version = Path.Combine(Root, "VERSION.txt");
            if (!File.Exists(version)) return null;
            foreach (string line in File.ReadAllLines(version))
            {
                string prefix = "commit sha:";
                if (line.TrimStart().StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                {
                    string value = line.Substring(line.IndexOf(':') + 1).Trim();
                    if (value.Length >= 7) return value.ToLowerInvariant();
                }
            }
        }
        catch (Exception ex)
        {
            Log("Could not read VERSION.txt commit: " + ex.Message);
        }
        return null;
    }

    private static string FeedUrl()
    {
        string overridePath = Path.Combine(NcmmDir, "feed.url");
        try
        {
            if (File.Exists(overridePath))
            {
                string value = File.ReadAllText(overridePath).Trim();
                if (value.StartsWith("https://", StringComparison.OrdinalIgnoreCase)) return value;
                Log("Ignoring non-HTTPS feed.url override.");
            }
        }
        catch { }
        return DefaultFeedUrl;
    }

    private static string FeedUrlForRequest(bool forceRefresh)
    {
        string url = FeedUrl();
        if (!forceRefresh ||
            !url.StartsWith("https://raw.githubusercontent.com/", StringComparison.OrdinalIgnoreCase))
            return url;

        string separator = url.IndexOf('?') >= 0 ? "&" : "?";
        string fresh = url + separator + "ncmm_refresh=" + DateTime.UtcNow.Ticks.ToString();
        Log("Forced feed refresh requested; bypassing raw.githubusercontent.com cache.");
        return fresh;
    }

    private static HostBinding ReadBinding()
    {
        try
        {
            string path = Path.Combine(NcmmDir, "host.binding.json");
            if (!File.Exists(path)) return null;
            return Json.Deserialize<HostBinding>(File.ReadAllText(path));
        }
        catch (Exception ex)
        {
            Log("Host binding parse failed: " + ex.Message);
            return null;
        }
    }

    private static bool HasValidLocalHost(string vanillaSha, string sourceCommit)
    {
        string host = Path.Combine(Root, "cataclysm-tiles.ncmm.exe");
        if (!File.Exists(host)) return false;
        HostBinding binding = ReadBinding();
        if (binding == null) return false;
        if (!String.Equals(binding.vanilla_sha256, vanillaSha, StringComparison.OrdinalIgnoreCase)) return false;
        if (!String.IsNullOrEmpty(sourceCommit) &&
            !String.Equals(binding.source_commit, sourceCommit, StringComparison.OrdinalIgnoreCase)) return false;
        if (String.IsNullOrEmpty(binding.host_sha256)) return false;
        try
        {
            string actual = Sha256(host);
            if (!String.Equals(actual, binding.host_sha256, StringComparison.OrdinalIgnoreCase))
            {
                Log("Installed host binary hash mismatch; host rejected.");
                return false;
            }
            return true;
        }
        catch (Exception ex)
        {
            Log("Installed host verification failed: " + ex.Message);
            return false;
        }
    }

    private static bool ShouldCheckFeed(bool localValid, bool forceRefresh)
    {
        if (forceRefresh || !localValid) return true;
        string stamp = Path.Combine(NcmmDir, "last-feed-check.txt");
        try
        {
            if (!File.Exists(stamp)) return true;
            DateTime last;
            if (!DateTime.TryParse(File.ReadAllText(stamp).Trim(), null,
                System.Globalization.DateTimeStyles.RoundtripKind, out last)) return true;
            return DateTime.UtcNow - last.ToUniversalTime() >= TimeSpan.FromHours(1);
        }
        catch { return true; }
    }

    private static void StampFeedCheck()
    {
        try
        {
            File.WriteAllText(Path.Combine(NcmmDir, "last-feed-check.txt"),
                DateTime.UtcNow.ToString("o") + Environment.NewLine, Encoding.ASCII);
        }
        catch { }
    }

    private static bool TryFetchCertifiedHost(string vanillaSha, string sourceCommit, bool localWasValid, bool forceRefresh)
    {
        string temp = Path.Combine(NcmmDir, "host.download.tmp");
        FeedStatus = "checking";
        State.feed_status = FeedStatus;
        WriteRuntimeState();
        try
        {
            using (TimeoutWebClient wc = new TimeoutWebClient())
            {
                wc.Headers[HttpRequestHeader.UserAgent] = "NCMM/0.6.1";
                string feedText = wc.DownloadString(FeedUrlForRequest(forceRefresh));
                FeedIndex feed = Json.Deserialize<FeedIndex>(feedText);
                if (feed == null || feed.schema != 1 || feed.loader_api != LoaderApi || feed.hosts == null)
                {
                    FeedStatus = "rejected_schema_or_api";
                    Log("Host feed rejected: unsupported schema/API.");
                    return false;
                }

                FeedHostEntry entry;
                if (!feed.hosts.TryGetValue(vanillaSha.ToLowerInvariant(), out entry) || entry == null)
                {
                    FeedStatus = "no_certified_host";
                    Log("No certified host in feed for vanilla SHA " + vanillaSha + ".");
                    return false;
                }
                if (entry.loader_api != LoaderApi)
                {
                    FeedStatus = "rejected_loader_api";
                    Log("Feed host uses unsupported loader API.");
                    return false;
                }
                if (!String.IsNullOrEmpty(sourceCommit) &&
                    !String.Equals(entry.source_commit, sourceCommit, StringComparison.OrdinalIgnoreCase))
                {
                    FeedStatus = "rejected_source_commit";
                    Log("Feed source commit mismatch; refusing host.");
                    return false;
                }
                if (String.IsNullOrEmpty(entry.host_url) ||
                    !entry.host_url.StartsWith("https://", StringComparison.OrdinalIgnoreCase) ||
                    String.IsNullOrEmpty(entry.host_sha256))
                {
                    FeedStatus = "rejected_incomplete_entry";
                    Log("Feed host entry is incomplete.");
                    return false;
                }

                HostBinding current = ReadBinding();
                if (localWasValid && current != null &&
                    String.Equals(current.host_sha256, entry.host_sha256, StringComparison.OrdinalIgnoreCase))
                {
                    FeedStatus = "current";
                    return true;
                }

                try { if (File.Exists(temp)) File.Delete(temp); } catch { }
                wc.DownloadFile(entry.host_url, temp);
                string downloadedSha = Sha256(temp);
                if (!String.Equals(downloadedSha, entry.host_sha256, StringComparison.OrdinalIgnoreCase))
                {
                    Log("Downloaded host SHA256 mismatch; rejected.");
                    return false;
                }

                string host = Path.Combine(Root, "cataclysm-tiles.ncmm.exe");
                string staged = Path.Combine(NcmmDir, "host.staged.exe");
                try { if (File.Exists(staged)) File.Delete(staged); } catch { }
                File.Copy(temp, staged, true);
                if (!String.Equals(Sha256(staged), entry.host_sha256, StringComparison.OrdinalIgnoreCase))
                {
                    Log("Staged host SHA256 mismatch; rejected.");
                    return false;
                }

                PublishFileAtomic(staged, host);
                if (!String.Equals(Sha256(host), entry.host_sha256, StringComparison.OrdinalIgnoreCase))
                {
                    FeedStatus = "host_postinstall_hash_mismatch";
                    Log("Host post-install SHA256 mismatch; host rejected.");
                    return false;
                }

                HostBinding binding = new HostBinding();
                binding.vanilla_sha256 = vanillaSha.ToLowerInvariant();
                binding.host_sha256 = entry.host_sha256.ToLowerInvariant();
                binding.source_commit = (entry.source_commit ?? "").ToLowerInvariant();
                binding.upstream_tag = entry.upstream_tag ?? "";
                binding.installed_utc = DateTime.UtcNow.ToString("o");
                string bindingPath = Path.Combine(NcmmDir, "host.binding.json");
                string bindingTmp = bindingPath + ".tmp";
                File.WriteAllText(bindingTmp, Json.Serialize(binding), Encoding.UTF8);
                PublishFileAtomic(bindingTmp, bindingPath);

                FeedStatus = localWasValid ? "updated" : "downloaded";
                Log((localWasValid ? "Certified host updated for " : "Certified host downloaded for ") +
                    (entry.upstream_tag ?? entry.source_commit) + ".");
                return true;
            }
        }
        catch (WebException ex)
        {
            FeedStatus = "unavailable";
            Log("Host feed unavailable; " +
                (localWasValid ? "keeping current certified host: " : "vanilla fallback remains active: ") + ex.Message);
            return false;
        }
        catch (Exception ex)
        {
            FeedStatus = "failed";
            Log("Host auto-sync failed; " +
                (localWasValid ? "keeping current certified host: " : "vanilla fallback remains active: ") + ex.Message);
            return false;
        }
        finally
        {
            State.feed_status = FeedStatus;
            WriteRuntimeState();
            StampFeedCheck();
            try { if (File.Exists(temp)) File.Delete(temp); } catch { }
        }
    }

    private static string QuoteArg(string arg)
    {
        if (arg.Length > 0 && arg.IndexOfAny(new char[] { ' ', '\t', '\n', '\v', '"' }) < 0)
            return arg;

        StringBuilder sb = new StringBuilder();
        sb.Append('"');
        int slashes = 0;
        foreach (char c in arg)
        {
            if (c == '\\') slashes++;
            else if (c == '"')
            {
                sb.Append('\\', slashes * 2 + 1);
                sb.Append('"');
                slashes = 0;
            }
            else
            {
                sb.Append('\\', slashes);
                slashes = 0;
                sb.Append(c);
            }
        }
        sb.Append('\\', slashes * 2);
        sb.Append('"');
        return sb.ToString();
    }

    private static int Launch(string exe, List<string> args)
    {
        ProcessStartInfo psi = new ProcessStartInfo();
        psi.FileName = exe;
        psi.WorkingDirectory = Root;
        psi.UseShellExecute = false;
        StringBuilder arguments = new StringBuilder();
        for (int i = 0; i < args.Count; i++)
        {
            if (i != 0) arguments.Append(' ');
            arguments.Append(QuoteArg(args[i]));
        }
        psi.Arguments = arguments.ToString();

        Log("Launching: " + Path.GetFileName(exe));
        using (Process child = Process.Start(psi))
        {
            child.WaitForExit();
            Log("Child exit code: " + child.ExitCode.ToString());
            State.last_exit_code = child.ExitCode;
            State.reason = child.ExitCode == 0 ? "child_exit_0" : "child_exit_nonzero";
            WriteRuntimeState();
            return child.ExitCode;
        }
    }

    [STAThread]
    private static int Main(string[] args)
    {
        string self = Process.GetCurrentProcess().MainModule.FileName;
        Root = Path.GetDirectoryName(self);
        NcmmDir = Path.Combine(Root, "ncmm");
        LogPath = Path.Combine(NcmmDir, "bootstrap.log");
        Directory.CreateDirectory(NcmmDir);
        Log("NCMM bootstrap " + RuntimeVersion + " starting.");

        try
        {
            ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072;
            Log("TLS 1.2 enabled for NCMM network requests.");
        }
        catch (Exception ex)
        {
            Log("Could not enable TLS 1.2: " + ex.Message);
        }

        string vanilla = Path.Combine(Root, "cataclysm-tiles.vanilla.exe");
        string host = Path.Combine(Root, "cataclysm-tiles.ncmm.exe");
        string disabled = Path.Combine(NcmmDir, "ncmm.disabled");
        string autoDisabled = Path.Combine(NcmmDir, "ncmm.auto_disabled");
        string pending = Path.Combine(NcmmDir, "boot.pending");
        string ready = Path.Combine(NcmmDir, "boot.ready");

        List<string> forwarded = new List<string>();
        bool forceVanilla = false;
        bool reset = false;
        bool offline = false;
        bool refresh = false;
        bool diagnosticsOnly = false;
        foreach (string arg in args)
        {
            if (String.Equals(arg, "--ncmm-vanilla", StringComparison.OrdinalIgnoreCase)) forceVanilla = true;
            else if (String.Equals(arg, "--ncmm-reset", StringComparison.OrdinalIgnoreCase)) reset = true;
            else if (String.Equals(arg, "--ncmm-offline", StringComparison.OrdinalIgnoreCase)) offline = true;
            else if (String.Equals(arg, "--ncmm-refresh", StringComparison.OrdinalIgnoreCase)) refresh = true;
            else if (String.Equals(arg, "--ncmm-diagnose", StringComparison.OrdinalIgnoreCase)) diagnosticsOnly = true;
            else forwarded.Add(arg);
        }

        State.schema = 1;
        State.runtime_version = RuntimeVersion;
        State.loader_api = LoaderApi;
        State.source_commit = ReadSourceCommit();
        State.vanilla_sha256 = TrySha256(vanilla);
        State.host_sha256 = TrySha256(host);
        State.host_status = "not_checked";
        State.feed_status = FeedStatus;
        State.selected_mode = "UNDECIDED";
        State.reason = "startup";
        State.offline = offline;
        State.refresh_requested = refresh;
        State.diagnostics_only = diagnosticsOnly;
        WriteRuntimeState();

        if (reset && !diagnosticsOnly)
        {
            try { if (File.Exists(autoDisabled)) File.Delete(autoDisabled); } catch { }
            try { if (File.Exists(pending)) File.Delete(pending); } catch { }
            Log("NCMM crash-loop state reset by command line.");
        }

        if (!File.Exists(vanilla))
        {
            State.selected_mode = "ERROR";
            State.reason = "vanilla_missing";
            State.host_status = "not_checked";
            WriteRuntimeState();
            Log("FATAL: cataclysm-tiles.vanilla.exe is missing.");
            return 112;
        }

        if (!diagnosticsOnly && File.Exists(pending))
        {
            try
            {
                File.Delete(pending);
                File.WriteAllText(autoDisabled,
                    "Previous NCMM host did not reach ready state. Remove this file or launch with --ncmm-reset after repair.\r\n");
            }
            catch { }
            Log("Previous NCMM boot did not reach ready state; NCMM auto-disabled.");
        }

        bool useHost = !forceVanilla && !File.Exists(disabled) && !File.Exists(autoDisabled);
        string vanillaSha = State.vanilla_sha256;
        string sourceCommit = State.source_commit;

        if (diagnosticsOnly)
        {
            bool localValid = false;
            try
            {
                localValid = !String.IsNullOrEmpty(vanillaSha) && HasValidLocalHost(vanillaSha, sourceCommit);
            }
            catch { localValid = false; }

            State.host_valid = localValid;
            State.host_status = localValid ? "valid" : "invalid_or_missing";
            State.host_sha256 = TrySha256(host);
            State.selected_mode = useHost && localValid ? "NCMM_HOST" : "VANILLA";
            State.reason = "diagnostics_only";
            WriteRuntimeState();
            Log("NCMM diagnostics-only state written to ncmm/runtime.state.json.");
            return 0;
        }

        if (useHost)
        {
            try
            {
                vanillaSha = Sha256(vanilla).ToLowerInvariant();
                State.vanilla_sha256 = vanillaSha;
                bool localValid = HasValidLocalHost(vanillaSha, sourceCommit);
                State.host_valid = localValid;
                State.host_status = localValid ? "valid" : "invalid_or_missing";
                if (!offline && ShouldCheckFeed(localValid, refresh))
                {
                    TryFetchCertifiedHost(vanillaSha, sourceCommit, localValid, refresh);
                    localValid = HasValidLocalHost(vanillaSha, sourceCommit);
                    State.host_valid = localValid;
                    State.host_status = localValid ? "valid" : "invalid_or_missing";
                    State.host_sha256 = TrySha256(host);
                }
                if (!localValid)
                {
                    useHost = false;
                    Log("No valid certified NCMM host installed; falling back to vanilla.");
                }
            }
            catch (Exception ex)
            {
                useHost = false;
                Log("Host validation failed: " + ex.Message);
            }
        }

        if (useHost)
        {
            try
            {
                // boot.ready belongs to the current host launch, never to an earlier successful session.
                try { if (File.Exists(ready)) File.Delete(ready); } catch { }
                File.WriteAllText(pending,
                    "NCMM host launch pending. Host must delete this file after successful module initialization.\r\n");
            }
            catch (Exception ex)
            {
                Log("Could not create boot.pending; refusing NCMM host: " + ex.Message);
                useHost = false;
            }
        }

        State.host_sha256 = TrySha256(host);
        State.selected_mode = useHost ? "NCMM_HOST" : "VANILLA";
        if (useHost) State.reason = "certified_host";
        else if (forceVanilla) State.reason = "forced_vanilla";
        else if (File.Exists(disabled)) State.reason = "manual_disabled";
        else if (File.Exists(autoDisabled)) State.reason = "auto_disabled";
        else State.reason = "host_invalid_or_unavailable";
        WriteRuntimeState();

        try
        {
            return Launch(useHost ? host : vanilla, forwarded);
        }
        catch (Exception ex)
        {
            if (useHost)
            {
                // Process.Start failed before the host could run. This is not a host crash-loop signal.
                try { if (File.Exists(pending)) File.Delete(pending); } catch { }
            }
            State.reason = "launch_failed";
            State.last_exit_code = useHost ? 113 : 114;
            WriteRuntimeState();
            Log("Launch failed: " + ex.ToString());
            if (useHost) return 113;
            return 114;
        }
    }
}
