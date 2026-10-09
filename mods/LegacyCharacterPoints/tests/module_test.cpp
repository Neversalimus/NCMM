#include "ncmm_api.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
extern "C" const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1();
extern "C" void ncmm_on_locale_changed_v1(const ncmm_host_api_v1 *);
namespace {
ncmm_host_api_v2_core core{};
std::string missing,locale="en";
int calls=0, fail_at=0, bindings=0;
std::map<std::string,int> defaults;
std::string mode_default,label;
int cap(const char *s) {return s && missing!=s;}
uint32_t major(){return NCMM_API_VERSION_MAJOR;}
uint32_t minor(){return NCMM_API_VERSION_MINOR;}
const char *get_locale(){return locale.c_str();}
const void *query(const char *id,uint32_t major,uint32_t) {
    return id && std::strcmp(id,NCMM_HOST_API_V2_CORE_ID)==0 && major==2 ? &core : nullptr;
}
int ok(){return ++calls!=fail_at;}
int boolreg(const char*,const char *key,const char *name,const char*,int val,uint32_t){
    if(!ok())return 0;
    defaults[key]=val;label=name;return 1;
}
int intreg(const char*,const char *key,const char*,const char*,int lo,int hi,int val,uint32_t){
    if(lo!=0 || hi!=1000 || !ok())return 0;
    defaults[key]=val;return 1;
}
int enumreg(const char*,const char*,const char*,const char*,const char *const *ids,
            const char *const *,size_t count,const char *val,uint32_t){
    if(count!=4 || std::string(ids[1])!="multi_pool" || !ok())return 0;
    mode_default=val;return 1;
}
int bind(const char*,const char*,const char*,uint32_t){if(!ok())return 0;++bindings;return 1;}
int boolget(const char*,int f){return f;}
int64_t intget(const char*,int64_t f){return f;}
void require(bool ok,const char *m){if(!ok){std::cerr<<m<<'\n';std::exit(1);}}
ncmm_host_api_v1 setup(){
    core={};core.struct_size=sizeof(core);core.abi_version=NCMM_HOST_API_V2_CORE_ABI;core.api_major=2;core.api_minor=3;
    core.world_setting_register_bool=&boolreg;core.world_setting_register_int=&intreg;
    core.world_setting_register_enum=&enumreg;core.runtime_hook_bind_setting=&bind;
    core.runtime_hook_bool=&boolget;core.runtime_hook_i64=&intget;
    ncmm_host_api_v1 api{};api.abi_version=NCMM_ABI_VERSION;api.has_capability=&cap;
    api.get_api_version_major=&major;api.get_api_version_minor=&minor;api.get_locale=&get_locale;api.query_interface=&query;
    calls=fail_at=bindings=0;missing.clear();defaults.clear();mode_default.clear();locale="en";
    return api;
}
}
int main(){
    const auto *desc=ncmm_get_descriptor_v1();
    require(desc && std::string(desc->id)=="legacy_character_points","descriptor");
    auto api=setup();require(desc->init(nullptr)==0,"null api");
    for(size_t i=0;i<desc->required_capability_count;++i){
        api=setup();missing=desc->required_capabilities[i];
        require(desc->init(&api)==0 && calls==0,"missing capability fails before writes");
    }
    api=setup();core.struct_size=offsetof(ncmm_host_api_v2_core,runtime_hook_i64);
    require(desc->init(&api)==0 && calls==0,"truncated ABI tail");
    api=setup();core.api_major=99;require(desc->init(&api)==0,"wrong core version");
    api=setup();core.world_setting_register_enum=nullptr;require(desc->init(&api)==0,"missing enum callback");
    api=setup();core.runtime_hook_bind_setting=nullptr;require(desc->init(&api)==0,"missing binding callback");
    api=setup();require(desc->init(&api)==1,"real init");
    require(defaults.size()==5 && bindings==5 && mode_default=="any","settings and binding counts");
    require(defaults["NCMM_LCP_STAT_POINTS"]==6 && defaults["NCMM_LCP_TRAIT_POINTS"]==0 &&
        defaults["NCMM_LCP_SKILL_POINTS"]==2 && defaults["NCMM_LCP_TRAIT_CAP"]==12,"classic defaults");
    const int total=calls;
    for(int n=1;n<=total;++n){api=setup();fail_at=n;require(desc->init(&api)==0,"registration failure");desc->shutdown();}
    for(int n=1;n<=total;++n){
        api=setup();require(desc->init(&api)==1,"locale fixture init");
        fail_at=calls+n;bool reported=false;
        try{ncmm_on_locale_changed_v1(&api);}catch(...){reported=true;}
        require(reported,"locale registration failure reaches Host boundary");
        desc->shutdown();
    }
    api=setup();require(desc->init(&api)==1,"reinitialize");locale="ru";ncmm_on_locale_changed_v1(&api);
    require(label=="Классические очки персонажа","Russian registration");
    desc->shutdown();const int before=calls;ncmm_on_locale_changed_v1(&api);require(before==calls,"shutdown is inert");
    std::cout<<"Legacy Character Points real module init/ABI/locale/failure lifecycle: PASS\n";
}
