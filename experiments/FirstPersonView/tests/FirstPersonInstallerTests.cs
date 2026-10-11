using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;

internal static class FirstPersonInstallerTests
{
    private static int checks;
    private static void Require(bool ok, string reason)
    {
        checks++;
        if (!ok) throw new InvalidOperationException(reason);
    }
    private static string Tree(string root)
    {
        return String.Join("\n", Directory.GetFiles(root, "*", SearchOption.AllDirectories)
            .Where(p => Path.GetFileName(p) != ".ncmm-install.lock")
            .OrderBy(p => p, StringComparer.Ordinal).Select(p => p.Substring(root.Length) + "|" + SetupCore.Sha256(p)).ToArray());
    }

    private static void CheckUpgrade(string root, string game, string payload, string bootstrap,
        string receiptDirectory, bool uppercase)
    {
        Directory.CreateDirectory(root);
        File.Copy(Path.Combine(game, "cataclysm-tiles.exe"), Path.Combine(root, "cataclysm-tiles.vanilla.exe"));
        File.Copy(bootstrap, Path.Combine(root, "cataclysm-tiles.exe"));
        File.Copy(Path.Combine(game, "VERSION.txt"), Path.Combine(root, "VERSION.txt"));
        string original = SetupCore.Sha256(Path.Combine(root, "cataclysm-tiles.vanilla.exe"));
        string receipt = Path.Combine(root, receiptDirectory);
        Directory.CreateDirectory(receipt);
        string oldHash = SetupCore.Sha256(bootstrap);
        File.WriteAllText(Path.Combine(receipt, "bootstrap.sha256"), (uppercase ? oldHash.ToUpperInvariant() : oldHash) + "\r\n");
        string note = Path.Combine(receipt, "player-note.txt");
        File.WriteAllText(note, "existing-launcher-note");
        Directory.CreateDirectory(Path.Combine(root, "save"));
        string save = Path.Combine(root, "save", "survivor.sav");
        File.WriteAllText(save, "existing-upgrade-save");
        string before = Tree(root);
        bool failed = false;
        Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE", "1");
        Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE", "preview_host_installed");
        try { SetupCore.Install(root, payload, new[] { "first_person_view" }); } catch (IOException) { failed = true; }
        finally { Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE", null); Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE", null); }
        Require(failed && Tree(root) == before, "Existing bootstrap upgrade did not roll back exactly");
        Require(SetupCore.Install(root, payload, new[] { "first_person_view" }).CompletionVerified, "Existing bootstrap upgrade failed");
        Require(SetupCore.SyncCertifiedHost(root).Ready, "Upgraded Host not ready");
        Require(SetupCore.Sha256(Path.Combine(root, "cataclysm-tiles.vanilla.exe")) == original, "Upgrade replaced the official backup");
        JavaScriptSerializer json = new JavaScriptSerializer();
        SetupHostBinding binding = json.Deserialize<SetupHostBinding>(File.ReadAllText(Path.Combine(root, "ncmm", "host.binding.json")));
        FirstPersonBundle installed = json.Deserialize<FirstPersonBundle>(File.ReadAllText(Path.Combine(root, "ncmm", "first-person-preview.json")));
        Require(binding.vanilla_sha256 == original && installed.vanilla_sha256 == original, "Upgrade bound the wrong official executable variant");
        Require(File.ReadAllText(note) == "existing-launcher-note" && File.ReadAllText(save) == "existing-upgrade-save", "Upgrade changed user files");
        if (receiptDirectory == "cml") Require(File.Exists(Path.Combine(root, "ncmm", "migrated-from-cml.txt")), "Legacy CML was not migrated");
        SetupCore.RestoreVanilla(root);
        Require(SetupCore.Sha256(Path.Combine(root, "cataclysm-tiles.exe")) == original &&
            File.ReadAllText(save) == "existing-upgrade-save", "Upgraded installation did not restore its own variant");
    }
    private static int Main(string[] args)
    {
        string work = Path.Combine(Path.GetTempPath(), "ncmm-first-person-" + Guid.NewGuid().ToString("N"));
        try
        {
            string root = Path.Combine(work, "game");
            Directory.CreateDirectory(root);
            File.Copy(Path.Combine(args[0], "cataclysm-tiles.exe"), Path.Combine(root, "cataclysm-tiles.exe"));
            File.Copy(Path.Combine(args[0], "VERSION.txt"), Path.Combine(root, "VERSION.txt"));
            Directory.CreateDirectory(Path.Combine(root, "save", "Untouched"));
            string save = Path.Combine(root, "save", "Untouched", "survivor.sav");
            File.WriteAllText(save, "existing-save-fixture");
            string saveHash = SetupCore.Sha256(save);
            string original = SetupCore.Sha256(Path.Combine(root, "cataclysm-tiles.exe"));
            string payload = args[1];
            JavaScriptSerializer json = new JavaScriptSerializer();
            FirstPersonBundle bundle = json.Deserialize<FirstPersonBundle>(File.ReadAllText(Path.Combine(payload, "first-person-bundle.json")));
            Require(SetupCore.Sha256(args[2]) != bundle.files["cataclysm-tiles.ncmm-bootstrap.exe"], "Upgrade fixture is the preview bootstrap itself");
            Require(SetupCore.DiscoverBundledModules(Path.Combine(payload, "code_mods")).Any(m => m.Manifest.id == "first_person_view"), "First Person View absent from installer selection");
            string[] selection = { "first_person_view" };
            InstallResult first = SetupCore.Install(root, payload, selection);
            Require(first.CompletionVerified, "Install was not verified");
            Require(SetupCore.SyncCertifiedHost(root).Ready, "Bundled Host missing");
            Require(SetupCore.SyncCertifiedHost(root).Message.Contains(bundle.upstream_tag.Substring("cdda-experimental-".Length)), "Host status identifies the wrong game version");
            Require(SetupCore.IsModuleInstalled(root, "FirstPersonView", "first_person_view"), "Module missing");
            Require(SetupCore.Sha256(Path.Combine(root, "cataclysm-tiles.vanilla.exe")) == original, "Vanilla backup changed");
            string note = Path.Combine(root, "code_mods", "FirstPersonView", "player-note.txt");
            File.WriteAllText(note, "preserve-user-note");
            Require(SetupCore.Install(root, payload, new string[0]).CompletionVerified, "Disable failed");
            Require(!SetupCore.IsModuleInstalled(root, "FirstPersonView", "first_person_view") && File.Exists(note), "Disable removed user files or kept runnable module");
            Require(SetupCore.Install(root, payload, selection).CompletionVerified, "Re-enable failed");
            string before = Tree(root);
            Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE", "1");
            Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE", "preview_host_installed");
            bool failed = false;
            try { SetupCore.Install(root, payload, new string[0]); } catch (IOException) { failed = true; }
            finally { Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_THROW_PHASE", null); Environment.SetEnvironmentVariable("NCMM_SETUP_MATRIX_TEST_MODE", null); }
            Require(failed && Tree(root) == before, "Host/module transaction did not roll back exactly");
            Require(!File.Exists(Path.Combine(root, ".ncmm-setup.pending.json")), "Transaction marker leaked");
            SetupCore.RestoreVanilla(root);
            Require(SetupCore.Sha256(Path.Combine(root, "cataclysm-tiles.exe")) == original, "Vanilla restore differs");
            Require(SetupCore.Sha256(save) == saveHash, "Save changed");
            File.WriteAllText(Path.Combine(root, "cataclysm-tiles.exe"), "wrong-official-build");
            before = Tree(root); failed = false;
            try { SetupCore.Install(root, payload, selection); } catch (InvalidOperationException) { failed = true; }
            Require(failed && Tree(root) == before, "Unknown executable was accepted or changed");
            File.Copy(Path.Combine(args[0], "cataclysm-tiles.exe"), Path.Combine(root, "cataclysm-tiles.exe"), true);
            string version = Path.Combine(root, "VERSION.txt");
            string versionText = File.ReadAllText(version);
            string otherSource = bundle.source_commit == "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83" ?
                "074aa98bd5be3de4c35f154082db32a0e63bb0f1" : "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83";
            File.WriteAllText(version, "commit sha: " + otherSource);
            before = Tree(root); failed = false;
            try { SetupCore.Install(root, payload, selection); } catch (InvalidOperationException) { failed = true; }
            Require(failed && Tree(root) == before, "Other supported source version was accepted or changed installation");
            File.WriteAllText(version, versionText);
            string mismatched = Path.Combine(work, "mismatched-payload");
            Copy(payload, mismatched);
            bundle.source_commit = otherSource;
            File.WriteAllText(Path.Combine(mismatched, "first-person-bundle.json"), json.Serialize(bundle));
            before = Tree(root); failed = false;
            try { SetupCore.Install(root, mismatched, selection); } catch (InvalidOperationException) { failed = true; }
            Require(failed && Tree(root) == before, "Mismatched source/tag pair was accepted or changed installation");
            string damaged = Path.Combine(work, "damaged-payload");
            Copy(payload, damaged);
            File.AppendAllText(Path.Combine(damaged, "host", "cataclysm-tiles.ncmm.exe"), "corruption");
            before = Tree(root); failed = false;
            try { SetupCore.Install(root, damaged, selection); } catch (InvalidOperationException) { failed = true; }
            Require(failed && Tree(root) == before, "Damaged Host accepted or changed installation");
            string missing = Path.Combine(work, "missing-backup");
            Directory.CreateDirectory(Path.Combine(missing, "ncmm"));
            File.Copy(args[2], Path.Combine(missing, "cataclysm-tiles.exe"));
            File.Copy(Path.Combine(args[0], "VERSION.txt"), Path.Combine(missing, "VERSION.txt"));
            File.WriteAllText(Path.Combine(missing, "ncmm", "bootstrap.sha256"), SetupCore.Sha256(args[2]).ToUpperInvariant());
            before = Tree(missing); failed = false;
            try { SetupCore.Install(missing, payload, selection); }
            catch (InvalidOperationException ex) { failed = ex.Message.Contains("backup is missing"); }
            Require(failed && Tree(missing) == before, "Missing original backup was accepted, changed, or misdiagnosed");
            File.Copy(Path.Combine(args[0], "cataclysm-tiles.exe"), Path.Combine(missing, "cataclysm-tiles.vanilla.exe"));
            File.WriteAllText(Path.Combine(missing, "ncmm", "bootstrap.sha256"), new string('0', 64));
            before = Tree(missing); failed = false;
            try { SetupCore.Install(missing, payload, selection); }
            catch (InvalidOperationException ex) { failed = ex.Message.Contains("SHA256: " + SetupCore.Sha256(args[2])); }
            Require(failed && Tree(missing) == before, "Unknown bootstrap was accepted through its backup or misdiagnosed");
            string[] games = new[] { args[0] }.Concat(args.Skip(3)).ToArray();
            for (int variant = 0; variant < games.Length; variant++)
            {
                CheckUpgrade(Path.Combine(work, "upgrade-" + variant + "-ncmm"), games[variant], payload, args[2], "ncmm", false);
                CheckUpgrade(Path.Combine(work, "upgrade-" + variant + "-ncmm-uppercase"), games[variant], payload, args[2], "ncmm", true);
                CheckUpgrade(Path.Combine(work, "upgrade-" + variant + "-cml"), games[variant], payload, args[2], "cml", true);
                string expected = SetupCore.Sha256(Path.Combine(games[variant], "cataclysm-tiles.exe"));
                Require(SetupCore.Install(games[variant], payload).CompletionVerified, "Actual game variant installation failed");
                Require(SetupCore.SyncCertifiedHost(games[variant]).Ready, "Actual game variant Host not ready");
                SetupHostBinding actual = json.Deserialize<SetupHostBinding>(File.ReadAllText(Path.Combine(games[variant], "ncmm", "host.binding.json")));
                Require(actual.vanilla_sha256 == expected && SetupCore.Sha256(Path.Combine(games[variant], "cataclysm-tiles.vanilla.exe")) == expected,
                    "Actual game variant hash or original backup differs");
            }
            Console.WriteLine("First Person View install/upgrade/disable/re-enable/rollback/restore/identity: PASS (" + checks + " checks; " + games.Length + " official packages).");
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
        finally { if (Directory.Exists(work)) Directory.Delete(work, true); }
    }
    private static void Copy(string source, string destination)
    {
        Directory.CreateDirectory(destination);
        foreach (string file in Directory.GetFiles(source)) File.Copy(file, Path.Combine(destination, Path.GetFileName(file)));
        foreach (string dir in Directory.GetDirectories(source)) Copy(dir, Path.Combine(destination, Path.GetFileName(dir)));
    }
}
