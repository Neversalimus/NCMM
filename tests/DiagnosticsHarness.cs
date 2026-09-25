using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;

internal static class DiagnosticsHarness
{
    private static int failures = 0;

    private static void AssertTrue(bool condition, string message)
    {
        if (condition)
        {
            Console.WriteLine("PASS " + message);
        }
        else
        {
            Console.Error.WriteLine("FAIL " + message);
            failures++;
        }
    }

    private static void WriteBytes(string path, string text)
    {
        File.WriteAllBytes(path, Encoding.ASCII.GetBytes(text));
    }

    private static void WriteJson(string path, object value)
    {
        File.WriteAllText(path, new JavaScriptSerializer().Serialize(value), Encoding.UTF8);
    }

    private static string NewGameRoot(string root, string name)
    {
        string game = Path.Combine(root, name);
        Directory.CreateDirectory(game);
        Directory.CreateDirectory(Path.Combine(game, "ncmm"));
        Directory.CreateDirectory(Path.Combine(game, "code_mods"));
        WriteBytes(Path.Combine(game, "cataclysm-tiles.exe"), "bootstrap-" + name);
        WriteBytes(Path.Combine(game, "cataclysm-tiles.vanilla.exe"), "vanilla-" + name);
        WriteBytes(Path.Combine(game, "cataclysm-tiles.ncmm.exe"), "host-" + name);
        File.WriteAllText(Path.Combine(game, "VERSION.txt"),
            "commit sha: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n", Encoding.ASCII);

        string activeSha = SetupCore.Sha256(Path.Combine(game, "cataclysm-tiles.exe")).ToLowerInvariant();
        string vanillaSha = SetupCore.Sha256(Path.Combine(game, "cataclysm-tiles.vanilla.exe")).ToLowerInvariant();
        string hostSha = SetupCore.Sha256(Path.Combine(game, "cataclysm-tiles.ncmm.exe")).ToLowerInvariant();

        File.WriteAllText(Path.Combine(game, "ncmm", "bootstrap.sha256"), activeSha + "\n", Encoding.ASCII);
        File.WriteAllText(Path.Combine(game, "ncmm", "vanilla.sha256"), vanillaSha + "\n", Encoding.ASCII);

        WriteJson(Path.Combine(game, "ncmm", "host.binding.json"), new
        {
            vanilla_sha256 = vanillaSha,
            host_sha256 = hostSha,
            source_commit = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            upstream_tag = "cdda-experimental-test",
            patch_revision = new string('b', 64),
            ncmm_version = "0.7.0",
            loader_api = 1,
            installed_utc = DateTime.UtcNow.ToString("o")
        });

        WriteJson(Path.Combine(game, "ncmm", "runtime.state.json"), new
        {
            schema = 1,
            runtime_version = "0.7.0",
            loader_api = 1,
            updated_utc = DateTime.UtcNow.ToString("o"),
            source_commit = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            vanilla_sha256 = vanillaSha,
            host_sha256 = hostSha,
            binding_host_sha256 = hostSha,
            host_valid = true,
            host_status = "valid",
            feed_status = "current",
            selected_mode = "NCMM_HOST",
            reason = "certified_host",
            manual_disabled = false,
            auto_disabled = false,
            boot_pending = false,
            offline = false,
            refresh_requested = false,
            diagnostics_only = false,
            last_exit_code = (int?)0
        });

        File.WriteAllText(Path.Combine(game, "ncmm", "boot.ready"), "ready\n", Encoding.ASCII);
        return game;
    }

    private static void AddModule(string game, string folder, string id, bool disabled)
    {
        string dir = Path.Combine(game, "code_mods", folder);
        Directory.CreateDirectory(dir);
        WriteBytes(Path.Combine(dir, "ncmm_mod.dll"), "dummy-" + folder);
        WriteJson(Path.Combine(dir, "mod.json"), new
        {
            id = id,
            name = folder,
            version = "1.0.0",
            loader_api = 1,
            requires = new string[] { "core.v1" },
            failure_policy = "disable"
        });
        if (disabled)
            File.WriteAllText(Path.Combine(dir, "disabled"), "disabled\n", Encoding.ASCII);
    }

    private static void WriteModuleState(string game, object[] modules)
    {
        WriteJson(Path.Combine(game, "ncmm", "modules.state.json"), new
        {
            schema = 2,
            host_version = "0.7.0",
            loader_api = 1,
            capabilities = new string[] { "core.v1" },
            modules = modules
        });
    }

    private static void RunDuplicateScenario(string root)
    {
        string game = NewGameRoot(root, "duplicate");
        AddModule(game, "Alpha", "same_id", false);
        AddModule(game, "Beta", "same_id", false);
        WriteModuleState(game, new object[] {
            new { id="same_id", name="Alpha", version="1.0.0", state="rejected",
                  reason="duplicate_module_id", default_hotkey="", directory="Alpha" },
            new { id="same_id", name="Beta", version="1.0.0", state="rejected",
                  reason="duplicate_module_id", default_hotkey="", directory="Beta" }
        });

        DiagnosticsReport report = SetupCore.Diagnose(game);
        AssertTrue(report.Errors > 0, "duplicate active IDs raise diagnostics error");
        AssertTrue(report.Text.Contains("Duplicate active module id 'same_id'"), "duplicate folders are named");
        AssertTrue(report.Text.Contains("modules.state.json contains duplicate module id 'same_id'"), "duplicate host state is detected");
        AssertTrue(report.Text.Contains("=== Bootstrap Runtime State ==="), "runtime state section present");
        AssertTrue(report.Text.Contains("=== Host Module State ==="), "module state section present");
        AssertTrue(report.Text.Contains("Binding: NCMM=0.7.0"), "binding identity reported");
        AssertTrue(!String.IsNullOrEmpty(report.SavedPath) && File.Exists(report.SavedPath),
            "diagnostics-latest.txt exported");
    }

    private static void RunDisabledDuplicateScenario(string root)
    {
        string game = NewGameRoot(root, "disabled-duplicate");
        AddModule(game, "Alpha", "same_id", false);
        AddModule(game, "Beta", "same_id", true);
        WriteModuleState(game, new object[] {
            new { id="same_id", name="Alpha", version="1.0.0", state="loaded",
                  reason="ok", default_hotkey="", directory="Alpha" },
            new { id="same_id", name="Beta", version="1.0.0", state="disabled",
                  reason="user_disabled", default_hotkey="", directory="Beta" }
        });

        DiagnosticsReport report = SetupCore.Diagnose(game);
        AssertTrue(!report.Text.Contains("Duplicate active module id 'same_id'"),
            "disabled duplicate does not block active identity");
        AssertTrue(!report.Text.Contains("modules.state.json contains duplicate module id 'same_id'"),
            "disabled duplicate does not create a host-state duplicate error");
        AssertTrue(report.Text.Contains("Module summary: loaded=1 | disabled=1"),
            "module summary reports loaded and disabled modules");
    }

    private static void RunRuntimeFaultScenario(string root)
    {
        string game = NewGameRoot(root, "runtime-fault");
        AddModule(game, "Faulty", "faulty_mod", false);
        WriteModuleState(game, new object[] {
            new { id="faulty_mod", name="Faulty", version="1.0.0", state="runtime_fault",
                  reason="turn_exception", default_hotkey="", directory="Faulty" }
        });

        DiagnosticsReport report = SetupCore.Diagnose(game);
        AssertTrue(report.Warnings > 0, "runtime fault surfaces as diagnostics warning");
        AssertTrue(report.Text.Contains("turn callback quarantined after exception"),
            "runtime fault reason is human-readable");
        AssertTrue(report.Text.Contains("runtime_fault=1"), "runtime fault counted in summary");
    }

    public static int Main()
    {
        string root = Path.Combine(Path.GetTempPath(),
            "ncmm-diagnostics2-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            RunDuplicateScenario(root);
            RunDisabledDuplicateScenario(root);
            RunRuntimeFaultScenario(root);

            if (failures != 0)
            {
                Console.Error.WriteLine("NCMM Diagnostics 2.0 Harness: FAIL (" + failures.ToString() + ")");
                return 1;
            }
            Console.WriteLine("NCMM Diagnostics 2.0 Harness: PASS");
            return 0;
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
        }
    }
}
