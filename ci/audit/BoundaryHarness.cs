using System;
using System.IO;
using System.Text;
using System.Reflection;
using System.Collections.Generic;
using System.Diagnostics;
using System.Web.Script.Serialization;

// Audit-only: compile with unchanged production SetupCore/Bootstrap.
// All mutations are in temporary fixtures; fake executables are never launched.
internal static class AuditBoundaryHarness
{
    private static string Work;
    private static readonly JavaScriptSerializer Json = new JavaScriptSerializer();
    private const string Commit = "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83";
    private static int Errors;
    private static void Write(string path, string value) {
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        File.WriteAllText(path, value, new UTF8Encoding(false));
    }
    private static void Require(bool value, string message) { if (!value) throw new Exception(message); }
    private static string Game(string name) {
        string p=Path.Combine(Work,name); Directory.CreateDirectory(p);
        Write(Path.Combine(p,"cataclysm-tiles.exe"),"original-vanilla-fixture");
        Write(Path.Combine(p,"VERSION.txt"),"commit sha: " + Commit + "\n"); return p;
    }
    private static string Payload(string name,string folder,string version,string bootstrap) {
        string p=Path.Combine(Work,name); Directory.CreateDirectory(p);
        Write(Path.Combine(p,"cataclysm-tiles.ncmm-bootstrap.exe"),bootstrap);
        if (folder != null) {
            string m=Path.Combine(p,"code_mods",folder);
            Write(Path.Combine(m,"mod.json"),Json.Serialize(new SetupModuleManifest {
                id="audit_fixture",name="Audit fixture",version=version,loader_api=1,
                requires=new string[0],failure_policy="disable_mod"
            }));
            Write(Path.Combine(m,"ncmm_mod.dll"),"fixture-DLL-"+version);
        }
        return p;
    }
    private static void SetBoot(string root) {
        Type t=typeof(NCMMBootstrap); BindingFlags f=BindingFlags.NonPublic|BindingFlags.Static;
        string dir=Path.Combine(root,"ncmm"); Directory.CreateDirectory(dir);
        t.GetField("Root",f).SetValue(null,root);
        t.GetField("NcmmDir",f).SetValue(null,dir);
        t.GetField("LogPath",f).SetValue(null,Path.Combine(dir,"audit-bootstrap.log"));
        t.GetField("HashCache",f).SetValue(null,null);
        t.GetField("HashCacheLoaded",f).SetValue(null,false);
    }
    private static object Boot(string method,params object[] args) {
        return typeof(NCMMBootstrap).GetMethod(method,BindingFlags.NonPublic|BindingFlags.Static).Invoke(null,args);
    }
    private static void Probe(string name,Action test) {
        try { test(); Console.WriteLine("REPRODUCED | " + name); }
        catch (Exception e) { Errors++; Console.WriteLine("HARNESS_ERROR_OR_NOT_REPRODUCED | " + name + " | " + e); }
    }
    private static int Main() {
        Work=Path.Combine(Path.GetTempPath(),"ncmm-boundary-audit-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Work);
        Console.WriteLine("AUDIT_BASELINE=4a83414a1bf4147860b0e3dd49a9aded0c5581d0");
        Console.WriteLine("FIXTURE_ROOT="+Work);
        Probe("rollback deletes live file before detecting missing snapshot",delegate {
            string root=Game("rollback"); string id=Guid.NewGuid().ToString("N");
            string backup=Path.Combine(root,".ncmm-setup-tx-"+id);
            Directory.CreateDirectory(Path.Combine(backup,"snapshot"));
            Write(Path.Combine(backup,"snapshot.json"),Json.Serialize(new SetupSnapshotManifest {
                schema=1,game_root=root,entries=new List<SetupSnapshotEntry> {
                    new SetupSnapshotEntry { relative_path="cataclysm-tiles.exe",existed=true,directory=false }
                }
            }));
            Write(Path.Combine(root,".ncmm-setup.pending.json"),Json.Serialize(new SetupTransactionState {
                schema=1,transaction_id=id,game_root=root,backup_root=backup,phase="modules_installed"
            }));
            bool rejected=false;
            try { SetupCore.RecoverPendingSetupTransaction(root); }
            catch (InvalidOperationException e) { rejected=true; Console.WriteLine("EXPECTED_EXCEPTION="+e.Message); }
            bool exists=File.Exists(Path.Combine(root,"cataclysm-tiles.exe"));
            Console.WriteLine("recoveryRejected="+rejected+" liveExeExists="+exists);
            Require(rejected && !exists,"Expected production rollback to reject missing snapshot after deleting live EXE.");
        });
        Probe("Setup accepts shortened source commit but Bootstrap rejects same Host",delegate {
            string root=Game("commit-prefix"); SetBoot(root);
            string host=Path.Combine(root,"cataclysm-tiles.ncmm.exe"); Write(host,"host-fixture");
            string vanilla=SetupCore.Sha256(Path.Combine(root,"cataclysm-tiles.exe"));
            string hash=SetupCore.Sha256(host),patch=new string('a',64);
            var entry=new SetupCertifiedHostEntry { source_commit=Commit,
                upstream_tag="cdda-experimental-2026-10-01-1040",host_sha256=hash,patch_revision=patch,
                ncmm_version=SetupCore.RuntimeVersion,loader_api=1,
                host_url="https://github.com/Neversalimus/NCMM/releases/download/ncmm-host-audit/cataclysm-tiles.ncmm.exe" };
            var feed=new SetupCertifiedHostFeed {schema=1,loader_api=1,runtime_version=SetupCore.RuntimeVersion,
                patch_revision=patch,hosts=new Dictionary<string,SetupCertifiedHostEntry>{{vanilla,entry}}};
            SetupCertifiedHostEntry found; string reason;
            bool setup=SetupCore.TrySelectCertifiedHost(feed,vanilla,Commit.Substring(0,12),out found,out reason);
            Write(Path.Combine(root,"ncmm","host.binding.json"),Json.Serialize(new SetupHostBinding {
                vanilla_sha256=vanilla,host_sha256=hash,source_commit=Commit,
                patch_revision=patch,ncmm_version=SetupCore.RuntimeVersion,loader_api=1
            }));
            bool full=(bool)Boot("HasValidLocalHost",vanilla,Commit);
            bool shortRef=(bool)Boot("HasValidLocalHost",vanilla,Commit.Substring(0,12));
            Console.WriteLine("setupPrefix="+setup+" bootstrapFull="+full+" bootstrapPrefix="+shortRef);
            Require(setup && full && !shortRef,"Commit identity disagreement not reproduced.");
        });
        Probe("Bootstrap SHA256 cache misses a same-length timestamp-preserving rewrite",delegate {
            string root=Game("hash-cache"); SetBoot(root);
            string path=Path.Combine(root,"probe.bin"); Write(path,"AAAAAA");
            DateTime created=File.GetCreationTimeUtc(path),written=File.GetLastWriteTimeUtc(path);
            string first=(string)Boot("Sha256",path);
            Write(path,"BBBBBB"); File.SetCreationTimeUtc(path,created); File.SetLastWriteTimeUtc(path,written);
            string cached=(string)Boot("Sha256",path),actual=SetupCore.Sha256(path);
            Console.WriteLine("first="+first+" cached="+cached+" actual="+actual);
            Require(cached==first && cached!=actual,"Stale hash cache not reproduced.");
        });
        Probe("same module ID with a renamed directory leaves two active DLLs",delegate {
            string root=Game("rename");
            string p1=Payload("payload-old","FixtureOld","0.1.0","bootstrap-one");
            string p2=Payload("payload-new","FixtureNew","0.2.0","bootstrap-one");
            Require(SetupCore.Install(root,p1).CompletionVerified,"Initial install incomplete.");
            InstallResult result=SetupCore.Install(root,p2);
            bool oldDll=File.Exists(Path.Combine(root,"code_mods","FixtureOld","ncmm_mod.dll"));
            bool newDll=File.Exists(Path.Combine(root,"code_mods","FixtureNew","ncmm_mod.dll"));
            Console.WriteLine("completionVerified="+result.CompletionVerified+" oldDll="+oldDll+" newDll="+newDll);
            Require(result.CompletionVerified && oldDll && newDll,"Renamed directory residue not reproduced.");
        });
        Probe("module copy writes through a destination junction outside game root",delegate {
            string root=Game("junction-game"); string outside=Path.Combine(Work,"junction-outside");
            Directory.CreateDirectory(outside); Write(Path.Combine(outside,"marker.txt"),"outside-original");
            string p=Payload("payload-junction","Fixture","0.1.0","bootstrap-one");
            Write(Path.Combine(p,"code_mods","Fixture","data","marker.txt"),"payload-overwrite");
            string dest=Path.Combine(root,"code_mods","Fixture"); Directory.CreateDirectory(dest);
            string link=Path.Combine(dest,"data");
            var start=new ProcessStartInfo("cmd.exe","/d /c mklink /J \""+link+"\" \""+outside+"\"");
            start.UseShellExecute=false; start.RedirectStandardOutput=true; start.RedirectStandardError=true;
            using (Process process=Process.Start(start)) {
                string output=process.StandardOutput.ReadToEnd()+process.StandardError.ReadToEnd();
                process.WaitForExit(); Console.WriteLine(output.Trim());
                Require(process.ExitCode==0,"Junction fixture setup failed.");
            }
            InstallResult result=SetupCore.Install(root,p);
            string actual=File.ReadAllText(Path.Combine(outside,"marker.txt"));
            Console.WriteLine("completionVerified="+result.CompletionVerified+" outsideMarker="+actual);
            Require(result.CompletionVerified && actual=="payload-overwrite","Junction write was not reproduced.");
        });
        Probe("missing bootstrap receipt during upgrade replaces vanilla backup with old Bootstrap",delegate {
            string root=Game("missing-receipt");
            string p1=Payload("payload-boot-one",null,null,"bootstrap-one");
            string p2=Payload("payload-boot-two",null,null,"bootstrap-two");
            Require(SetupCore.Install(root,p1).CompletionVerified,"First Host-only install failed.");
            File.Delete(Path.Combine(root,"ncmm","bootstrap.sha256"));
            InstallResult result=SetupCore.Install(root,p2);
            string vanilla=File.ReadAllText(Path.Combine(root,"cataclysm-tiles.vanilla.exe"));
            Console.WriteLine("completionVerified="+result.CompletionVerified+" vanillaContents="+vanilla);
            Require(result.CompletionVerified && vanilla=="bootstrap-one","Lost-receipt vanilla misidentification not reproduced.");
        });
        Console.WriteLine("HARNESS_ERRORS="+Errors);
        return Errors==0?0:1;
    }
}
