"""Offline CC0 held meshes. Reuses the PSX importer and the runtime's rig format.

Only the pinned selection is accepted. No network, Blender, executable source
content, external GLB buffers, runtime importer, or invented authored clips.
"""
import argparse
import hashlib
import io
import json
import math
import posixpath
from pathlib import Path, PurePosixPath
import shlex
import struct
from import_psx_arms import GLB, hierarchy, multiply, trs, require, finite

IDENTITY = tuple(1.0 if i % 5 == 0 else 0.0 for i in range(16))


def inverse(matrix):
    rows = [[matrix[c*4+r] for c in range(4)]+[float(r == c) for c in range(4)] for r in range(4)]
    for c in range(4):
        p = max(range(c, 4), key=lambda r: abs(rows[r][c]))
        require(abs(rows[p][c]) > 1e-12, 'Singular source transform')
        rows[c], rows[p] = rows[p], rows[c]
        scale = rows[c][c]; rows[c] = [v/scale for v in rows[c]]
        for r in range(4):
            if r != c:
                scale = rows[r][c]; rows[r] = [v-scale*w for v, w in zip(rows[r], rows[c])]
    return tuple(rows[r][c+4] for c in range(4) for r in range(4))


def transform(matrix, value, vector=False):
    return tuple(sum(matrix[c*4+r]*value[c] for c in range(3))+(0 if vector else matrix[12+r]) for r in range(3))


def unit(value):
    length = math.sqrt(sum(v*v for v in value)); require(length > 1e-10, 'Zero normal/direction')
    return tuple(v/length for v in value)


def srgb(value):
    require(math.isfinite(value) and 0 <= value <= 1, 'Invalid source colour')
    return value*12.92 if value <= .0031308 else 1.055*value**(1/2.4)-.055


class Inputs:
    def __init__(self, root, selection):
        self.root = root.resolve(); self.files = selection['files']; self.used = set(); self.textures = {}
        require(selection['schema'] == 1 and 0 < len(selection['models']) <= 40, 'Unexpected selection/version')
        for name, digest in self.files.items():
            require(hashlib.sha256(self.read(name)).hexdigest() == digest, 'Pinned source hash mismatch: '+name)
        self.used.clear()

    def read(self, name):
        p = PurePosixPath(name)
        require(not p.is_absolute() and '..' not in p.parts and '\\' not in name and ':' not in name and name in self.files,
                'Unlisted or escaping source path: '+name)
        resolved = self.root.joinpath(*p.parts).resolve()
        require(resolved.is_relative_to(self.root) and resolved.stat().st_size <= 32*1024*1024, 'Invalid source file')
        self.used.add(name); return resolved.read_bytes()

    def sibling(self, source, name):
        require(not PurePosixPath(name).is_absolute() and ':' not in name and '\\' not in name, 'Invalid relative source reference')
        return posixpath.normpath((PurePosixPath(source).parent/PurePosixPath(name)).as_posix())

    def texture(self, data):
        from PIL import Image
        with Image.open(io.BytesIO(data)) as image:
            require(image.format == 'PNG' and 0 < image.width <= 512 and 0 < image.height <= 512, 'Unbounded/non-PNG texture')
            rgba = image.convert('RGBA').tobytes(); width, height = image.size
        digest = hashlib.sha256(struct.pack('<II', width, height)+rgba).hexdigest()
        self.textures[digest] = (width, height, rgba)
        return digest


def material(color=(1, 1, 1, 1), texture=None, finish=0):
    require(len(color) == 4 and finite(color) and all(0 <= v <= 1 for v in color), 'Invalid material colour')
    return {'color': tuple(srgb(v) for v in color[:3])+(color[3],), 'texture': texture, 'finish': finish}


def read_mtl(inputs, path):
    result = {}; current = None
    for line in inputs.read(path).decode('utf-8-sig').splitlines():
        tokens = shlex.split(line, comments=True, posix=True)
        if not tokens: continue
        key, args = tokens[0], tokens[1:]
        if key == 'newmtl':
            current = {'color': [1, 1, 1, 1], 'texture': None}; result[' '.join(args)] = current
        elif current is not None:
            if key == 'Kd': current['color'][:3] = list(map(float, args))
            elif key == 'd': current['color'][3] = float(args[0])
            elif key == 'Tr': current['color'][3] = 1-float(args[0])
            elif key == 'map_Kd':
                require(args and not args[0].startswith('-'), 'Unsupported MTL texture options')
                current['texture'] = inputs.texture(inputs.read(inputs.sibling(path, ' '.join(args))))
    return {name: material(value['color'], value['texture']) for name, value in result.items()}


def triangulate(polygon, normal):
    # Bounded offline ear clipping preserves the concave trigger guards and
    # handles in the selected OBJ sources. Holes/self-intersections fail closed.
    drop = max(range(3), key=lambda i: abs(normal[i])); axes = [i for i in range(3) if i != drop]
    points = [(p[axes[0]], p[axes[1]]) for p in polygon]
    def cross(a, b, c): return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    area = sum(points[i][0]*points[(i+1)%len(points)][1]-points[(i+1)%len(points)][0]*points[i][1] for i in range(len(points)))
    require(abs(area)>1e-14, 'Degenerate projected OBJ face'); sign = 1 if area>0 else -1
    remaining = list(range(len(points))); triangles = []
    while len(remaining)>3:
        found = False
        for at, b in enumerate(remaining):
            a, c = remaining[at-1], remaining[(at+1)%len(remaining)]
            winding = sign*cross(points[a], points[b], points[c])
            if abs(winding)<=1e-12:
                remaining.pop(at); found=True; break
            if winding<0: continue
            if any(all(sign*cross(points[u],points[v],points[p])>=-1e-12 for u,v in [(a,b),(b,c),(c,a)])
                   for p in remaining if p not in (a,b,c)): continue
            triangles.append((a,b,c));remaining.pop(at);found=True;break
        require(found, 'Non-simple OBJ polygon cannot be triangulated safely')
    if len(remaining)==3 and abs(cross(*(points[p] for p in remaining)))>1e-12:triangles.append(tuple(remaining))
    require(triangles, 'OBJ face has no nondegenerate triangles');return triangles


def read_obj(inputs, path):
    positions, normals, uvs, vertices, indices, materials, material_ids, remap = [], [], [], [], [], [], {}, {}
    library = {}; selected = None; degenerate_faces = 0
    for line in inputs.read(path).decode('utf-8-sig').splitlines():
        line = line.split('#', 1)[0].strip()
        if not line: continue
        key, _, rest = line.partition(' '); args = rest.split()
        if key == 'v': positions.append(tuple(map(float, args[:3])))
        elif key == 'vn': normals.append(tuple(map(float, args)))
        elif key == 'vt': uvs.append(tuple(map(float, args[:2])))
        elif key == 'mtllib': library.update(read_mtl(inputs, inputs.sibling(path, rest.strip())))
        elif key == 'usemtl': selected = rest.strip(); require(selected in library, 'Unknown OBJ material')
        elif key == 'f':
            require(3 <= len(args) <= 64 and selected in library, 'Unsupported polygon/material')
            if selected not in material_ids:
                material_ids[selected] = len(materials); materials.append(library[selected])
            mid = material_ids[selected]
            def index(value, count):
                n = int(value); n = n-1 if n > 0 else count+n
                require(0 <= n < count, 'OBJ index out of range'); return n
            corners = []
            for entry in args:
                fields = entry.split('/'); require(len(fields) <= 3, 'Invalid OBJ corner')
                corners.append((index(fields[0], len(positions)), index(fields[1], len(uvs)) if len(fields)>1 and fields[1] else -1,
                                index(fields[2], len(normals)) if len(fields)>2 and fields[2] else -1))
            polygon = [positions[c[0]] for c in corners]
            def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
            base_normal = None; maximum_area = 0
            for k in range(1, len(polygon)-1):
                n = cross(tuple(polygon[k][c]-polygon[0][c] for c in range(3)),tuple(polygon[k+1][c]-polygon[0][c] for c in range(3)))
                length = math.sqrt(sum(v*v for v in n))
                if length > maximum_area:
                    maximum_area = length; base_normal = tuple(v/length for v in n)
            if base_normal is None:
                # Exact zero-area export residue has no surface to render.
                # Count it explicitly; no nonzero-area face is dropped here.
                degenerate_faces += 1; continue
            for triangle in triangulate(polygon, base_normal):
                for corner in [corners[k] for k in triangle]:
                    p, uv, n = corner; normal = normals[n] if n >= 0 else base_normal
                    tex = (uvs[uv][0], 1-uvs[uv][1]) if uv >= 0 else (0, 0)
                    identity = (corner, mid, normal)
                    if identity not in remap:
                        remap[identity] = len(vertices); vertices.append((positions[p], normal, tex, 0, mid))
                    indices.append(remap[identity])
            require(len(vertices)<=16384 and len(indices)<=98304, 'OBJ exceeds held mesh limits')
    return {'vertices': vertices, 'indices': indices, 'materials': materials,
            'nodes': [{'name': 'Source'}], 'joints': [0], 'inverse_bind': [IDENTITY], 'palettes': [((0, 0, 0, 0), (1, 0, 0, 0))],
            'source_nodes': 0, 'source_clips': 0, 'removed_zero_area_faces': degenerate_faces}


def read_glb(inputs, path):
    glb = GLB(inputs.read(path)); d = glb.document
    require(not d.get('animations'), 'Selected source has clips: extend the existing clip importer instead of discarding them')
    require(len(d.get('skins', [])) <= 1 and 0 < len(d['nodes']) < 128, 'Unsupported source rig')
    nodes = d['nodes']; hierarchy(nodes)
    skinned = bool(d.get('skins'))
    joints = d['skins'][0]['joints'] if skinned else list(range(len(nodes)))
    inverse_bind = glb.accessor(d['skins'][0]['inverseBindMatrices'], 'MAT4') if skinned else [IDENTITY]*len(nodes)
    require(len(joints) == len(inverse_bind), 'Mismatched inverse bind matrices')
    materials = []; adaptations = []
    for source in d.get('materials', []):
        extensions = source.get('extensions', {})
        require(source.get('alphaMode', 'OPAQUE') in ('OPAQUE', 'BLEND') and
                not set(extensions)-{'KHR_materials_transmission'}, 'Unsupported authored material')
        pbr = source.get('pbrMetallicRoughness', {}); texture = None
        if 'baseColorTexture' in pbr:
            tex = pbr['baseColorTexture']; require(tex.get('texCoord', 0) == 0, 'Unsupported UV set')
            image = d['images'][d['textures'][tex['index']]['source']]
            if 'bufferView' in image:
                view = d['bufferViews'][image['bufferView']]; require(view['buffer'] == 0, 'External texture buffer')
                start = view.get('byteOffset', 0); content = glb.binary[start:start+view['byteLength']]
            else: content = inputs.read(inputs.sibling(path, image['uri']))
            texture = inputs.texture(content)
        color = list(pbr.get('baseColorFactor', [1, 1, 1, 1]))
        transmission = extensions.get('KHR_materials_transmission', {}).get('transmissionFactor', 0)
        require(math.isfinite(transmission) and 0 <= transmission <= 1, 'Invalid optical transmission')
        if transmission:
            # The existing NCMM renderer supports tinted alpha glass, not
            # physical refraction. Retain the lens as a separate material and
            # declare this bounded approximation in the generated manifest.
            color[3] *= 1-.82*transmission
            adaptations.append({'source_material': source.get('name', ''), 'source_transmission': transmission,
                                'alpha': color[3], 'policy': 'existing tinted alpha glass; no physical refraction'})
        materials.append(material(color, texture))
    require(materials and len(materials) <= 16, 'Unexpected material count')
    vertices, indices, palettes, palette_map = [], [], [], {}
    for ni, node in enumerate(nodes):
        if 'mesh' not in node: continue
        require((node.get('skin') == 0) == skinned, 'Mixed rigid and skinned source nodes')
        for primitive in d['meshes'][node['mesh']]['primitives']:
            require(primitive.get('mode', 4) == 4 and not primitive.get('targets'), 'Unsupported primitive or morph targets')
            a = primitive['attributes']; positions = glb.accessor(a['POSITION'], 'VEC3'); normals = glb.accessor(a['NORMAL'], 'VEC3')
            uvs = glb.accessor(a['TEXCOORD_0'], 'VEC2') if 'TEXCOORD_0' in a else [(0, 0)]*len(positions)
            weights = glb.accessor(a['WEIGHTS_0'], 'VEC4') if skinned else [(1, 0, 0, 0)]*len(positions)
            joint_indices = glb.accessor(a['JOINTS_0'], 'VEC4') if skinned else [(ni, 0, 0, 0)]*len(positions)
            require(not any(name.endswith('_1') and name.startswith(('JOINTS', 'WEIGHTS')) for name in a), 'Additional influences must not be dropped')
            require(len({len(v) for v in [positions, normals, uvs, weights, joint_indices]}) == 1, 'Attribute counts disagree')
            first = len(vertices); mid = primitive.get('material', 0); require(0 <= mid < len(materials), 'Material index out of range')
            for p, n, uv, js, ws in zip(positions, normals, uvs, joint_indices, weights):
                require(abs(sum(ws)-1) < 2e-5 and all(0 <= w <= 1 for w in ws) and
                        all(int(j) == j and 0 <= j < len(joints) for j in js), 'Invalid source influences')
                key = (tuple(map(int, js)), tuple(ws))
                if key not in palette_map: palette_map[key] = len(palettes); palettes.append(key)
                vertices.append((p, n, uv, palette_map[key], mid))
            source_indices = [v[0] for v in glb.accessor(primitive['indices'], 'SCALAR')] if 'indices' in primitive else list(range(len(positions)))
            require(len(source_indices)%3 == 0 and all(int(i) == i and 0 <= i < len(positions) for i in source_indices), 'Invalid source indices')
            indices.extend(first+int(i) for i in source_indices)
    return {'vertices': vertices, 'indices': indices, 'materials': materials, 'nodes': nodes, 'joints': joints,
            'inverse_bind': inverse_bind, 'palettes': palettes, 'source_nodes': len(nodes), 'source_clips': 0,
            'material_adaptations': adaptations}


def prepare(inputs, profile):
    mating = profile.get('mount_rotation', [0, 0, 0, 1])
    require(isinstance(mating, list) and len(mating) == 4 and finite(mating) and
            abs(sum(v*v for v in mating)-1) < 1e-5, 'Invalid rigid mating rotation')
    path = profile['source']; data = read_glb(inputs, path) if path.endswith('.glb') else read_obj(inputs, path)
    radii = profile.get('grip_radii', [.016, .016])
    require(isinstance(radii, list) and len(radii) == 2 and finite(radii) and
            all(.004 <= r <= .047 for r in radii), 'Invalid physical grip radii')
    sections = profile.get('handle_sections', [])
    if sections:
        # These toy-proportioned static OBJ handles would become 10-12 cm
        # thick when the complete tool is scaled to Native's real length.
        # Bake the declared smooth radial correction offline. Keep originals,
        # topology, UVs, author materials and the full-sized tool head.
        require(path.endswith('.obj') and len(data['nodes']) == 1 and profile['length_axis'] == 1,
                'Handle correction requires an unrigged upright OBJ')
        require(isinstance(sections, list) and 2 <= len(sections) <= 16 and
                all(isinstance(row, list) and len(row) == 3 and finite(row) and
                    .1 <= row[1] <= 1 and .1 <= row[2] <= 1 for row in sections) and
                all(a[0] < b[0] for a, b in zip(sections, sections[1:])), 'Invalid handle sections')
        def fit(p, n):
            a = b = sections[0] if p[1] < sections[0][0] else sections[-1]
            for lo, hi in zip(sections, sections[1:]):
                if lo[0] <= p[1] <= hi[0]:a, b = lo, hi;break
            t = (p[1]-a[0])/(b[0]-a[0]) if a is not b else 0
            sx, sz = a[1]+(b[1]-a[1])*t, a[2]+(b[2]-a[2])*t
            dx, dz = ((b[1]-a[1])/(b[0]-a[0]), (b[2]-a[2])/(b[0]-a[0])) if a is not b else (0, 0)
            normal = unit((n[0]/sx, n[1]-dx*p[0]*n[0]/sx-dz*p[2]*n[2]/sz, n[2]/sz))
            return (p[0]*sx, p[1], p[2]*sz), normal
        data['vertices'] = [(*fit(p, n), uv, palette, material) for p, n, uv, palette, material in data['vertices']]
    vs, ids, nodes = data['vertices'], data['indices'], data['nodes']
    require(0 < len(vs) <= 16384 and 0 < len(ids) <= 98304 and len(data['palettes']) <= 1024, 'Mesh exceeds shared bounded rig contract')
    require(all(finite((*v[0], *v[1], *v[2])) and all(abs(uv) <= 1000000 for uv in v[2]) for v in vs), 'Invalid position, normal or UV')
    require(all(not data['materials'][v[4]]['texture'] or all(-1e-6 <= uv <= 1+1e-6 for uv in v[2]) for v in vs),
            'Textured source requires wrapping outside the existing clamp sampler')
    parents, order = hierarchy(nodes); globals_ = [None]*len(nodes)
    for i in order:
        node = nodes[i]; scale = node.get('scale', [1, 1, 1])
        require(min(scale)>0 and max(scale)-min(scale) < 1e-5, 'Non-uniform source scale needs a normal-matrix extension')
        local = trs(node.get('translation', [0, 0, 0]), node.get('rotation', [0, 0, 0, 1]), scale)
        globals_[i] = multiply(globals_[parents[i]], local) if parents[i] >= 0 else local
    # Bind-pose positions are measured through the complete authored skin,
    # including every weight, inverse bind and mesh-node transform.
    bind_skin = [multiply(globals_[j], m) for j, m in zip(data['joints'], data['inverse_bind'])]
    world_points = []
    for p, n, uv, palette, mid in vs:
        js, ws = data['palettes'][palette]
        world_points.append(tuple(sum(transform(bind_skin[js[k]], p)[r]*ws[k] for k in range(4)) for r in range(3)))
    axis = profile['length_axis']; extent = max(p[axis] for p in world_points)-min(p[axis] for p in world_points)
    require(extent > .0001 and .03 <= profile['nominal_length'] <= 3.5, 'Invalid physical model dimensions')
    scale = profile['nominal_length']/extent; rotation = profile.get('rotation', [0, 0, 0, 1])
    root = trs([0, 0, 0], rotation, [scale]*3)
    names = {n.get('name', str(i)): i for i, n in enumerate(nodes)}
    require(len(names) == len(nodes), 'Duplicate authored node names')
    default_node = names.get('Body', 0)
    contacts = {}
    for name in ['primary', 'secondary', 'trigger', 'muzzle', 'sight', 'pivot']:
        spec = profile['contacts'].get(name, profile['contacts']['primary'])
        if isinstance(spec, list):
            require(len(spec) == 3 and finite(spec), 'Invalid contact point: '+name)
            node = default_node; local = transform(inverse(globals_[node]), spec)
        else:
            require(isinstance(spec, dict) and isinstance(spec.get('node'), str), 'Invalid contact node: '+name)
            require(spec['node'] in names, 'Missing contact node: '+spec['node']); node = names[spec['node']]
            point = spec.get('world', spec.get('point', [0, 0, 0]))
            require(isinstance(point, list) and len(point) == 3 and finite(point), 'Invalid contact point: '+name)
            local = transform(inverse(globals_[node]), point) if 'world' in spec else tuple(point)
        contacts[name] = (node, local)
    primary_node, primary_point = contacts['primary']; primary = transform(root, transform(globals_[primary_node], primary_point))
    root = list(root); root[12:15] = [-v for v in primary]; root = tuple(root)
    root_index = len(nodes); root_t = root[12:15]
    data['nodes'] = [dict(n, parent=parents[i] if parents[i] >= 0 else root_index) for i, n in enumerate(nodes)]+[
        {'name': 'NCMM_units_axes_grip', 'parent': -1, 'translation': root_t, 'rotation': rotation, 'scale': [scale]*3}]
    data['order'] = [root_index]+order
    magazine = names.get('Magazine', -1); round_node = names.get('Shell', -1)
    def below(node, ancestor):
        if ancestor < 0: return False
        while node >= 0:
            if node == ancestor: return True
            node = parents[node]
        return False
    parts = []
    for at in range(0, len(ids), 3):
        flags = set()
        for index in ids[at:at+3]:
            js, ws = data['palettes'][vs[index][3]]
            for joint, weight in zip(js, ws):
                if weight:
                    node = data['joints'][joint]
                    flags.add(1 if below(node, magazine) else 2 if below(node, round_node) else 0)
        require(len(flags) == 1 or flags == {0}, 'Mixed detachable-part influences need explicit author partitioning')
        parts.append(next(iter(flags)))
    data.update(profile=profile, contacts=contacts, triangle_part=parts, root_matrix=root,
                sockets=[names.get(n, -1) for n in ['Attach_Scope', 'Attach_Muzzle', 'Attach_Rail.Bottom', 'Attach_Rail.SideRight']],
                trigger_node=names.get('Trigger', -1), slide_node=names.get(profile.get('slide_node', ''), -1), slide_translation=[0, 0, 0])
    if data['slide_node'] >= 0:
        parent = parents[data['slide_node']]; parent_matrix = globals_[parent] if parent >= 0 else IDENTITY
        data['slide_translation'] = transform(inverse(parent_matrix), [0, 0, .022/scale], True)
    return data


def number(value):
    require(math.isfinite(value), 'Cannot emit NaN/Inf')
    text = format(value, '.9g'); return text+('.0' if '.' not in text and 'e' not in text else '')+'f'


def aggregate(values): return '{'+','.join(number(v) for v in values)+'}'


def emit_model(data, identity):
    p = data['profile']; symbol = p['id']; out = ['// Generated offline; do not edit. CC0 source/provenance: ../selection.json',
        '#include "held_item.hpp"', 'namespace first_person::held {']
    for m in data['materials']:
        if m['texture']:
            digest = m['texture']; w, h, rgba = data['textures'][digest]
            out.append(f'extern const uint32_t texture_{digest[:24]}[{w*h}];')
    def array(kind, name, rows): out.extend([f'static const {kind} {name}[]={{', *[r+',' for r in rows], '};'])
    array('viewmodel::vertex', 'vertices', ['{'+aggregate(v[0])+','+aggregate(v[1])+','+number(v[2][0])+','+number(v[2][1])+f',{v[3]},0,0'+'}' for v in data['vertices']])
    array('uint32_t', 'indices', [','.join(map(str, data['indices'][i:i+24])) for i in range(0, len(data['indices']), 24)])
    array('uint16_t', 'vertex_material', [','.join(str(v[4]) for v in data['vertices'][i:i+32]) for i in range(0, len(data['vertices']), 32)])
    array('part', 'triangle_part', [','.join('part::'+['body', 'magazine', 'round'][v] for v in data['triangle_part'][i:i+24]) for i in range(0, len(data['triangle_part']), 24)])
    array('viewmodel::node', 'nodes', ['{'+json.dumps(n.get('name', str(i)))+f',{n["parent"]},'+'{'+aggregate(n.get('translation', [0, 0, 0]))+','+aggregate(n.get('rotation', [0, 0, 0, 1]))+','+aggregate(n.get('scale', [1, 1, 1]))+'}}' for i, n in enumerate(data['nodes'])])
    array('uint16_t', 'order', [','.join(map(str, data['order']))]); array('uint16_t', 'joints', [','.join(map(str, data['joints']))])
    array('viewmodel::matrix', 'inverse_bind', ['{'+aggregate(m)+'}' for m in data['inverse_bind']])
    array('viewmodel::weights', 'palettes', ['{{'+','.join(map(str, js))+'},'+aggregate(ws)+'}' for js, ws in data['palettes']])
    mats = []
    for m in data['materials']:
        digest = m['texture']; w, h = data['textures'][digest][:2] if digest else (0, 0)
        texture = 'texture_'+digest[:24] if digest else 'nullptr'; tid = int(digest[:16], 16) if digest else 0
        mats.append('{'+aggregate(m['color'])+f',{m["finish"]},{texture},{w},{h},{tid}ull'+'}')
    array('material', 'materials', mats)
    contacts = ['{'+str(data['contacts'][n][0])+','+aggregate(data['contacts'][n][1])+'}' for n in ['primary', 'secondary', 'trigger', 'muzzle', 'sight', 'pivot']]
    for name in ['primary_axis', 'secondary_axis']:
        contacts.append(aggregate(unit(transform(data['root_matrix'], p.get(name, [0, 1, 0]), True))))
    if 'grip_radii' in p:contacts.extend(number(v) for v in p['grip_radii'])
    out.extend(['static const model value={',f'1,{json.dumps(symbol)},{identity}ull,family::{p["family"]},',
        '{vertices,std::size(vertices),indices,std::size(indices),nodes,order,std::size(nodes),joints,inverse_bind,std::size(joints),palettes,std::size(palettes),nullptr,0,nullptr,0,nullptr,0,nullptr,nullptr,0,0},',
        'vertex_material,triangle_part,materials,std::size(materials),{'+','.join(contacts)+'},',
        ','.join(map(str, data['sockets']+[data['trigger_node'], data['slide_node']]))+','+aggregate(data['slide_translation'])+','+number(p['nominal_length'])+','+str(p['two_handed']).lower()+
        (','+aggregate(p['mount_rotation']) if 'mount_rotation' in p else ''),
        '};', f'const model &asset_{symbol}() noexcept {{return value;}}', '}'])
    return ('\n'.join(out)+'\n').encode('utf8')


def convert(selection_path, output, check=False):
    selection = json.loads(selection_path.read_text(encoding='utf-8')); inputs = Inputs(selection_path.parent, selection)
    generated = {}; summaries = []; identities = set(); symbols = []
    for profile in selection['models']:
        require(profile['id'].replace('_', '').isalnum() and profile['id'][0].isalpha(), 'Unsafe generated symbol')
        require(profile['family'] in {'pistol','revolver','smg','assault','battle','pump','auto_shotgun','knife','machete','bat','axe','hatchet','sledge','spear','hammer','crowbar','shovel','pickaxe','saw','wrench','screwdriver','pliers','bottle','gas_can','medical','flashlight','optic','suppressor','foregrip','gun_light'}, 'Unknown held family')
        require(isinstance(profile['two_handed'], bool), 'Handedness must be an explicit boolean')
        require(profile['id'] not in symbols, 'Duplicate asset symbol')
        inputs.used.clear()
        try: data = prepare(inputs, profile)
        except ValueError as error: raise ValueError(profile['id']+': '+str(error)) from error
        data['textures'] = inputs.textures
        sources = sorted((name, inputs.files[name]) for name in inputs.used)
        digest = hashlib.sha256(json.dumps([profile, sources], sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        identity = int(digest[:16], 16); require(identity and identity not in identities, 'Duplicate model identity'); identities.add(identity)
        name = profile['id']+'.generated.cpp'; generated[name] = emit_model(data, identity); symbols.append(profile['id'])
        summaries.append({'id': profile['id'], 'identity': identity, 'vertices': len(data['vertices']), 'triangles': len(data['indices'])//3,
                          'source_nodes': data['source_nodes'], 'retained_nodes': len(data['nodes'])-1, 'palettes': len(data['palettes']),
                          'source_clips': data['source_clips'], 'materials': len(data['materials']), 'nominal_length_m': profile['nominal_length'],
                          'removed_zero_area_faces': data.get('removed_zero_area_faces', 0),
                          'material_adaptations': data.get('material_adaptations', []),
                          'normalization_matrix': data['root_matrix'], 'generated': name, 'sha256': hashlib.sha256(generated[name]).hexdigest()})
    textures = ['// Generated offline from pinned PNG pixels.', '#include <cstdint>', 'namespace first_person::held {']
    for digest, (width, height, rgba) in sorted(inputs.textures.items()):
        words = [int.from_bytes(rgba[i:i+4], 'big') for i in range(0, len(rgba), 4)]
        textures.append(f'extern const uint32_t texture_{digest[:24]}[{len(words)}]={{')
        textures.extend(','.join(f'0x{v:08x}u' for v in words[i:i+16])+',' for i in range(0, len(words), 16)); textures.append('};')
    generated['textures.generated.cpp'] = ('\n'.join(textures+['}'])+'\n').encode()
    registry = ['// Generated asset inventory; no item-name matching.', '#include "held_item.hpp"', 'namespace first_person::held {']
    registry += [f'const model &asset_{s}() noexcept;' for s in symbols]
    registry += ['const model *const *models(size_t &count) noexcept {', 'static const model *const values[]={'+','.join('&asset_'+s+'()' for s in symbols)+'};', 'count=std::size(values);return values;}', '}']
    generated['inventory.generated.cpp'] = ('\n'.join(registry)+'\n').encode()
    manifest = {'schema': 1, 'converter': 'import_held_items.py/v1', 'selection_sha256': hashlib.sha256(selection_path.read_bytes()).hexdigest(),
                'models': summaries, 'files': {n: hashlib.sha256(v).hexdigest() for n, v in sorted(generated.items())},
                'runtime_importer': False, 'authored_clip_count': 0, 'unique_textures': len(inputs.textures)}
    generated['manifest.json'] = (json.dumps(manifest, indent=2)+'\n').encode()
    output = output.resolve()
    if check:
        require(output.is_dir() and {p.name for p in output.iterdir() if p.is_file()} == set(generated), 'Generated file inventory differs')
        require(all((output/n).read_bytes() == v for n, v in generated.items()), 'Generated assets differ from pinned offline import')
    else:
        output.mkdir(parents=True, exist_ok=True)
        for name, content in generated.items(): (output/name).write_bytes(content)
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--selection', type=Path, required=True); parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--check', action='store_true'); args = parser.parse_args()
    result = convert(args.selection, args.output, args.check)
    print(json.dumps({'success': True, 'models': len(result['models']), 'unique_textures': result['unique_textures'], 'check': args.check}))
