// Opt-in debugger for one freshly launched diagnostic child. x64 Windows only.
// A first-chance AV is retained even if CDDA's fatal-error handler exits directly.
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
public sealed class NCMMNativeDebugger : IDisposable {
    [StructLayout(LayoutKind.Explicit, Size=176)] struct DebugEvent {
        [FieldOffset(0)] public uint Code;
        [FieldOffset(4)] public uint ProcessId;
        [FieldOffset(8)] public uint ThreadId;
        [FieldOffset(16)] public uint ExceptionCode;
        [FieldOffset(32)] public IntPtr ExceptionAddress;
        [FieldOffset(168)] public uint FirstChance;
        [FieldOffset(16)] public IntPtr FileHandle;
        [FieldOffset(40)] public IntPtr ImageBase;
        [FieldOffset(24)] public IntPtr DllBase;
        [FieldOffset(16)] public uint ExitCode;
        [FieldOffset(16)] public IntPtr DebugString;
        [FieldOffset(24)] public ushort DebugStringUnicode;
        [FieldOffset(26)] public ushort DebugStringLength;
    }
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool DebugActiveProcess(uint pid);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool DebugActiveProcessStop(uint pid);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool DebugSetProcessKillOnExit(bool kill);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool WaitForDebugEvent(out DebugEvent ev,uint timeout);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool ContinueDebugEvent(uint pid,uint tid,uint status);
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] bytes,UIntPtr size,out UIntPtr read);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern uint GetFinalPathNameByHandle(IntPtr handle,System.Text.StringBuilder path,uint count,uint flags);
    [DllImport("Dbghelp.dll", SetLastError=true)] static extern bool MiniDumpWriteDump(IntPtr process,uint pid,IntPtr file,uint type,IntPtr exception,IntPtr user,IntPtr callback);
    readonly uint pid; readonly string directory; readonly Thread thread;
    readonly ManualResetEvent initialized=new ManualResetEvent(false);
    volatile bool stop; public volatile string Error;
    public volatile int Dumps;
    NCMMNativeDebugger(uint child,string output) { pid=child;directory=output; thread=new Thread(Run);thread.IsBackground=true;thread.Start(); }
    public static NCMMNativeDebugger Start(uint child,string output) {
        if(IntPtr.Size!=8)throw new PlatformNotSupportedException("Native debug-event layout requires x64.");
        var session=new NCMMNativeDebugger(child,output);
        if(!session.initialized.WaitOne(15000)){session.Dispose();throw new Exception("Diagnostic debugger attach timed out.");}
        if(session.Error!=null){session.Dispose();throw new Exception(session.Error);}
        return session;
    }
    void Log(string text) { File.AppendAllText(Path.Combine(directory,"first-person-debug-events.log"),DateTime.UtcNow.ToString("o")+" "+text+Environment.NewLine); }
    void Dump(DebugEvent ev) {
        Log(String.Format("exception=0x{0:X8} address=0x{1:X} thread={2} first_chance={3}",ev.ExceptionCode,ev.ExceptionAddress.ToInt64(),ev.ThreadId,ev.FirstChance));
        if(Dumps>=3)return;
        string name=String.Format("first-person-crash-{0}-{1:X8}-{2}.dmp",pid,ev.ExceptionCode,Dumps);
        IntPtr handle=OpenProcess(0x410,false,pid);
        try {
            if(handle==IntPtr.Zero)throw new Exception("OpenProcess error "+Marshal.GetLastWin32Error());
            using(var file=new FileStream(Path.Combine(directory,name),FileMode.CreateNew,FileAccess.Write,FileShare.Read)) {
                // Thread contexts, stacks, unloaded modules and referenced memory;
                // do not copy the entire world/heap into a multi-gigabyte dump.
                if(!MiniDumpWriteDump(handle,pid,file.SafeFileHandle.DangerousGetHandle(),0x1060,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero))
                    throw new Exception("MiniDumpWriteDump error "+Marshal.GetLastWin32Error());
            }
            ++Dumps;Log("dump="+name);
        } finally {if(handle!=IntPtr.Zero)CloseHandle(handle);}
    }
    int debugStrings;
    void DebugString(DebugEvent ev) {
        if(ev.DebugStringLength==0)return;
        int length=Math.Min(16384,ev.DebugStringLength*(ev.DebugStringUnicode!=0?2:1));byte[] bytes=new byte[length];UIntPtr read;
        IntPtr handle=OpenProcess(0x410,false,pid);
        try {if(handle!=IntPtr.Zero&&ReadProcessMemory(handle,ev.DebugString,bytes,(UIntPtr)length,out read)) {
            string text=(ev.DebugStringUnicode!=0?System.Text.Encoding.Unicode:System.Text.Encoding.Default).GetString(bytes,0,(int)read.ToUInt64()).TrimEnd('\0');
            if(debugStrings++<1000||text.IndexOf("ERROR",StringComparison.OrdinalIgnoreCase)>=0||text.IndexOf("Assertion",StringComparison.OrdinalIgnoreCase)>=0)
                Log("debug_output="+text);
        }}
        finally {if(handle!=IntPtr.Zero)CloseHandle(handle);}
    }
    void Module(DebugEvent ev) {
        var name=new System.Text.StringBuilder(32768);
        if(ev.FileHandle!=IntPtr.Zero&&ev.FileHandle!=new IntPtr(-1)) {
            GetFinalPathNameByHandle(ev.FileHandle,name,(uint)name.Capacity,0);CloseHandle(ev.FileHandle);
        }
        Log(String.Format("module=0x{0:X} path={1}",(ev.Code==3?ev.ImageBase:ev.DllBase).ToInt64(),name));
    }
    void Run() {
        bool attached=false,loaderBreakpoint=true;
        try {
            if(!DebugActiveProcess(pid))throw new Exception("DebugActiveProcess error "+Marshal.GetLastWin32Error());
            attached=true;
            if(!DebugSetProcessKillOnExit(false))throw new Exception("DebugSetProcessKillOnExit error "+Marshal.GetLastWin32Error());
            Log("attached child="+pid);initialized.Set();
            while(!stop) {
                DebugEvent ev;
                if(!WaitForDebugEvent(out ev,100)) {int error=Marshal.GetLastWin32Error();if(error!=121&&error!=0)throw new Exception("WaitForDebugEvent error "+error);continue;}
                uint status=0x00010002;bool exited=ev.Code==5;
                try {
                    if(ev.Code==1) {
                        if(loaderBreakpoint&&ev.ExceptionCode==0x80000003)loaderBreakpoint=false;
                        else {status=0x80010001;if(ev.FirstChance==0||ev.ExceptionCode==0xC0000005||ev.ExceptionCode==0xC0000374||ev.ExceptionCode==0xC000001D)Dump(ev);}
                    } else if(ev.Code==3||ev.Code==6)Module(ev);
                    else if(ev.Code==8)DebugString(ev);
                    else if(exited)Log(String.Format("exit=0x{0:X8}",ev.ExitCode));
                } finally {if(!ContinueDebugEvent(ev.ProcessId,ev.ThreadId,status))throw new Exception("ContinueDebugEvent error "+Marshal.GetLastWin32Error());}
                if(exited){attached=false;break;}
            }
        } catch(Exception error) {Error=error.ToString();try{Log("debugger_error="+Error);}catch{} }
        finally { initialized.Set();if(attached)DebugActiveProcessStop(pid); }
    }
    public void Dispose() {stop=true;if(Thread.CurrentThread!=thread)thread.Join(5000);}
}
