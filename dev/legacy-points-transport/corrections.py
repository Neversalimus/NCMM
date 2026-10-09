import json
from pathlib import Path
root=Path('candidate')
p=root/'compat/compatibility.manifest.json'
d=json.loads(p.read_text(encoding='utf-8-sig'))
a=d['self_test']['optional_modules']
assert a.count('legacy_character_points')==1, 'Expected one initial module record'
a[a.index('legacy_character_points')]={'id':'legacy_character_points','version':'0.1.0'}
p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
p=root/'mods/LegacyCharacterPoints/CMakeLists.txt'
s=p.read_text();old='target_compile_options(${t} PRIVATE /EHsc /utf-8)'
assert s.count(old)==1
s=s.replace(old,'# Exported locale callbacks report failure to the Host exception boundary.\n    target_compile_options(${t} PRIVATE /EHsc- /utf-8)')
p.write_text(s,encoding='utf-8',newline='\n')
p=root/'mods/LegacyCharacterPoints/tests/module_test.cpp'
s=p.read_text(encoding='utf-8');old='    api=setup();require(desc->init(&api)==1,"reinitialize");locale="ru";ncmm_on_locale_changed_v1(&api);'
assert s.count(old)==1
new='''    for(int n=1;n<=total;++n){
        api=setup();require(desc->init(&api)==1,"locale fixture init");
        fail_at=calls+n;bool reported=false;
        try{ncmm_on_locale_changed_v1(&api);}catch(...){reported=true;}
        require(reported,"locale registration failure reaches Host boundary");
        desc->shutdown();
    }
'''+old
s=s.replace(old,new);p.write_text(s,encoding='utf-8',newline='\n')
print('Versioned catalog and explicit locale-failure boundary applied; all checks remain enabled.')
