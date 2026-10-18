"""Small hostile/mathematical fixtures; never native game qualification."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from import_held_items import Inputs, IDENTITY, convert, inverse, prepare, read_obj, transform, triangulate


def glb_fixture(clips=False, extra_weights=False):
    binary=bytearray(); views=[]; accessors=[]
    def array(kind, component, values):
        while len(binary)%4:binary.append(0)
        start=len(binary);fmt={5123:'H',5125:'I',5126:'f'}[component]
        for row in values:binary.extend(struct.pack('<'+fmt*len(row),*row))
        views.append(dict(buffer=0,byteOffset=start,byteLength=len(binary)-start))
        accessors.append(dict(bufferView=len(views)-1,componentType=component,type=kind,count=len(values)))
        return len(accessors)-1
    attrs={'POSITION':array('VEC3',5126,[(0,0,0),(1,0,0),(0,1,0)]),
           'NORMAL':array('VEC3',5126,[(0,0,1)]*3),
           'TEXCOORD_0':array('VEC2',5126,[(-.25,0),(1,0),(0,1)]),
           'JOINTS_0':array('VEC4',5123,[(0,1,0,0)]*3),
           'WEIGHTS_0':array('VEC4',5126,[(.25,.75,0,0)]*3)}
    if extra_weights:attrs['WEIGHTS_1']=attrs['WEIGHTS_0']
    indices=array('SCALAR',5125,[(0,),(1,),(2,)])
    first=list(IDENTITY);first[12]=.1
    second=list(first);second[13]=.2
    matrices=array('MAT4',5126,[inverse(tuple(first)),inverse(tuple(second))])
    document=dict(asset=dict(version='2.0'),buffers=[dict(byteLength=len(binary))],bufferViews=views,accessors=accessors,
                  nodes=[dict(name='Body',translation=[.1,0,0],children=[1]),dict(name='Tip',translation=[0,.2,0]),dict(name='Mesh',mesh=0,skin=0)],
                  skins=[dict(joints=[0,1],inverseBindMatrices=matrices)],
                  meshes=[dict(primitives=[dict(attributes=attrs,indices=indices,material=0)])],
                  materials=[dict(pbrMetallicRoughness=dict(baseColorFactor=[.2,.3,.4,1]))])
    if clips:document['animations']=[dict(name='AuthorClip',samplers=[],channels=[])]
    packed=json.dumps(document,separators=(',',':')).encode();packed+=b' '*(-len(packed)%4)
    binary+=b'\0'*(-len(binary)%4)
    return b'glTF'+struct.pack('<II',2,12+8+len(packed)+8+len(binary))+struct.pack('<I4s',len(packed),b'JSON')+packed+struct.pack('<I4s',len(binary),b'BIN\0')+binary


class HeldImportTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)

    def inputs(self, files, profile=None):
        hashes={}
        for name,data in files.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);hashes[name]=hashlib.sha256(data).hexdigest()
        selection=dict(schema=1,files=hashes,models=[profile or {}])
        return Inputs(self.root,selection),selection

    def profile(self):
        return dict(id='test_mesh',source='mesh.glb',family='pistol',length_axis=0,nominal_length=.5,rotation=[0,1,0,0],
                    contacts=dict(primary=[0,0,0],secondary=[0,.1,0],trigger=[.05,0,0],muzzle=[1,0,0],sight=[1,.1,0],pivot=[0,0,0]),two_handed=False)

    def test_concave_guard_preserves_open_area(self):
        polygon=[(0,0,0),(3,0,0),(3,3,0),(2,3,0),(2,1,0),(1,1,0),(1,3,0),(0,3,0)]
        triangles=triangulate(polygon,(0,0,1))
        area=0
        for a,b,c in triangles:
            p,q,r=polygon[a],polygon[b],polygon[c]
            area+=abs((q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0]))/2
        self.assertEqual(area,7)
        self.assertEqual(len(triangles),6)

    def test_near_collinear_export_vertices_do_not_choose_wrong_plane(self):
        polygon=[(.015589,.067166,-.00224),(.017735,.070782,-.00224),(.019882,.074398,-.00224),
                 (.019882,.074398,.004812),(.017735,.070782,.004812),(.015589,.067166,.004812)]
        obj='mtllib mesh.mtl\n'+''.join('v '+' '.join(map(str,p))+'\n' for p in polygon)+'usemtl flat\nf 1 2 3 4 5 6\n'
        inputs,_=self.inputs({'mesh.obj':obj.encode(),'mesh.mtl':b'newmtl flat\nKd .2 .3 .4\n'})
        mesh=read_obj(inputs,'mesh.obj');self.assertGreater(len(mesh['indices']),0)
        self.assertEqual(mesh['removed_zero_area_faces'],0)

    def test_all_weights_normals_and_unused_uv_survive(self):
        inputs,_=self.inputs({'mesh.glb':glb_fixture()});mesh=prepare(inputs,self.profile())
        self.assertEqual(mesh['palettes'],[((0,1,0,0),(.25,.75,0,0))])
        self.assertEqual(mesh['vertices'][0][1],(0.,0.,1.))
        self.assertEqual(mesh['vertices'][0][2],(-.25,0.))
        self.assertEqual(mesh['source_nodes'],3);self.assertEqual(len(mesh['nodes']),4)
        self.assertEqual(mesh['order'][0],3)
        self.assertEqual(mesh['source_clips'],0)
        p=transform(mesh['root_matrix'],(0,0,0))
        self.assertLess(max(abs(v) for v in p),1e-7)

    def test_extra_influences_cannot_be_silently_dropped(self):
        inputs,_=self.inputs({'mesh.glb':glb_fixture(extra_weights=True)})
        with self.assertRaisesRegex(ValueError,'influences'):prepare(inputs,self.profile())

    def test_unknown_authored_clips_are_not_discarded(self):
        inputs,_=self.inputs({'mesh.glb':glb_fixture(clips=True)})
        with self.assertRaisesRegex(ValueError,'clips'):prepare(inputs,self.profile())

    def test_source_hash_and_path_are_checked_before_output(self):
        _,selection=self.inputs({'mesh.glb':glb_fixture()},self.profile())
        (self.root/'mesh.glb').write_bytes(b'tampered')
        path=self.root/'selection.json';path.write_text(json.dumps(selection))
        with self.assertRaisesRegex(ValueError,'hash mismatch'):convert(path,self.root/'out')
        self.assertFalse((self.root/'out').exists())
        with self.assertRaises(ValueError):Inputs(self.root,dict(schema=1,files={'../secret':'0'*64},models=[{}]))

    def test_output_is_deterministic_and_check_rejects_tampering(self):
        _,selection=self.inputs({'mesh.glb':glb_fixture()},self.profile())
        path=self.root/'selection.json';path.write_text(json.dumps(selection))
        first=convert(path,self.root/'out');second=convert(path,self.root/'out',True)
        self.assertEqual(first,second)
        (self.root/'out/test_mesh.generated.cpp').write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError,'differ'):convert(path,self.root/'out',True)


if __name__=='__main__':unittest.main()
