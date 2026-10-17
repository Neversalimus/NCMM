// Postmortem only: reads one owned native diagnostic dump; never attaches to a live process.
#include <windows.h>
#include <dbgeng.h>
#include <cstdio>
#include <cstdlib>
#pragma comment(lib,"dbgeng.lib")
struct output:IDebugOutputCallbacks {ULONG references=1;HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void **out)override{if(id==__uuidof(IUnknown)||id==__uuidof(IDebugOutputCallbacks)){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}ULONG STDMETHODCALLTYPE AddRef()override{return ++references;}ULONG STDMETHODCALLTYPE Release()override{return --references;}HRESULT STDMETHODCALLTYPE Output(ULONG mask,PCSTR text)override{fputs(text,stdout);return S_OK;}};
int main(int argc,char **argv){if(argc<2)return 2;IDebugClient *client=nullptr;IDebugControl *control=nullptr;IDebugSystemObjects *systems=nullptr;output callback;
 if(FAILED(DebugCreate(__uuidof(IDebugClient),(void **)&client)))return 3;client->SetOutputCallbacks(&callback);client->QueryInterface(__uuidof(IDebugControl),(void **)&control);client->QueryInterface(__uuidof(IDebugSystemObjects),(void **)&systems);
 IDebugSymbols *symbols=nullptr;client->QueryInterface(__uuidof(IDebugSymbols),(void **)&symbols);if(argc>3)symbols->SetImagePath(argv[3]);symbols->SetSymbolPath("");
 HRESULT result=client->OpenDumpFile(argv[1]);printf("OpenDumpFile=%lx\n",result);if(FAILED(result))return 4;result=control->WaitForEvent(0,10000);printf("WaitForEvent=%lx\n",result);
 if(argc>2){ULONG id=0;result=systems->GetThreadIdBySystemId((ULONG)strtoul(argv[2],nullptr,10),&id);printf("Thread lookup=%lx engine_id=%lu\n",result,id);if(SUCCEEDED(result))systems->SetCurrentThreadId(id);}
 control->Execute(DEBUG_OUTCTL_ALL_CLIENTS,"r; kb; ~* kb; lm",DEBUG_EXECUTE_DEFAULT);client->EndSession(DEBUG_END_PASSIVE);return 0;}
