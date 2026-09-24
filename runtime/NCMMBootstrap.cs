using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
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

internal static class NCMMBootstrap
{
    private const int LoaderApi = 1;
    private const string DefaultFeedUrl = "https://raw.githubusercontent.com/Neversalimus/Cataclysm/master/ncmm-platform/feed/index.json";

    private static string Root;
    private static string NcmmDir;
    private static string LogPath;
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

    private static bool TryFetchCertifiedHost(string vanillaSha, string sourceCommit, bool localWasValid)
    {
        string temp = Path.Combine(NcmmDir, "host.download.tmp");
        try
        {
            using (TimeoutWebClient wc = new TimeoutWebClient())
            {
                wc.Headers[HttpRequestHeader.UserAgent] = "NCMM/0.3";
                string feedText = wc.DownloadString(FeedUrl());
                FeedIndex feed = Json.Deserialize<FeedIndex>(feedText);
                if (feed == null || feed.schema != 1 || feed.loader_api != LoaderApi || feed.hosts == null)
                {
                    Log("Host feed rejected: unsupported schema/API.");
                    return false;
                }

                FeedHostEntry entry;
                if (!feed.hosts.TryGetValue(vanillaSha.ToLowerInvariant(), out entry) || entry == null)
                {
                    Log("No certified host in feed for vanilla SHA " + vanillaSha + ".");
                    return false;
                }
                if (entry.loader_api != LoaderApi)
                {
                    Log("Feed host uses unsupported loader API.");
                    return false;
                }
                if (!String.IsNullOrEmpty(sourceCommit) &&
                    !String.Equals(entry.source_commit, sourceCommit, StringComparison.OrdinalIgnoreCase))
                {
                    Log("Feed source commit mismatch; refusing host.");
                    return false;
                }
                if (String.IsNullOrEmpty(entry.host_url) ||
                    !entry.host_url.StartsWith("https://", StringComparison.OrdinalIgnoreCase) ||
                    String.IsNullOrEmpty(entry.host_sha256))
                {
                    Log("Feed host entry is incomplete.");
                    return false;
                }

                HostBinding current = ReadBinding();
                if (localWasValid && current != null &&
                    String.Equals(current.host_sha256, entry.host_sha256, StringComparison.OrdinalIgnoreCase))
                {
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

                if (File.Exists(host)) File.Delete(host);
                File.Move(staged, host);
                if (!String.Equals(Sha256(host), entry.host_sha256, StringComparison.OrdinalIgnoreCase))
                {
                    Log("Host post-install SHA256 mismatch; deleting host.");
                    try { File.Delete(host); } catch { }
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
                if (File.Exists(bindingPath)) File.Delete(bindingPath);
                File.Move(bindingTmp, bindingPath);

                Log((localWasValid ? "Certified host updated for " : "Certified host downloaded for ") +
                    (entry.upstream_tag ?? entry.source_commit) + ".");
                return true;
            }
        }
        catch (WebException ex)
        {
            Log("Host feed unavailable; " +
                (localWasValid ? "keeping current certified host: " : "vanilla fallback remains active: ") + ex.Message);
            return false;
        }
        catch (Exception ex)
        {
            Log("Host auto-sync failed; " +
                (localWasValid ? "keeping current certified host: " : "vanilla fallback remains active: ") + ex.Message);
            return false;
        }
        finally
        {
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

        List<string> forwarded = new List<string>();
        bool forceVanilla = false;
        bool reset = false;
        bool offline = false;
        bool refresh = false;
        foreach (string arg in args)
        {
            if (String.Equals(arg, "--ncmm-vanilla", StringComparison.OrdinalIgnoreCase)) forceVanilla = true;
            else if (String.Equals(arg, "--ncmm-reset", StringComparison.OrdinalIgnoreCase)) reset = true;
            else if (String.Equals(arg, "--ncmm-offline", StringComparison.OrdinalIgnoreCase)) offline = true;
            else if (String.Equals(arg, "--ncmm-refresh", StringComparison.OrdinalIgnoreCase)) refresh = true;
            else forwarded.Add(arg);
        }

        if (reset)
        {
            try { if (File.Exists(autoDisabled)) File.Delete(autoDisabled); } catch { }
            try { if (File.Exists(pending)) File.Delete(pending); } catch { }
            Log("NCMM crash-loop state reset by command line.");
        }

        if (!File.Exists(vanilla))
        {
            Log("FATAL: cataclysm-tiles.vanilla.exe is missing.");
            return 112;
        }

        if (File.Exists(pending))
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
        string vanillaSha = null;
        string sourceCommit = ReadSourceCommit();

        if (useHost)
        {
            try
            {
                vanillaSha = Sha256(vanilla).ToLowerInvariant();
                bool localValid = HasValidLocalHost(vanillaSha, sourceCommit);
                if (!offline && ShouldCheckFeed(localValid, refresh))
                {
                    TryFetchCertifiedHost(vanillaSha, sourceCommit, localValid);
                }
                if (!HasValidLocalHost(vanillaSha, sourceCommit))
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
                File.WriteAllText(pending,
                    "NCMM host launch pending. Host must delete this file after successful module initialization.\r\n");
            }
            catch (Exception ex)
            {
                Log("Could not create boot.pending; refusing NCMM host: " + ex.Message);
                useHost = false;
            }
        }

        try
        {
            return Launch(useHost ? host : vanilla, forwarded);
        }
        catch (Exception ex)
        {
            Log("Launch failed: " + ex.ToString());
            if (useHost) return 113;
            return 114;
        }
    }
}
