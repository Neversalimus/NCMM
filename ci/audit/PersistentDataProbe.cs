using System;
using System.IO;

// Audit fixture only; production SetupCore is compiled without edits.
internal static class PersistentDataProbe
{
    private static void Put(string path,string content) {
        Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllText(path,content);
    }
    private static void Copy(string from,string to) {
        Directory.CreateDirectory(to);
        foreach(string file in Directory.GetFiles(from)) File.Copy(file,Path.Combine(to,Path.GetFileName(file)),true);
        foreach(string dir in Directory.GetDirectories(from)) Copy(dir,Path.Combine(to,Path.GetFileName(dir)));
    }
    private static int Main(string[] args) {
        string root=Path.Combine(Path.GetTempPath(),"ncmm-persistent-audit-"+Guid.NewGuid().ToString("N"));
        string game=Path.Combine(root,"game"),payload=Path.Combine(root,"payload");
        Put(Path.Combine(game,"cataclysm-tiles.exe"),"fake-vanilla-never-executed");
        Put(Path.Combine(payload,"cataclysm-tiles.ncmm-bootstrap.exe"),"fake-bootstrap-never-executed");
        string src=Path.Combine(args[0],"mods","SurvivorProgression");
        string module=Path.Combine(payload,"code_mods","SurvivorProgression");
        Directory.CreateDirectory(module);
        File.Copy(Path.Combine(src,"mod.json"),Path.Combine(module,"mod.json"));
        Copy(Path.Combine(src,"persistent_data"),Path.Combine(module,"persistent_data"));
        Put(Path.Combine(module,"ncmm_mod.dll"),"fake-DLL-not-loaded");
        InstallResult first=SetupCore.Install(game,payload);
        InstallResult second=SetupCore.Install(game,payload,new string[0]);
        string installed=Path.Combine(game,"code_mods","SurvivorProgression");
        int data=Directory.GetFiles(Path.Combine(installed,"persistent_data"),"*.json",SearchOption.AllDirectories).Length;
        bool manifest=File.Exists(Path.Combine(installed,"mod.json")),dll=File.Exists(Path.Combine(installed,"ncmm_mod.dll"));
        Console.WriteLine("BASELINE=4a83414a1bf4147860b0e3dd49a9aded0c5581d0");
        Console.WriteLine("firstVerified="+first.CompletionVerified+" disabledVerified="+second.CompletionVerified+
            " persistentJsonFiles="+data+" modManifestExists="+manifest+" moduleDllExists="+dll);
        Console.WriteLine("Production Host fallback discovery requires mod.json; actual game-save consequences are not executed by this fixture.");
        if(!first.CompletionVerified || !second.CompletionVerified || data<2 || manifest || dll) return 1;
        Console.WriteLine("REPRODUCED | GUI-style deselection preserves Survivor JSON files but removes their discovery manifest");
        return 0;
    }
}
