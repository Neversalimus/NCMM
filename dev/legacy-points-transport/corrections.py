import json
from pathlib import Path
root=Path('candidate')
p=root/'compat/compatibility.manifest.json'
d=json.loads(p.read_text(encoding='utf-8-sig'))
a=d['self_test']['optional_modules']
assert a.count('legacy_character_points')==1, 'Expected one initial module record'
a[a.index('legacy_character_points')]={'id':'legacy_character_points','version':'0.1.0'}
p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print('Compatibility catalog record corrected; complete catalog checks remain enabled.')
