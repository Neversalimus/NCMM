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
    private const string BallisticId = "ballistic_hit_chance";
    private const string EquipmentBodyMapId = "equipment_body_map";
    private const string AwsDir = "AdvancedWorldSettings";
    private const string SurvivorDir = "SurvivorProgression";
    private const string BallisticDir = "BallisticHitChance";
    private const string EquipmentBodyMapDir = "EquipmentBodyMap";

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
        if (id == BallisticId) return BallisticDir;
        if (id == EquipmentBodyMapId) return EquipmentBodyMapDir;
        throw new InvalidOperationException("Unknown test module id: " + id);
    }

    private static void AssertPayloadModuleTreeInstalled(string payloadDir, string installedDir, string moduleId)
    {
        foreach (string payloadFile in Directory.GetFiles(payloadDir, "*", SearchOption.AllDirectories))
        {
            string relative = Relative(payloadDir, payloadFile);
            string installedFile = Path.Combine(installedDir, relative);
            AssertTrue(File.Exists(installedFile),
                       "packaged module file missing after install: " + moduleId + " | " + relative);
            AssertEqual(Sha256(installedFile), Sha256(payloadFile),
                        "packaged module file hash mismatch after install: " + moduleId + " | " + relative);
        }
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

        foreach (string id in new string[] { AwsId, SurvivorId, BallisticId, EquipmentBodyMapId })
        {
            string dirName = ModuleDirectory(id);
            string installedDir = Path.Combine(root, "code_mods", dirName);
            string payloadDir = Path.Combine(payload, "code_mods", dirName);
            bool expected = expectedModuleIds.Contains(id);
            AssertTrue(SetupCore.IsModuleInstalled(root, dirName, id) == expected,
                       "module installed-state mismatch: " + id);
            if (expected)
            {
                AssertPayloadModuleTreeInstalled(payloadDir, installedDir, id);
            }
        }

        AssertTrue(!File.Exists(Path.Combine(root, ".ncmm-setup.pending.json")),
                   "setup transaction marker leaked after success");
        AssertTrue(Directory.GetDirectories(root, ".ncmm-setup-tx-*").Length == 0,
                   "setup transaction backup leaked after success");
    }

    private static InstallResult InstallVerified(string root, string payload, IEnumerable<string> selected)
    {
        InstallResult result = SetupCore.Install(root, payload, selected);
        AssertTrue(result != null && result.CompletionVerified,
                   "installer returned before completion verification");
        return result;
    }

    private static void ExpectInstallFailure(string root, string payload, IEnumerable<string> selected,
                                             string messageFragment)
    {
        bool failed = false;
        try
        {
            InstallVerified(root, payload, selected);
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
        InstallVerified(root, payload, new string[] { SurvivorId });
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
        InstallVerified(root, payload, new string[] { SurvivorId });
        AssertInstalled(root, payload, originalHash, SurvivorId);
        string before = FingerprintTree(root);

        int exitCode = RunAbortChild(self, root, payload, phase, AwsId, SurvivorId);
        AssertTrue(exitCode == 86, "hard-abort child returned " + exitCode + " instead of 86");
        AssertTrue(File.Exists(Path.Combine(root, ".ncmm-setup.pending.json")),
                   "hard abort did not leave recovery marker");

        bool recovered = SetupCore.RecoverPendingSetupTransaction(root);
        AssertTrue(recovered, "pending setup transaction was not recovered");
        AssertEqual(FingerprintTree(root), before, "hard-abort recovery was not byte-identical at " + phase);

        InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
        AssertInstalled(root, payload, originalHash, AwsId, SurvivorId);
    }

    private static int ChildInstall(string[] args)
    {
        if (args.Length < 3) return 64;
        string root = args[1];
        string payload = args[2];
        string[] selected = args.Skip(3).ToArray();
        InstallVerified(root, payload, selected);
        return 0;
    }

    private static Dictionary<string, object> ReadJsonObject(string path)
    {
        AssertTrue(File.Exists(path), "required JSON state is missing: " + path);
        object value = new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path));
        Dictionary<string, object> result = value as Dictionary<string, object>;
        AssertTrue(result != null, "JSON state root is not an object: " + path);
        return result;
    }

    private static string JsonString(Dictionary<string, object> value, string key)
    {
        object raw;
        return value.TryGetValue(key, out raw) && raw != null ? Convert.ToString(raw) : "";
    }

    private static int JsonInt(Dictionary<string, object> value, string key, int fallback)
    {
        object raw;
        if (!value.TryGetValue(key, out raw) || raw == null) return fallback;
        try { return Convert.ToInt32(raw); } catch { return fallback; }
    }

    private static int RealInstallSmoke(string gameRoot, string payload)
    {
        gameRoot = Path.GetFullPath(gameRoot);
        payload = Path.GetFullPath(payload);
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        AssertTrue(File.Exists(exe), "real CDDA smoke target has no cataclysm-tiles.exe");
        string originalVanilla = Sha256(exe);

        InstallVerified(gameRoot, payload, new string[] { AwsId, SurvivorId, BallisticId, EquipmentBodyMapId });
        AssertInstalled(gameRoot, payload, originalVanilla, AwsId, SurvivorId, BallisticId, EquipmentBodyMapId);

        InstallVerified(gameRoot, payload, new string[] { AwsId, BallisticId, EquipmentBodyMapId });
        AssertInstalled(gameRoot, payload, originalVanilla, AwsId, BallisticId, EquipmentBodyMapId);

        InstallVerified(gameRoot, payload, new string[] { AwsId, SurvivorId, BallisticId, EquipmentBodyMapId });
        AssertInstalled(gameRoot, payload, originalVanilla, AwsId, SurvivorId, BallisticId, EquipmentBodyMapId);

        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine("NCMM Real CDDA Install Preparation: PASS");
        Console.ResetColor();
        return 0;
    }

    private static int RealRuntimeVerifyAndRestore(string gameRoot)
    {
        gameRoot = Path.GetFullPath(gameRoot);
        string exe = Path.Combine(gameRoot, "cataclysm-tiles.exe");
        string vanilla = Path.Combine(gameRoot, "cataclysm-tiles.vanilla.exe");
        AssertTrue(File.Exists(exe), "bootstrap disappeared before runtime verification");
        AssertTrue(File.Exists(vanilla), "vanilla backup disappeared before runtime verification");
        string originalVanilla = Sha256(vanilla);

        string ncmm = Path.Combine(gameRoot, "ncmm");
        try
        {
            AssertTrue(File.Exists(Path.Combine(ncmm, "boot.ready")),
                       "real Host smoke did not publish boot.ready");
            AssertTrue(!File.Exists(Path.Combine(ncmm, "boot.pending")),
                       "real Host smoke left boot.pending");

            Dictionary<string, object> runtime = ReadJsonObject(Path.Combine(ncmm, "runtime.state.json"));
            AssertEqual(JsonString(runtime, "selected_mode"), "NCMM_HOST",
                        "real runtime smoke did not select certified Host");
            AssertEqual(JsonString(runtime, "reason"), "child_exit_0",
                        "real runtime smoke did not finish with clean child exit");
            AssertTrue(JsonInt(runtime, "last_exit_code", -1) == 0,
                       "real runtime smoke child exit code was not zero");

            Dictionary<string, object> modules = ReadJsonObject(Path.Combine(ncmm, "modules.state.json"));
            object rawModules;
            AssertTrue(modules.TryGetValue("modules", out rawModules), "modules.state.json has no modules array");
            object[] moduleArray = rawModules as object[];
            AssertTrue(moduleArray != null, "modules.state.json modules field is not an array");

            Dictionary<string, Dictionary<string, object>> byId =
                new Dictionary<string, Dictionary<string, object>>(StringComparer.Ordinal);
            foreach (object item in moduleArray)
            {
                Dictionary<string, object> module = item as Dictionary<string, object>;
                if (module == null) continue;
                string id = JsonString(module, "id");
                if (!String.IsNullOrEmpty(id)) byId[id] = module;
            }

            foreach (string id in new string[] { AwsId, SurvivorId, BallisticId, EquipmentBodyMapId })
            {
                AssertTrue(byId.ContainsKey(id), "real Host did not report module: " + id);
                Dictionary<string, object> module = byId[id];
                AssertEqual(JsonString(module, "state"), "loaded", "module did not load: " + id);
                AssertEqual(JsonString(module, "lifecycle"), "active", "module lifecycle is not active: " + id);
            }
        }
        finally
        {
            string bootstrapHash = Sha256(exe);
            bool blocked = false;
            try { SetupCore.RestoreVanilla(gameRoot); }
            catch (InvalidOperationException) { blocked = true; }
            AssertTrue(blocked, "RestoreVanilla did not protect native persistent item definitions");
            AssertEqual(Sha256(exe), bootstrapHash, "Blocked restore changed the live executable");
            // A separate disposable no-native-data fixture proves restoration still works.
            string clean = Path.Combine(Path.GetTempPath(), "ncmm-restore-official-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(Path.Combine(clean, "ncmm"));
            try {
                File.Copy(exe, Path.Combine(clean, "cataclysm-tiles.exe"));
                File.Copy(vanilla, Path.Combine(clean, "cataclysm-tiles.vanilla.exe"));
                File.Copy(Path.Combine(ncmm, "installed-components.json"), Path.Combine(clean, "ncmm", "installed-components.json"));
                File.Copy(Path.Combine(ncmm, "vanilla.sha256"), Path.Combine(clean, "ncmm", "vanilla.sha256"));
                SetupCore.RestoreVanilla(clean);
                AssertEqual(Sha256(Path.Combine(clean, "cataclysm-tiles.exe")), originalVanilla,
                            "RestoreVanilla failed on an installation without native persistent data");
            } finally { Directory.Delete(clean, true); }
        }

        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine("NCMM Real Runtime Smoke: PASS (certified Host + AWS + Survivor + Ballistic Hit Chance + Equipment Body Map + clean exit)");
        Console.ResetColor();
        return 0;
    }

    private static void RunAuditCases(string self, string work, string payload)
    {
        Run("deep managed paths survive install, update and rollback", delegate {
            string root = NewGame(work, "deep-path", "deep-path-original");
            InstallVerified(root, payload, new string[] { AwsId });
            string owned = Path.Combine(root, "code_mods", AwsDir, "user-data");
            while (owned.Length < 285) owned = Path.Combine(owned, "long-component-0123456789");
            Directory.CreateDirectory(owned);
            string file = Path.Combine(owned, "keep.txt");
            File.WriteAllText(file, "user-owned-deep-file");
            string before = FingerprintTree(root);
            Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE", "1");
            Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE", "modules_installed");
            try { ExpectInstallFailure(root, payload, new string[] { AwsId, SurvivorId }, "Injected NCMM setup failure"); }
            finally {
                Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE", null);
                Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE", null);
            }
            AssertEqual(FingerprintTree(root), before, "Deep-path rollback changed user files");
            InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
            AssertEqual(File.ReadAllText(file), "user-owned-deep-file", "Deep-path update damaged user file");
        });
        Run("A04 unidentified old bootstrap never replaces vanilla", delegate {
            string root = NewGame(work, "missing-bootstrap-identity", "unidentified-old-bootstrap");
            string vanilla = Path.Combine(root, "cataclysm-tiles.vanilla.exe");
            File.WriteAllText(vanilla, "known-original", Encoding.ASCII);
            string original = Sha256(vanilla), exe = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
            ExpectInstallFailure(root, payload, new string[0], null);
            AssertEqual(Sha256(vanilla), original, "Unknown bootstrap overwrote vanilla");
            AssertEqual(Sha256(Path.Combine(root, "cataclysm-tiles.exe")), exe, "Unknown bootstrap was overwritten");
        });
        Run("A05 data-only disabled Survivor remains discoverable and re-enables", delegate {
            string root = NewGame(work, "data-only-survivor", "data-only");
            InstallVerified(root, payload, new string[] { SurvivorId });
            string dir = Path.Combine(root, "code_mods", SurvivorDir);
            string persistent = FingerprintTree(Path.Combine(dir, "persistent_data"));
            AssertTrue(persistent != "<missing>", "Survivor persistent definitions fixture is absent");
            InstallVerified(root, payload, new string[0]);
            AssertTrue(File.Exists(Path.Combine(dir, "mod.json")), "Data-only module lost its manifest");
            AssertTrue(File.Exists(Path.Combine(dir, "disabled")), "Data-only module lost disabled marker");
            AssertTrue(!File.Exists(Path.Combine(dir, "ncmm_mod.dll")), "Deselected DLL is still executable");
            AssertEqual(FingerprintTree(Path.Combine(dir, "persistent_data")), persistent, "Persistent item data changed");
            InstallVerified(root, payload, new string[] { SurvivorId });
            AssertTrue(!File.Exists(Path.Combine(dir, "disabled")), "Owned disabled marker not cleared");
        });
        Run("A03 damaged rollback snapshot preserves every live file", delegate {
            string root = NewGame(work, "damaged-rollback", "damaged-rollback");
            InstallVerified(root, payload, new string[] { AwsId });
            AssertTrue(RunAbortChild(self, root, payload, "modules_installed", SurvivorId) == 86, "No hard-abort fixture");
            var state = new JavaScriptSerializer().Deserialize<SetupTransactionState>(
                File.ReadAllText(Path.Combine(root, ".ncmm-setup.pending.json")));
            string snapshotExe = Path.Combine(state.backup_root, "snapshot", "cataclysm-tiles.exe");
            File.WriteAllText(snapshotExe, "damaged snapshot", Encoding.ASCII);
            string before = FingerprintTree(root);
            bool rejected = false;
            try { SetupCore.RecoverPendingSetupTransaction(root); } catch (IOException) { rejected = true; }
            AssertTrue(rejected, "Damaged rollback was accepted");
            AssertEqual(FingerprintTree(root), before, "Damaged rollback mutated the live installation");
        });
        Run("A07 same module ID directory rename retires old DLL", delegate {
            string root = NewGame(work, "rename-module", "rename-module");
            InstallVerified(root, payload, new string[] { AwsId });
            string altered = Path.Combine(work, "renamed-payload");
            foreach (string file in Directory.GetFiles(payload, "*", SearchOption.AllDirectories)) {
                string destination = Path.Combine(altered, Relative(payload, file));
                Directory.CreateDirectory(Path.GetDirectoryName(destination)); File.Copy(file, destination);
            }
            Directory.Move(Path.Combine(altered, "code_mods", AwsDir), Path.Combine(altered, "code_mods", "AWS-Renamed"));
            InstallVerified(root, altered, new string[] { AwsId });
            AssertTrue(!File.Exists(Path.Combine(root, "code_mods", AwsDir, "ncmm_mod.dll")), "Duplicate DLL survived rename");
            AssertTrue(File.Exists(Path.Combine(root, "code_mods", "AWS-Renamed", "ncmm_mod.dll")), "New module missing");
        });
    }

    private static int Main(string[] args)
    {
        if (args.Length > 0 && String.Equals(args[0], "--child-install", StringComparison.Ordinal))
            return ChildInstall(args);
        if (args.Length == 3 && String.Equals(args[0], "--real-install-smoke", StringComparison.Ordinal))
            return RealInstallSmoke(args[1], args[2]);
        if (args.Length == 2 && String.Equals(args[0], "--real-runtime-verify", StringComparison.Ordinal))
            return RealRuntimeVerifyAndRestore(args[1]);
        if (args.Length != 1)
        {
            Console.Error.WriteLine("usage: NCMM_InstallationMatrix_Harness.exe <payload-root>");
            Console.Error.WriteLine("   or: NCMM_InstallationMatrix_Harness.exe --real-install-smoke <game-root> <payload-root>");
            Console.Error.WriteLine("   or: NCMM_InstallationMatrix_Harness.exe --real-runtime-verify <game-root>");
            return 2;
        }

        string payload = Path.GetFullPath(args[0]);
        string self = Process.GetCurrentProcess().MainModule.FileName;
        string work = Path.Combine(Path.GetTempPath(), "ncmm-install-matrix-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(work);

        try
        {
            RunAuditCases(self, work, payload);
            Run("clean CDDA -> NCMM without modules", delegate {
                string root = NewGame(work, "clean-none", "vanilla-none");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[0]);
                AssertInstalled(root, payload, original);
            });

            Run("clean CDDA -> Survivor only", delegate {
                string root = NewGame(work, "clean-survivor", "vanilla-survivor");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { SurvivorId });
                AssertInstalled(root, payload, original, SurvivorId);
            });

            Run("clean CDDA -> AWS only", delegate {
                string root = NewGame(work, "clean-aws", "vanilla-aws");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId });
                AssertInstalled(root, payload, original, AwsId);
            });

            Run("clean CDDA -> Ballistic Hit Chance only", delegate {
                string root = NewGame(work, "clean-ballistic", "vanilla-ballistic");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { BallisticId });
                AssertInstalled(root, payload, original, BallisticId);
            });

            Run("clean CDDA -> Equipment Body Map only", delegate {
                string root = NewGame(work, "clean-equipment-body-map", "vanilla-equipment-body-map");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { EquipmentBodyMapId });
                AssertInstalled(root, payload, original, EquipmentBodyMapId);
            });

            Run("Host feed exact SHA, source commit and release URL contracts", delegate {
                string vanillaHash = new string('a', 64);
                string sourceHash = "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83";
                string patch = new string('b', 64);
                SetupCertifiedHostEntry e = new SetupCertifiedHostEntry {
                    source_commit = sourceHash,
                    upstream_tag = "cdda-experimental-2026-10-01-1040",
                    host_url = "https://github.com/Neversalimus/NCMM/releases/download/ncmm-host-cdda-experimental-2026-10-01-1040-r2909/cataclysm-tiles.ncmm.exe",
                    host_sha256 = new string('c', 64),
                    patch_revision = patch,
                    ncmm_version = SetupCore.RuntimeVersion,
                    loader_api = 1
                };
                SetupCertifiedHostFeed feed = new SetupCertifiedHostFeed {
                    schema = 1, loader_api = 1, runtime_version = SetupCore.RuntimeVersion,
                    patch_revision = patch,
                    hosts = new Dictionary<string, SetupCertifiedHostEntry> { { vanillaHash, e } }
                };
                SetupCertifiedHostEntry found; string reason;
                AssertTrue(SetupCore.TrySelectCertifiedHost(feed, vanillaHash, sourceHash, out found, out reason),
                           "matching certified Host was rejected: " + reason);
                AssertTrue(Object.ReferenceEquals(e, found), "wrong Host selected");
                AssertTrue(SetupCore.TrySelectCertifiedHost(feed, vanillaHash, sourceHash.Substring(0, 12),
                    out found, out reason), "dirty CDDA commit prefix was rejected");
                AssertTrue(!SetupCore.TrySelectCertifiedHost(feed, new string('d', 64), sourceHash,
                    out found, out reason), "unlisted vanilla SHA accepted");
                AssertTrue(!SetupCore.TrySelectCertifiedHost(feed, vanillaHash, new string('e', 40),
                    out found, out reason), "wrong source commit accepted");
                e.host_url = "https://malicious.example.com/cataclysm-tiles.ncmm.exe";
                AssertTrue(!SetupCore.TrySelectCertifiedHost(feed, vanillaHash, sourceHash,
                    out found, out reason), "non-GitHub release URL accepted");
                e.host_url = "https://github.com/Neversalimus/NCMM/releases/download/ncmm-host-1040/cataclysm-tiles.ncmm.exe";
                e.patch_revision = new string('e', 64);
                AssertTrue(!SetupCore.TrySelectCertifiedHost(feed, vanillaHash, sourceHash,
                    out found, out reason), "different Host patch revision accepted");
                e.patch_revision = patch;
                feed.runtime_version = "999.0.0";
                AssertTrue(!SetupCore.TrySelectCertifiedHost(feed, vanillaHash, sourceHash,
                    out found, out reason), "wrong runtime version accepted");
            });

            Run("clean CDDA -> all optional modules", delegate {
                string root = NewGame(work, "clean-all", "vanilla-all");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId, BallisticId, EquipmentBodyMapId });
                AssertInstalled(root, payload, original, AwsId, SurvivorId, BallisticId, EquipmentBodyMapId);
            });

            Run("clean CDDA -> Survivor + AWS", delegate {
                string root = NewGame(work, "clean-full", "vanilla-full");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
                AssertInstalled(root, payload, original, AwsId, SurvivorId);
            });

            Run("disable Ballistic Hit Chance -> AWS + Survivor remain", delegate {
                string root = NewGame(work, "disable-ballistic", "vanilla-disable-ballistic");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId, BallisticId, EquipmentBodyMapId });
                string ballisticDir = Path.Combine(root, "code_mods", BallisticDir);
                File.WriteAllText(Path.Combine(ballisticDir, "user-note.txt"), "preserve me\n", Encoding.UTF8);
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
                AssertInstalled(root, payload, original, AwsId, SurvivorId);
                AssertTrue(File.Exists(Path.Combine(ballisticDir, "user-note.txt")),
                           "deselecting Ballistic Hit Chance removed user-owned module files");
            });

            Run("disable Survivor -> AWS remains", delegate {
                string root = NewGame(work, "disable-survivor", "vanilla-disable");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
                string survivorDir = Path.Combine(root, "code_mods", SurvivorDir);
                File.WriteAllText(Path.Combine(survivorDir, "user-note.txt"), "preserve me\n", Encoding.UTF8);
                InstallVerified(root, payload, new string[] { AwsId });
                AssertInstalled(root, payload, original, AwsId);
                AssertTrue(File.Exists(Path.Combine(survivorDir, "user-note.txt")),
                           "deselecting Survivor removed user-owned module files");
            });

            Run("re-enable Survivor after removal", delegate {
                string root = NewGame(work, "reenable-survivor", "vanilla-reenable");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
                string survivorDir = Path.Combine(root, "code_mods", SurvivorDir);
                File.WriteAllText(Path.Combine(survivorDir, "disabled.note"), "persist\n", Encoding.UTF8);
                InstallVerified(root, payload, new string[] { AwsId });
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
                AssertInstalled(root, payload, original, AwsId, SurvivorId);
                AssertTrue(File.Exists(Path.Combine(survivorDir, "disabled.note")),
                           "re-enable lost preserved user file");
            });

            Run("same-version reinstall is safe and idempotent", delegate {
                string root = NewGame(work, "reinstall", "vanilla-reinstall");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
                int backupsBefore = Directory.GetFiles(Path.Combine(root, "ncmm"),
                    "cataclysm-tiles.vanilla.backup-*.exe").Length;
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
                int backupsAfter = Directory.GetFiles(Path.Combine(root, "ncmm"),
                    "cataclysm-tiles.vanilla.backup-*.exe").Length;
                AssertInstalled(root, payload, original, AwsId, SurvivorId);
                AssertTrue(backupsAfter == backupsBefore, "idempotent reinstall created a spurious vanilla archive");
            });

            Run("packaged file inventory removes stale files but preserves user files", delegate {
                string root = NewGame(work, "managed-file-upgrade", "vanilla-managed");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId });
                string dir = Path.Combine(root, "code_mods", AwsDir);
                string obsolete = Path.Combine(dir, "old-definition.json");
                string custom = Path.Combine(dir, "user-notes.txt");
                File.WriteAllText(obsolete, "{\"old\":true}", Encoding.ASCII);
                File.WriteAllText(custom, "mine", Encoding.ASCII);
                SetupInstalledComponents receipt = ReadInstalledState(root);
                SetupInstalledComponent entry = receipt.components.First(x => x.id == AwsId);
                AssertTrue(entry.files != null && entry.files.Count > 0,
                           "managed file inventory was not persisted");
                entry.files.Add("old-definition.json", Sha256(obsolete));
                File.WriteAllText(Path.Combine(root, "ncmm", "installed-components.json"),
                    new JavaScriptSerializer().Serialize(receipt), Encoding.UTF8);
                InstallVerified(root, payload, new string[] { AwsId });
                AssertTrue(!File.Exists(obsolete), "obsolete NCMM-owned file survived upgrade");
                AssertTrue(File.Exists(custom), "upgrade removed user-owned file");
                AssertInstalled(root, payload, original, AwsId);
            });

            Run("modified retired managed file fails closed and restores previous tree", delegate {
                string root = NewGame(work, "managed-file-modified", "vanilla-modified");
                InstallVerified(root, payload, new string[] { AwsId });
                string dir = Path.Combine(root, "code_mods", AwsDir);
                string obsolete = Path.Combine(dir, "old-definition.json");
                File.WriteAllText(obsolete, "original", Encoding.ASCII);
                SetupInstalledComponents receipt = ReadInstalledState(root);
                receipt.components.First(x => x.id == AwsId).files.Add(
                    "old-definition.json", Sha256(obsolete));
                File.WriteAllText(Path.Combine(root, "ncmm", "installed-components.json"),
                    new JavaScriptSerializer().Serialize(receipt), Encoding.UTF8);
                File.WriteAllText(obsolete, "user edited", Encoding.ASCII);
                string before = FingerprintTree(root);
                ExpectInstallFailure(root, payload, new string[] { AwsId }, "Former packaged file was modified");
                AssertEqual(FingerprintTree(root), before, "rollback lost modified file");
            });

            Run("retired NCMM module is disabled without deleting user files", delegate {
                string root = NewGame(work, "retired-module", "vanilla-retired");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId });
                string legacy = Path.Combine(root, "code_mods", "RetiredModule");
                Directory.CreateDirectory(legacy);
                File.WriteAllText(Path.Combine(legacy, "mod.json"),
                    "{\"id\":\"retired_ncmm\",\"version\":\"0.1\"}", Encoding.ASCII);
                File.WriteAllText(Path.Combine(legacy, "ncmm_mod.dll"), "old-dll", Encoding.ASCII);
                File.WriteAllText(Path.Combine(legacy, "my-notes.txt"), "keep", Encoding.ASCII);
                SetupInstalledComponents receipt = ReadInstalledState(root);
                receipt.components.Add(new SetupInstalledComponent {
                    id = "retired_ncmm", directory = "RetiredModule", version = "0.1"
                });
                File.WriteAllText(Path.Combine(root, "ncmm", "installed-components.json"),
                    new JavaScriptSerializer().Serialize(receipt), Encoding.UTF8);
                InstallVerified(root, payload, new string[] { AwsId });
                AssertTrue(!File.Exists(Path.Combine(legacy, "mod.json")) &&
                           !File.Exists(Path.Combine(legacy, "ncmm_mod.dll")),
                           "retired module remains loadable");
                AssertTrue(File.Exists(Path.Combine(legacy, "my-notes.txt")),
                           "retiring module removed user notes");
                AssertInstalled(root, payload, original, AwsId);
            });

            Run("RestoreVanilla rejects damaged backup and restores verified bytes", delegate {
                string root = NewGame(work, "restore-hash-check", "vanilla-restore-check");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { AwsId });
                string exe = Path.Combine(root, "cataclysm-tiles.exe");
                string vanilla = Path.Combine(root, "cataclysm-tiles.vanilla.exe");
                byte[] originalBytes = File.ReadAllBytes(vanilla);
                string bootstrapHash = Sha256(exe);
                File.AppendAllText(vanilla, "broken", Encoding.ASCII);
                bool rejected = false;
                try { SetupCore.RestoreVanilla(root); }
                catch (InvalidOperationException ex) {
                    rejected = ex.Message.Contains("SHA256 mismatch");
                }
                AssertTrue(rejected, "damaged vanilla backup was accepted");
                AssertEqual(Sha256(exe), bootstrapHash, "failed vanilla restore mutated bootstrap");
                File.WriteAllBytes(vanilla, originalBytes);
                SetupCore.RestoreVanilla(root);
                AssertEqual(Sha256(exe), original, "verified vanilla was not restored");
                AssertTrue(Directory.GetFiles(root, "*.ncmm-restore-*.tmp").Length == 0,
                           "restore staged file leaked");
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
                InstallVerified(root, payload, new string[] { SurvivorId });
                AssertInstalled(root, payload, original, SurvivorId);
            });

            Run("corrupted Survivor DLL is repaired by reinstall", delegate {
                string root = NewGame(work, "repair-dll", "vanilla-repair-dll");
                string original = Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
                InstallVerified(root, payload, new string[] { SurvivorId });
                string dll = Path.Combine(root, "code_mods", SurvivorDir, "ncmm_mod.dll");
                File.AppendAllText(dll, "corruption", Encoding.ASCII);
                AssertTrue(Sha256(dll) != Sha256(Path.Combine(payload, "code_mods", SurvivorDir, "ncmm_mod.dll")),
                           "DLL corruption fixture failed");
                InstallVerified(root, payload, new string[] { SurvivorId });
                AssertInstalled(root, payload, original, SurvivorId);
            });

            Run("corrupted managed manifest fails closed and rolls back", delegate {
                string root = NewGame(work, "corrupt-manifest", "vanilla-corrupt-manifest");
                InstallVerified(root, payload, new string[] { AwsId, SurvivorId });
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
