// Execute real shared I/O and actual Bootstrap binding validation (no network).
using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Web.Script.Serialization;

internal static class AuditIOHarness
{
    private static void Check(bool value, string message) { if (!value) throw new Exception(message); }
    private static void Write(string path, string text) { Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllText(path, text, Encoding.ASCII); }
    private static string H(string root) { return Path.Combine(root, "cataclysm-tiles.ncmm.exe"); }
    private static string B(string root) { return Path.Combine(root, "ncmm", "host.binding.json"); }
    private static int Child(string mode, string root, string phase)
    {
        if(mode == "lock") {
            try { using (NcmmInstallLock gate = NcmmInstallLock.Acquire(root)) { return 1; } }
            catch (IOException) { return 77; }
        }
        string staged = Path.Combine(root, "incoming.exe"); Write(staged, "new-host");
        Environment.SetEnvironmentVariable("NCMM_HOST_MATRIX_TEST_MODE", "1");
        Environment.SetEnvironmentVariable("NCMM_HOST_MATRIX_ABORT_PHASE", phase);
        NcmmRuntimeIO.PublishHost(root, staged, "new-binding", NcmmRuntimeIO.Hash(staged)); return 1;
    }
    private static int Spawn(string mode, string root, string phase)
    {
        var start = new ProcessStartInfo(Process.GetCurrentProcess().MainModule.FileName,
            "--child " + mode + " \"" + root + "\" " + phase);
        start.UseShellExecute=false; start.CreateNoWindow=true;
        using(var process=Process.Start(start)) {
            Check(process.WaitForExit(30000), "Child hung in " + mode); return process.ExitCode;
        }
    }
    private static void HostCrash(string work, string phase, bool old)
    {
        string root=Path.Combine(work,"host-"+phase+"-"+old); Directory.CreateDirectory(root);
        if(old) { Write(H(root),"old-host"); Write(B(root),"old-binding"); }
        Check(Spawn("host",root,phase)==86,"Host did not abort at "+phase);
        Check(NcmmRuntimeIO.RecoverHost(root),"Missing pending Host transaction");
        bool committed=phase=="committed";
        if(old || committed) {
            Check(File.ReadAllText(H(root))==(committed?"new-host":"old-host"),"Wrong Host after recovery");
            Check(File.ReadAllText(B(root))==(committed?"new-binding":"old-binding"),"Mismatched binding after recovery");
        } else Check(!File.Exists(H(root)) && !File.Exists(B(root)),"First install left partial pair");
        Check(!NcmmRuntimeIO.RecoverHost(root),"Recovery not idempotent");
    }
    private static int Main(string[] args)
    {
        if(args.Length>=3 && args[0]=="--child") return Child(args[1],args[2],args.Length>3?args[3]:"");
        string work=Path.Combine(Path.GetTempPath(),"ncmm-audit-io-"+Guid.NewGuid().ToString("N")); Directory.CreateDirectory(work);
        try {
            foreach(string phase in new[]{"prepared","host_published","binding_published","committed"})
                foreach(bool old in new[]{false,true}) HostCrash(work,phase,old);
            string root=Path.Combine(work,"corrupt-host"); Directory.CreateDirectory(root);
            Write(H(root),"old-host"); Write(B(root),"old-binding");
            Check(Spawn("host",root,"host_published")==86,"Host abort missing");
            var journal=new JavaScriptSerializer().Deserialize<NcmmHostJournal>(File.ReadAllText(Path.Combine(root,".ncmm-host.pending.json")));
            Write(Path.Combine(root,".ncmm-host-tx-"+journal.id,"ncmm","host.binding.json"),"corrupt");
            bool rejected=false; try {NcmmRuntimeIO.RecoverHost(root);} catch(IOException){rejected=true;}
            Check(rejected,"Corrupt Host backup was accepted");
            Check(File.ReadAllText(H(root))=="new-host" && File.ReadAllText(B(root))=="old-binding","Corrupt recovery changed live files");
            root=Path.Combine(work,"lock"); Directory.CreateDirectory(root);
            using(var gate=NcmmInstallLock.Acquire(root)) {
                using(var nested=NcmmInstallLock.Acquire(root)) Check(Spawn("lock",root,"")==77,"Concurrent mutation not blocked");
            }
            using(var gate=NcmmInstallLock.Acquire(root)) {} // unlocked after dispose
            string sha=new string('b',40);
            Check(NcmmRuntimeIO.SourceMatches(sha,sha.Substring(0,12)+"-dirty"),"Commit prefix rejected");
            Check(!NcmmRuntimeIO.SourceMatches(sha,"garbage"),"Malformed commit accepted");
            root=Path.Combine(work,"bootstrap"); Directory.CreateDirectory(root);
            Write(H(root),"AAA"); string digest=NcmmRuntimeIO.Hash(H(root)); DateTime timestamp=File.GetLastWriteTimeUtc(H(root));
            Write(B(root),new JavaScriptSerializer().Serialize(new HostBinding {
                vanilla_sha256=new string('a',64), host_sha256=digest,source_commit=sha,
                ncmm_version="0.8.2",loader_api=1,patch_revision=new string('c',64)}));
            Type bootstrap=typeof(NCMMBootstrap);
            bootstrap.GetField("Root",BindingFlags.Static|BindingFlags.NonPublic).SetValue(null,root);
            bootstrap.GetField("NcmmDir",BindingFlags.Static|BindingFlags.NonPublic).SetValue(null,Path.Combine(root,"ncmm"));
            MethodInfo valid=bootstrap.GetMethod("HasValidLocalHost",BindingFlags.Static|BindingFlags.NonPublic);
            object[] inputs={new string('a',64),sha.Substring(0,12)+"-dirty"};
            Check((bool)valid.Invoke(null,inputs),"Actual Bootstrap rejected valid prefix binding");
            Write(H(root),"BBB"); File.SetLastWriteTimeUtc(H(root),timestamp);
            Check(!(bool)valid.Invoke(null,inputs),"Actual Bootstrap trusted stale metadata-only digest");
            Write(Path.Combine(root,"code_mods","SurvivorProgression","persistent_data","items.json"),"[]");
            Check(NcmmRuntimeIO.HasSaveCriticalData(root),"Orphan persistent definitions were ignored");
            // Directory junctions require no elevation on the Windows CI filesystem.
            string outside=Path.Combine(work,"outside"), link=Path.Combine(root,"junction"); Directory.CreateDirectory(outside);
            Write(Path.Combine(outside,"keep.txt"),"untouched");
            var mklink=new ProcessStartInfo("cmd.exe","/d /c mklink /J \""+link+"\" \""+outside+"\"") {UseShellExecute=false,CreateNoWindow=true};
            using(var process=Process.Start(mklink)) { process.WaitForExit(); Check(process.ExitCode==0,"Junction fixture creation failed"); }
            try {rejected=false;try{NcmmRuntimeIO.GuardTree(root);}catch(IOException){rejected=true;}
                Check(rejected,"Managed junction not rejected");Check(File.ReadAllText(Path.Combine(outside,"keep.txt"))=="untouched","Outside file changed");}
            finally {Directory.Delete(link);}
            Console.WriteLine("NCMM audit shared I/O: PASS (8 hard-crash phases, corrupt recovery, process lock, actual Bootstrap, junction, save guard)"); return 0;
        } finally {Directory.Delete(work,true);}
    }
}
