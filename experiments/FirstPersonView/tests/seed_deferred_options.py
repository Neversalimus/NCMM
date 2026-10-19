"""Seed four nondefault serialized values in a fresh qualification game only."""
import argparse
import json
from pathlib import Path

def seed(game):
    path=Path(game)/'config/options.json'
    existing=json.loads(path.read_text(encoding='utf-8-sig')) if path.exists() else []
    fixture=json.loads((Path(__file__).with_name('deferred_options_fixture.json')).read_text(encoding='utf-8'))
    assert isinstance(existing,list) and len(fixture)==4
    names={v['name'] for v in fixture}
    values=[v for v in existing if v['name'] not in names]+fixture
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(values,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--game',type=Path,required=True)
    seed(parser.parse_args().game)
