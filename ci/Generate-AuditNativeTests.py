#!/usr/bin/env python3
"""Compile exact production bodies against narrow mocks; not an engine gameplay test."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('--output', required=True); a=p.parse_args()
r=Path(__file__).resolve().parents[1]; out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
def block(s, signature):
    start=s.index(signature); opening=s.index('{',start); depth=0;state='';escaped=False;i=opening
    while i<len(s):
        c=s[i];pair=s[i:i+2]
        if state=='//':
            if c=='\n':state=''
        elif state=='/*':
            if pair=='*/':state='';i+=1
        elif state:
            if escaped: escaped=False
            elif c=='\\':escaped=True
            elif c==state:state=''
        elif pair in ('//','/*'):state=pair;i+=1
        elif c in ('"',"'"):state=c
        elif c=='{':depth+=1
        elif c=='}':
            depth-=1
            if depth==0:return s[start:i+1]
        i+=1
    raise RuntimeError('Unclosed production block: '+signature)
loader=(r/'host_patch/ncmm_loader.cpp').read_text(encoding='utf-8-sig')
# A signature includes the definition's newline/brace to avoid forward declarations.
names=[('loaded_mod','struct loaded_mod {'),('quarantine','void quarantine_runtime_callback( loaded_mod &mod, runtime_callback_kind kind,\n'),('suspend','bool suspend_state_migration( loaded_mod &mod, const char *reason )\n{'),('ensure','bool ensure_state_migrated( loaded_mod &mod )\n{'),('dispatch','void dispatch_event_v2( uint32_t event_id )\n{')]
parts={k:block(loader,n) for k,n in names}
head=r'''
#include "ncmm_api.h"
#include "ncmm_fault_policy.h"
#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
using ncmm::runtime_callback_kind;
using ncmm::runtime_fault_policy;
#ifdef _WIN32
using HMODULE = void *;
#endif
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)
'''
stubs=r'''
std::vector<loaded_mod> loaded;
std::set<std::string> module_ids;
struct module_state { std::filesystem::path directory; std::string state,lifecycle,reason; };
std::vector<module_state> module_states;
struct ncmm_event_subscription_v2_internal { std::string module_id; uint32_t event_id; void(*callback)(uint32_t,void*); void *user_data; };
std::vector<ncmm_event_subscription_v2_internal> event_subscriptions_v2;
struct binding { std::string module_id; };
std::map<std::string,binding> worldgen_bindings_v2,runtime_setting_bindings_v2;
std::map<std::string,int64_t> schemas;
std::map<std::string,int> modifiers;
std::string owner;
struct module_call_scope { std::string previous; explicit module_call_scope(const char *id):previous(owner){owner=id?id:"";} ~module_call_scope(){owner=previous;} };
bool has_character=true;
int writes=0,migrations=0,calls=0;
ncmm_host_api_v1 api{};
void log_line(int,const char*){}
void write_modules_state(){++writes;}
void erase_module_modifiers(const std::string &id){modifiers.erase(id);}
bool character_state_available(){return has_character;}
int64_t character_state_get_i64(const char*id,const char*,int64_t){return schemas[id];}
bool event_available_v2(uint32_t){return true;}
loaded_mod *find_loaded_by_id(const char *id){for(auto &m:loaded)if(std::string(m.descriptor->id)==id)return &m;return nullptr;}
int migrate_ok(const ncmm_host_api_v1*,uint32_t,uint32_t to){++migrations;schemas[owner]=to;return 1;}
int migrate_fail(const ncmm_host_api_v1*,uint32_t,uint32_t){++migrations;return 0;}
void count_callback(uint32_t,void*){++calls;modifiers[owner]=5;}
void throw_callback(uint32_t,void*){++calls;modifiers[owner]=5;throw std::runtime_error("callback");}
'''
main=r'''
int main(){
 ncmm_mod_descriptor_v1 d{}; d.id="survivor_progression";
 loaded_mod m{};m.descriptor=&d;m.directory="SurvivorProgression";m.state_schema=8;m.migrate_state=&migrate_ok;
 loaded.push_back(m);module_ids.insert(d.id);module_states.push_back({m.directory,"loaded","active","ok"});
 schemas[d.id]=999;modifiers[d.id]=5;
 event_subscriptions_v2.push_back({d.id,NCMM_EVENT_TURN_V2,&count_callback,nullptr});
 dispatch_event_v2(NCMM_EVENT_TURN_V2);CHECK(calls==0 && modifiers.empty() && loaded[0].migration_suspended);
 int first_writes=writes;dispatch_event_v2(NCMM_EVENT_TURN_V2);CHECK(writes==first_writes);
 schemas[d.id]=7;dispatch_event_v2(NCMM_EVENT_TURN_V2);CHECK(calls==1 && migrations==1 && schemas[d.id]==8);
 event_subscriptions_v2.push_back({d.id,NCMM_EVENT_WORLD_LOADED_V2,&count_callback,nullptr});
 dispatch_event_v2(NCMM_EVENT_WORLD_LOADED_V2);int announced_calls=calls;dispatch_event_v2(NCMM_EVENT_WORLD_LOADED_V2);CHECK(calls==announced_calls);
 loaded[0].migration_ready=false;loaded[0].migration_suspended=false;loaded[0].migrate_state=&migrate_fail;schemas[d.id]=6;
 dispatch_event_v2(NCMM_EVENT_TURN_V2);int failed_migrations=migrations;dispatch_event_v2(NCMM_EVENT_TURN_V2);CHECK(migrations==failed_migrations && calls==announced_calls);
 loaded[0].migrate_state=&migrate_ok;schemas[d.id]=5;dispatch_event_v2(NCMM_EVENT_TURN_V2);CHECK(migrations==failed_migrations+1);
 worldgen_bindings_v2["world"]={d.id};runtime_setting_bindings_v2["setting"]={d.id};
 event_subscriptions_v2.clear();event_subscriptions_v2.push_back({d.id,NCMM_EVENT_TURN_V2,&throw_callback,nullptr});
 event_subscriptions_v2.push_back({d.id,NCMM_EVENT_TURN_V2,&count_callback,nullptr});
 int before=calls;dispatch_event_v2(NCMM_EVENT_TURN_V2);
 CHECK(calls==before+1 && modifiers.empty() && loaded[0].fault.modifiers_quarantined && event_subscriptions_v2.empty());
 CHECK(worldgen_bindings_v2.empty() && runtime_setting_bindings_v2.empty());
 CHECK(!ensure_state_migrated(loaded[0]));
 CHECK(loaded[0].on_turn==nullptr && loaded[0].locale_changed==nullptr && loaded[0].open_ui==nullptr && loaded[0].item_activate==nullptr);
 std::cout<<"Audit Host production event/migration/quarantine bodies: PASS\n";
}
'''
(out/'audit_host_boundaries.cpp').write_text(head+parts['loaded_mod']+';\n'+stubs+parts['quarantine']+'\n'+parts['suspend']+'\n'+parts['ensure']+'\n'+parts['dispatch']+'\n'+main, encoding='utf-8', newline='\n')
patch=(r/'host_patch/Apply-NCMMHostPatch.ps1').read_text(encoding='utf-8-sig')
prob=block(patch,'double dispersion_sources::probability_below( double threshold ) const')
head=r'''
#include "ncmm_checked_math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <chrono>
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)
struct dispersion_sources {std::vector<double> linear_sources,normal_sources,multipliers; double probability_below(double threshold) const;};
'''
main=r'''
int main(){
 dispersion_sources d;d.normal_sources={1000};d.linear_sources={0.1};
 CHECK(std::abs(d.probability_below(500)-0.4999202115449836)<1e-8);
 d.linear_sources.clear();CHECK(std::abs(d.probability_below(500)-.5)<1e-12);
 CHECK(d.probability_below(0)==0);CHECK(d.probability_below(1000)<1);CHECK(d.probability_below(1000.001)==1);
 for(int n=1;n<=8;++n){d.normal_sources={100};d.linear_sources.assign(n,10);CHECK(std::abs(d.probability_below(50+n*5)-.5)<1e-7);}
 d.linear_sources.assign(9,10);CHECK(d.probability_below(60)<0);
 d.normal_sources={1,1};CHECK(d.probability_below(1)<0);
 d.normal_sources={std::numeric_limits<double>::quiet_NaN()};CHECK(d.probability_below(1)<0);
 using namespace ncmm::checked;const int64_t m=std::numeric_limits<int64_t>::max();int64_t carry=m;
 CHECK(add(m,1)==m && add(-1,-1)==0 && multiply(m,2)==m);
 CHECK(percent(m,100,carry)==m && carry>=0 && carry<100);
 carry=0;CHECK(percent(m,1,carry)==m/100 && carry==m%100);
 carry=0;CHECK(percent(1,m,carry)==m/100 && carry==m%100);
 carry=99;CHECK(percent(0,m,carry)==0 && carry==99);
 #ifndef _MSC_VER
 uint64_t rng=0xabc321u;
 for(int i=0;i<20000;++i){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;int64_t a=rng>>1;
 rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;int64_t b=rng>>1;int64_t c=i%100,rem=c;
 __int128 exact=static_cast<__int128>(a)*b+c;int64_t expected=exact/100>m?m:static_cast<int64_t>(exact/100);
 CHECK(percent(a,b,rem)==expected && rem==exact%100);}
 #endif
 auto start=std::chrono::steady_clock::now();d.normal_sources={100};d.linear_sources.assign(8,10);
 for(int i=0;i<1000;++i)CHECK(d.probability_below(73)>=0);
 CHECK(std::chrono::steady_clock::now()-start<std::chrono::seconds(10));
 std::cout<<"Audit actual BHC body and checked arithmetic: PASS\n";
}
'''
(out/'audit_numeric.cpp').write_text(head+prob+'\n'+main, encoding='utf-8', newline='\n')
print('Generated test sources from exact production bodies in',out)
head=r'''
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <iostream>
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)
bool russian=false;
namespace ncmm { std::string localized_text(const char*a,const char*b){return russian?b:a;} }
int utf8_width(const std::string&s,bool=false){int n=0;for(unsigned char c:s)if((c&0xc0)!=0x80)++n;return n;}
std::string utf8_truncate(const std::string&s,int length){size_t at=0;int n=0;for(;at<s.size();++at)if((static_cast<unsigned char>(s[at])&0xc0)!=0x80){if(n==length)break;++n;}return s.substr(0,at);}
struct point{int x,y;point(int a,int b):x(a),y(b){}};
std::vector<std::string> printed;
void trim_and_print(int,point,int width,int,const std::string &s){CHECK(utf8_width(s)<=width);printed.push_back(s);}
int main(){int w=0,content_x=2,focus=3;bool zone_present[12],selected_covers[12];int encumbrance[12];
 std::fill_n(zone_present,12,true);std::fill_n(selected_covers,12,true);
 auto enc_color=[](int z){return z;};
 for(bool language:{false,true}){russian=language;
'''
labels=block(patch,'const std::string short_labels[] = {')+';\n'
row=block(patch,'const auto enc_row = [&]( int row, int first, int second, int third )')+';\n'
body=r'''
 for(int content_width=33;content_width<=100;++content_width)
 for(int equipment_body_map_focus=-1;equipment_body_map_focus<12;++equipment_body_map_focus)
 for(int value:{std::numeric_limits<int>::min(),0,9,99,100,999,std::numeric_limits<int>::max()}){
 std::fill_n(encumbrance,12,value);printed.clear();
'''
tail=r'''
 enc_row(0,0,1,2);enc_row(1,3,4,5);enc_row(2,6,7,-1);enc_row(3,8,9,-1);enc_row(4,10,11,-1);
 CHECK(printed.size()==12);for(const auto&s:printed){std::string expected=std::to_string(value);CHECK(s.size()>=expected.size() && s.substr(s.size()-expected.size())==expected);}
 }}std::cout<<"Audit EBM actual readout: PASS (RU/EN, widths33..100, all focus zones and int bounds)\n";
}
'''
(out/'audit_equipment_layout.cpp').write_text(head+labels+body+row+tail, encoding='utf-8', newline='\n')
