using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

internal static class InstallationMatrixHarness
{
    private const string AwsId = "advanced_world_settings";
    private const string SurvivorId = "survivor_progression";
    private const string AwsDir = "AdvancedWorldSettings";
    private const string SurvivorDir = "SurvivorProgression";

    private static int passed;
    private static int total;

    private static void AssertTrue(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    private static void AssertEqual(string actual, string expected, string message)
    {
        if (!String.Equals(actual, expected, StringComparison.Ordinal))
            throw new InvalidOperationException(message + " (actual='" + actual + "', expected='" + expected + "')");
    }

    private static void AssertSet(IEnumerable<string> actual, params string[] expected)
    {
        string[] left = actual.OrderBy(x => x, StringComparer.Ordinal).ToArray();
        string[] right = expected.OrderBy(x => x, StringComparer.Ordinal).ToArray();
        if (left.Length != right.Length || !left.SequenceEqual(right))
            throw new InvalidOperationException(
                "Component set mismatch. actual=[" + String.Join(",", left) + "] expected=[" + String.Join(",", right) + "]");
    }

    private static void Run(string name, Action body)
    {
        total++;
        try
        {
            body();
            passed++;
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("  PASS " + name);
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine("  FAIL " + name + " | " + ex.Message);
            Console.ResetColor();
            throw;
        }
        finally
        {
            Console.ResetColor();
        }
    }

    private static string NewGame(string work, string name, string vanillaMarker)
    {
        string root = Path.Combine(work, name);
        Directory.CreateDirectory(root);
        File.WriteAllText(Path.Combine(root, "cataclysm-tiles.exe"), vanillaMarker + Environment.NewLine, Encoding.ASCII);
        File.WriteAllText(Path.Combine(root, "VERSION.txt"),
            "commit sha: e262adb299a7613b4aedc5f12c08fe0413c56a84" + Environment.NewLine,
            Encoding.ASCII);
        return root;
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

    private static string Relative(string root, string path)
    {
        Uri rootUri = new Uri(root.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) +
                              Path.DirectorySeparatorChar);
        Uri pathUri = new Uri(path);
        return Uri.UnescapeDataString(rootUri.MakeRelativeUri(pathUri).ToString()).Replace('/', '\\');
    }

    private static string FingerprintTree(string root)
    {
        List<string> rows = new List<string>();
        if (!Directory.Exists(root)) return "<missing>";
        foreach (string file in Directory.GetFiles(root, "*", SearchOption.AllDirectories))
        {
            string rel = Relative(root, file);
            rows.Add(rel + "|" + Sha256(file));
        }
        rows.Sort(StringComparer.OrdinalIgnoreCase);
        return String.Join("\n", rows.ToArray());
    }

    private static SetupInstalledComponents ReadInstalledState(string root)
    {
        string path = Path.Combine(root, "ncmm", "installed-components.json");
        AssertTrue(File.Exists(path), "installed-components.json missing");
        return new JavaScriptSerializer().Deserialize<SetupInstalledComponents>(File.ReadAllText(path));
    }

    private static string ModuleDirectory(string id)
    {
        if (id == AwsId) return AwsDir;
        if (id == SurvivorId) return SurvivorDir;
        throw new InvalidOperationException("Unknown test module id: " + id);
    }

    private static void AssertInstalled(string root, string payload, string originalVanillaHash,
                                        params string[] expectedModuleIds)
    {
        string installedExe = Path.Combine(root, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(root, "cataclysm-tiles.vanilla.exe");
        AssertTrue(File.Exists(installedExe), "installed bootstrap missing");
        AssertTrue(File.Exists(vanilla), "vanilla backup missing");
        AssertEqual(Sha256(installedExe), Sha256(Path.Combine(payload, "cataclysm-tiles.ncmm-bootstrap.exe")),
                    "bootstrap hash mismatch");
        AssertEqual(Sha256(vanilla), originalVanillaHash, "vanilla backup changed");

        SetupInstalledComponents state = ReadInstalledState(root);
        List<string> ids = state.components.Select(x => x.id).ToList();
        List<string> expectedState = new List<string>();
        expectedState.Add("ncmm_host");
        expectedState.AddRange(expectedModuleIds);
        AssertSet(ids, expectedState.ToArray());

        foreach (string id in new string[] { AwsId, SurvivorId })
        {
            string dirName = ModuleDirectory(id);
            string installedDir = Path.Combine(root, "code_mods", dirName);
            string payloadDir = Path.Combine(payload, "code_mods", dirName);
            bool expected = expectedModuleIds.Contains(id);
            AssertTrue(SetupCore.IsModuleInstalled(root, dirName, id) == expected,
                       "module installed-state mismatch: " + id);
            if (expected)
            {
                AssertEqual(Sha256(Path.Combine(installedDir, "ncmm_mod.dll")),
                            Sha256(Path.Combine(payloadDir, "ncmm_mod.dll")),
                            "module DLL hash mismatch: " + id);
                AssertEqual(Sha256(Path.Combine(installedDir, "mod.json")),
                            Sha256(Path.Combine(payloadDir, "mod.json")),
                            "module manifest hash mismatch: " + id);
            }
        }

        AssertTrue(!File.Exists(Path.Combine(root, ".ncmm-setup.pending.json")),
                   "setup transaction marker leaked after success");
        AssertTrue(Directory.GetDirectories(root, ".ncmm-setup-tx-*").Length == 0,
                   "setup transaction backup leaked after success");
    }

    private static void ExpectInstallFailure(string root, string payload, IEnumerable<string> selected,
                                             string messageFragment)
    {
        bool failed = false;
        try
        {
            SetupCore.Install(root, payload, selected);
        }
        catch (Exception ex)
        {
            failed = true;
            if (!String.IsNullOrEmpty(messageFragment))
                AssertTrue(ex.Message.IndexOf(messageFragment, StringComparison.OrdinalIgnoreCase) >= 0,
                           "unexpected failure: " + ex.Message);
        }
        AssertTrue(failed, "installation unexpectedly succeeded");
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }

    private static int RunAbortChild(string self, string root, string payload, string phase,
                                     params string[] selected)
    {
        ProcessStartInfo psi = new ProcessStartInfo();
        psi.FileName = self;
        StringBuilder args = new StringBuilder();
        args.Append("--child-install ");
        args.Append(Quote(root)).Append(" ");
        args.Append(Quote(payload));
        foreach (string id in selected) args.Append(" ").Append(Quote(id));
        psi.Arguments = args.ToString();
        psi.UseShellExecute = false;
        psi.CreateNoWindow = true;
        psi.EnvironmentVariables["NCMM_SETUP_MATRIX_TEST_MODE"] = "1";
        psi.EnvironmentVariables["NCMM_SETUP_MATRIX_ABORT_PHASE"] = phase;
        using (Process process = Process.Start(psi))
        {
            process.WaitForExit();
            return process.ExitCode;
        }
    }

    private static void AssertRollbackOnInjectedThrow(string work, string payload, string phase)
    {
        string root = NewGame(work, "throw-" + phase, "vanilla-" + phase);
        string originalHash = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
        SetupCore.Install(root, payload, new string[] { SurvivorId });
        AssertInstalled(root, payload, originalHash, SurvivorId);
        string before = FingerprintTree(root);

        Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE", "1");
        Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE", phase);
        try
        {
            ExpectInstallFailure(root, payload, new string[] { AwsId, SurvivorId }, "Injected NCMM setup failure");
        }
        finally
        {
            Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE", null);
            Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE", null);
        }

        AssertEqual(FingerprintTree(root), before, "soft-failure rollback was not byte-identical at " + phase);
    }

    private static void AssertRecoveryAfterHardAbort(string self, string work, string payload, string phase)
    {
        string root = NewGame(work, "abort-" + phase, "vanilla-abort-" + phase);
        string originalHash = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
        SetupCore.Install(root, payload, new string[] { SurvivorId });
        AssertInstalled(root, payload, originalHash, SurvivorId);
        string before = FingerprintTree(root);

        int exitCode = RunAbortChild(self, root, payload, phase, AwsId, SurvivorId);
        AssertTrue(exitCode == 86, "hard-abort child returned " + exitCode + " instead of 86");
        AssertTrue(File.Exists(Path.Combine(root, ".ncmm-setup.pending.json")),
                   "hard abort did not leave recovery marker");

        bool recovered = SetupCore.RecoverPendingSetupTransaction(root);
        AssertTrue(recovered, "pending setup transaction was not recovered");
        AssertEqual(FingerprintTree(root), before, "hard-abort recovery was not byte-identical at " + phase);

        SetupCore.Install(root, payload, new string[] { AwsId, SurvivorId });
        AssertInstalled(root, payload, originalHash, AwsId, SurvivorId);
    }

    private static int ChildInstall(string[] args)
    {
        if (args.Length < 3) return 64;
        string root = args[1];
        string payload = args[2];
        string[] selected = args.Skip(3).ToArray();
        SetupCore.Install(root, payload, selected);
        return 0;
    }

    private static int RealInstallSmoke(string gameRoot, string payload)
    {
        gameRoot = Path.GetFullPath(gameRoot);
        payload = Path.GetFullPath(payload);
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        AssertTrue(File.Exists(exe), "real CDDA smoke target has no cataclysm-tiles.exe");
        string originalVanilla = Sha256(exe);

        SetupCore.Install(gameRoot, payload, new string[] { AwsId, SurvivorId });
        AssertInstalled(gameRoot, payload, originalVanilla, AwsId, SurvivorId);

        SetupCore.Install(gameRoot, payload, new string[] { AwsId });
        AssertInstalled(gameRoot, payload, originalVanilla, AwsId);

        SetupCore.Install(gameRoot, payload, new string[] { AwsId, SurvivorId });
        AssertInstalled(gameRoot, payload, originalVanilla, AwsId, SurvivorId);

        SetupCore.RestoreVanilla(gameRoot);
        AssertEqual(Sha256(exe), originalVanilla, "RestoreVanilla did not restore official executable bytes");

        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine("NCMM Real CDDA Install Smoke: PASS");
        Console.ResetColor();
        return 0;
    }

    private static int Main(string[] args)
    {
        if (args.Length > 0 && String.Equals(args[0], "--child-install", StringComparison.Ordinal))
            return ChildInstall(args);
        if (args.Length == 3 && String.Equals(args[0], "--real-install-smoke", StringComparison.Ordinal))
            return RealInstallSmoke(args[1], args[2]);
        if (args.Length != 1)
        {
            Console.Error.WriteLine("usage: NCMM_InstallationMatrix_Harness.exe <payload-root>");
            Console.Error.WriteLine("   or: NCMM_InstallationMatrix_Harness.exe --real-install-smoke <game-root> <payload-root>");
            return 2;
        }

        string payload = Path.GetFullPath(args[0]);
        string self = Process.GetCurrentProcess().MainModule.FileName;
        string work = Path.Combine(Path.GetTempPath(), "ncmm-install-matrix-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(work);

        try
        {
            Run("clean CDDA -> NCMM without modules", delegate {
                string root = NewGame(work, "clean-none", "vanilla-none");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                SetupCore.Install(root, payload, new string[0]);
                AssertInstalled(root, payload, original);
            });

            Run("clean CDDA -> Survivor only", delegate {
                string root = NewGame(work, "clean-survivor", "vanilla-survivor");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                SetupCore.Install(root, payload, new string[] { SurvivorId });
                AssertInstalled(root, payload, original, SurvivorId);
            });

            Run("clean CDDA -> AWS only", delegate {
                string root = NewGame(work, "clean-aws", "vanilla-aws");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                SetupCore.Install(root, payload, new string[] { AwsId });
                AssertInstalled(root, payload, original, AwsId);
            });

            Run("clean CDDA -> Survivor + AWS", delegate {
                string root = NewGame(work, "clean-full", "vanilla-full");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                SetupCore.Install(root, payload, new string[] { AwsId, SurvivorId });
                AssertInstalled(root, payload, original, AwsId, SurvivorId);
            });

            Run("disable Survivor -> AWS remains", delegate {
                string root = NewGame(work, "disable-survivor", "vanilla-disable");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                SetupCore.Install(root, payload, new string[] { AwsId, SurvivorId });
                string survivorDir = Path.Combine(root, "code_mods", SurvivorDir);
                File.WriteAllText(Path.Combine(survivorDir, "user-note.txt"), "preserve me\n", Encoding.UTF8);
                SetupCore.Install(root, payload, new string[] { AwsId });
                AssertInstalled(root, payload, original, AwsId);
                AssertTrue(File.Exists(Path.Combine(survivorDir, "user-note.txt")),
                           "deselecting Survivor removed user-owned module files");
            });

            Run("re-enable Survivor after removal", delegate {
                string root = NewGame(work, "reenable-survivor", "vanilla-reenable");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                SetupCore.Install(root, payload, new string[] { AwsId, SurvivorId });
                string survivorDir = Path.Combine(root, "code_mods", SurvivorDir);
                File.WriteAllText(Path.Combine(survivorDir, "disabled.note"), "persist\n", Encoding.UTF8);
                SetupCore.Install(root, payload, new string[] { AwsId });
                SetupCore.Install(root, payload, new string[] { AwsId, SurvivorId });
                AssertInstalled(root, payload, original, AwsId, SurvivorId);
                AssertTrue(File.Exists(Path.Combine(survivorDir, "disabled.note")),
                           "re-enable lost preserved user file");
            });

            Run("same-version reinstall is safe and idempotent", delegate {
                string root = NewGame(work, "reinstall", "vanilla-reinstall");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                SetupCore.Install(root, payload, new string[] { AwsId, SurvivorId });
                int backupsBefore = Directory.GetFiles(Path.Combine(root, "ncmm"),
                    "cataclysm-tiles.vanilla.backup-*.exe").Length;
                SetupCore.Install(root, payload, new string[] { AwsId, SurvivorId });
                int backupsAfter = Directory.GetFiles(Path.Combine(root, "ncmm"),
                    "cataclysm-tiles.vanilla.backup-*.exe").Length;
                AssertInstalled(root, payload, original, AwsId, SurvivorId);
                AssertTrue(backupsAfter == backupsBefore, "idempotent reinstall created a spurious vanilla archive");
            });

            Run("previous NCMM bootstrap -> current update preserves vanilla", delegate {
                string root = NewGame(work, "previous-runtime", "old-bootstrap");
                string vanilla = Path.Combine(root, "cataclysm-tiles.vanilla.exe");
                File.WriteAllText(vanilla, "true-original-vanilla\n", Encoding.ASCII);
                string original = Sha256(vanilla);
                Directory.CreateDirectory(Path.Combine(root, "ncmm"));
                string oldBootstrapHash = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                File.WriteAllText(Path.Combine(root, "ncmm", "bootstrap.sha256"),
                    oldBootstrapHash + Environment.NewLine, Encoding.ASCII);
                SetupCore.Install(root, payload, new string[] { SurvivorId });
                AssertInstalled(root, payload, original, SurvivorId);
            });

            Run("corrupted Survivor DLL is repaired by reinstall", delegate {
                string root = NewGame(work, "repair-dll", "vanilla-repair-dll");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                SetupCore.Install(root, payload, new string[] { SurvivorId });
                string dll = Path.Combine(root, "code_mods", SurvivorDir, "ncmm_mod.dll");
                File.AppendAllText(dll, "corruption", Encoding.ASCII);
                AssertTrue(Sha256(dll) != Sha256(Path.Combine(payload, "code_mods", SurvivorDir, "ncmm_mod.dll")),
                           "DLL corruption fixture failed");
                SetupCore.Install(root, payload, new string[] { SurvivorId });
                AssertInstalled(root, payload, original, SurvivorId);
            });

            Run("corrupted managed manifest fails closed and rolls back", delegate {
                string root = NewGame(work, "corrupt-manifest", "vanilla-corrupt-manifest");
                SetupCore.Install(root, payload, new string[] { AwsId, SurvivorId });
                string manifest = Path.Combine(root, "code_mods", SurvivorDir, "mod.json");
                File.WriteAllText(manifest, "{broken-json", Encoding.ASCII);
                string before = FingerprintTree(root);
                ExpectInstallFailure(root, payload, new string[] { AwsId, SurvivorId }, "Invalid NCMM module manifest");
                AssertEqual(FingerprintTree(root), before, "corrupt-manifest failure mutated installation");
            });

            Run("unknown component selection fails closed", delegate {
                string root = NewGame(work, "unknown-component", "vanilla-unknown");
                string before = FingerprintTree(root);
                ExpectInstallFailure(root, payload, new string[] { "__unknown_component__" }, "Unknown bundled NCMM component");
                AssertEqual(FingerprintTree(root), before, "unknown component failure mutated clean game");
            });

            Run("incomplete installer payload fails before mutation", delegate {
                string root = NewGame(work, "bad-payload-game", "vanilla-bad-payload");
                string badPayload = Path.Combine(work, "bad-payload");
                CopyDirectory(payload, badPayload);
                File.Delete(Path.Combine(badPayload, "code_mods", AwsDir, "ncmm_mod.dll"));
                string before = FingerprintTree(root);
                ExpectInstallFailure(root, badPayload, new string[] { AwsId }, "incomplete module");
                AssertEqual(FingerprintTree(root), before, "bad payload mutated game before preflight failure");
            });

            Run("rollback after injected bootstrap-stage failure", delegate {
                AssertRollbackOnInjectedThrow(work, payload, "bootstrap_installed");
            });

            Run("rollback after injected module-stage failure", delegate {
                AssertRollbackOnInjectedThrow(work, payload, "modules_installed");
            });

            Run("rollback immediately before commit", delegate {
                AssertRollbackOnInjectedThrow(work, payload, "ready_to_commit");
            });

            Run("recover hard interruption after module install", delegate {
                AssertRecoveryAfterHardAbort(self, work, payload, "modules_installed");
            });

            Run("recover hard interruption before commit", delegate {
                AssertRecoveryAfterHardAbort(self, work, payload, "ready_to_commit");
            });

            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("NCMM Installation Matrix: PASS (" + passed + "/" + total + " scenarios).");
            Console.ResetColor();
            return 0;
        }
        finally
        {
            try { Directory.Delete(work, true); } catch { }
        }
    }

    private static void CopyDirectory(string source, string destination)
    {
        Directory.CreateDirectory(destination);
        foreach (string file in Directory.GetFiles(source))
            File.Copy(file, Path.Combine(destination, Path.GetFileName(file)), true);
        foreach (string directory in Directory.GetDirectories(source))
            CopyDirectory(directory, Path.Combine(destination, Path.GetFileName(directory)));
    }
}
