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
            Require(SetupCore.DiscoverBundledModules(Path.Combine(payload, "code_mods")).Any(m => m.Manifest.id == "first_person_view"), "First Person View absent from installer selection");
            string[] selection = { "first_person_view" };
            InstallResult first = SetupCore.Install(root, payload, selection);
            Require(first.CompletionVerified, "Install was not verified");
            Require(SetupCore.SyncCertifiedHost(root).Ready, "Bundled Host missing");
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
            string damaged = Path.Combine(work, "damaged-payload");
            Copy(payload, damaged);
            File.AppendAllText(Path.Combine(damaged, "host", "cataclysm-tiles.ncmm.exe"), "corruption");
            before = Tree(root); failed = false;
            try { SetupCore.Install(root, damaged, selection); } catch (InvalidOperationException) { failed = true; }
            Require(failed && Tree(root) == before, "Damaged Host accepted or changed installation");
            Require(SetupCore.Install(args[0], payload).CompletionVerified, "Actual game installation failed");
            Require(SetupCore.SyncCertifiedHost(args[0]).Ready, "Actual game Host not ready");
            Console.WriteLine("First Person View install/disable/re-enable/rollback/restore/identity: PASS (" + checks + " checks).");
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
