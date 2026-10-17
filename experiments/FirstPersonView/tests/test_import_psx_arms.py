"""Independent malformed glTF fixtures for the offline importer (stdlib only)."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

SPEC=importlib.util.spec_from_file_location('psx_import',Path(__file__).resolve().parents[1]/'tools/import_psx_arms.py')
IMPORT=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(IMPORT)

def fixture(change=None):
    data=bytearray();views=[];accessors=[]
    def array(kind,component,rows):
        while len(data)%4:data.append(0)
        offset=len(data);layout=struct.Struct('<'+IMPORT.FORMATS[component]*IMPORT.COMPONENTS[kind])
        for row in rows:data.extend(layout.pack(*row))
        views.append({'buffer':0,'byteOffset':offset,'byteLength':len(data)-offset})
        accessors.append({'bufferView':len(views)-1,'componentType':component,'count':len(rows),'type':kind})
        return len(accessors)-1
    attrs={
        'POSITION':array('VEC3',5126,[(-.2,0,0),(-.1,0,0),(-.1,.1,0),(.1,0,0),(.2,0,0),(.1,.1,0)]),
        'NORMAL':array('VEC3',5126,[(0,0,1)]*6),
        'TEXCOORD_0':array('VEC2',5126,[(0,0),(1,0),(0,1)]*2),
        'JOINTS_0':array('VEC4',5123,[(0,1,0,0)]*3+[(2,3,0,0)]*3),
        'WEIGHTS_0':array('VEC4',5126,[(.75,.25,0,0)]*6)}
    indices=array('SCALAR',5123,[(i,) for i in range(6)])
    inverse=array('MAT4',5126,[tuple(1 if i%5==0 else 0 for i in range(16))]*4)
    times=array('SCALAR',5126,[(0,),(1,)])
    rotations=array('VEC4',5126,[(0,0,0,1),(0,0,.70710678,.70710678)])
    document={'asset':{'version':'2.0'},'buffers':[{'byteLength':len(data)}],
              'bufferViews':views,'accessors':accessors,'meshes':[{'primitives':[{'attributes':attrs,'indices':indices}]}],
              'skins':[{'joints':[0,1,2,3],'inverseBindMatrices':inverse}],
              'nodes':[{'name':'hand.R'},{'name':'forearm.R','children':[0]},
                       {'name':'hand.L'},{'name':'forearm.L','children':[2]},
                       {'name':'root','children':[1,3,5]},{'name':'ArmsMesh','mesh':0,'skin':0}],
              'animations':[{'name':'relax','samplers':[{'input':times,'output':rotations}],
                             'channels':[{'sampler':0,'target':{'node':0,'path':'rotation'}}]}]}
    if change:change(document,data,attrs)
    encoded=json.dumps(document,separators=(',',':')).encode()
    encoded+=b' '*((-len(encoded))%4);data+=b'\0'*((-len(data))%4)
    return struct.pack('<4sII',b'glTF',2,12+8+len(encoded)+8+len(data))+struct.pack('<II',len(encoded),0x4E4F534A)+encoded+struct.pack('<II',len(data),0x004E4942)+data

def set_first(document,data,attribute,values):
    accessor=document['accessors'][attribute]
    view=document['bufferViews'][accessor['bufferView']]
    struct.pack_into('<'+IMPORT.FORMATS[accessor['componentType']]*len(values),data,view['byteOffset'],*values)

class ImportTests(unittest.TestCase):
    def rejected(self,change):
        with self.assertRaises((ValueError,KeyError,IndexError,struct.error)):
            IMPORT.import_model(fixture(change))
    def test_valid_two_weight_fixture(self):
        value=IMPORT.import_model(fixture())
        self.assertEqual(value['palettes'],[((0,1,0,0),(.75,.25,0.0,0.0)),((2,3,0,0),(.75,.25,0.0,0.0))])
        self.assertEqual(len(value['indices']),6)
        self.assertEqual(value['bind_max_error'],0)
    def test_truncated_glb(self):
        with self.assertRaises(ValueError):IMPORT.GLB(fixture()[:-1])
    def test_unknown_required_extension(self):
        self.rejected(lambda d,b,a:d.update(extensionsRequired=['unknown_skinning']))
    def test_external_buffer(self):
        self.rejected(lambda d,b,a:d['buffers'][0].update(uri='https://example.invalid/model.bin'))
    def test_accessor_cannot_escape_its_view(self):
        self.rejected(lambda d,b,a:d['accessors'][a['POSITION']].update(byteOffset=4))
    def test_sparse_not_silently_ignored(self):
        self.rejected(lambda d,b,a:d['accessors'][a['POSITION']].update(sparse={}))
    def test_nan_position(self):
        self.rejected(lambda d,b,a:set_first(d,b,a['POSITION'],(float('nan'),0,0)))
    def test_uv_bounds(self):
        self.rejected(lambda d,b,a:set_first(d,b,a['TEXCOORD_0'],(1.01,0)))
    def test_weight_sum(self):
        self.rejected(lambda d,b,a:set_first(d,b,a['WEIGHTS_0'],(.25,.25,0,0)))
    def test_negative_weight(self):
        self.rejected(lambda d,b,a:set_first(d,b,a['WEIGHTS_0'],(1.1,-.1,0,0)))
    def test_joint_range(self):
        self.rejected(lambda d,b,a:set_first(d,b,a['JOINTS_0'],(4,1,0,0)))
    def test_additional_weights_not_silently_dropped(self):
        self.rejected(lambda d,b,a:a.update(WEIGHTS_1=a['WEIGHTS_0']))
    def test_cross_hand_weights(self):
        self.rejected(lambda d,b,a:set_first(d,b,a['JOINTS_0'],(0,2,0,0)))
    def test_bind_pose_mismatch(self):
        self.rejected(lambda d,b,a:d['nodes'][0].update(translation=[.1,0,0]))
    def test_hierarchy_cycle(self):
        with self.assertRaises(ValueError):IMPORT.hierarchy([{'children':[1]},{'children':[0]}])
    def test_multiple_parents(self):
        self.rejected(lambda d,b,a:d['nodes'][3]['children'].append(0))
    def test_unknown_interpolation(self):
        self.rejected(lambda d,b,a:d['animations'][0]['samplers'][0].update(interpolation='CUBICSPLINE'))
    def test_duplicate_channel(self):
        self.rejected(lambda d,b,a:d['animations'][0]['channels'].append(copy.deepcopy(d['animations'][0]['channels'][0])))
    def test_nonmonotonic_keys(self):
        self.rejected(lambda d,b,a:set_first(d,b,d['animations'][0]['samplers'][0]['input'],(1,)))
    def test_emission_deterministic(self):
        value=IMPORT.import_model(fixture())
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);IMPORT.emit(value,root);first=(root/'model.generated.cpp').read_bytes()
            self.assertNotIn(b'\r',first)
            IMPORT.emit(value,root);self.assertEqual(first,(root/'model.generated.cpp').read_bytes())

if __name__=='__main__':unittest.main()
