#!/usr/bin/env python3
"""Reproducible, offline import of the two pinned CC0 component meshes.

No complete vehicle, animation, collision, or runtime file loader is imported.
Native dimensions remain authoritative. Run with --check in qualification CI.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1] / "assets" / "vehicles"
LIMIT = 8192


def digest(data):
    return hashlib.sha256(data).hexdigest()


def number(value):
    value = float(value)
    if not math.isfinite(value) or abs(value) > 16:
        raise ValueError("unbounded/nonfinite mesh coordinate")
    return value


def parse_obj(text, texture):
    positions, normals, uvs, faces = [], [], [], []
    for line in text.splitlines():
        fields = line.split()
        if not fields or fields[0].startswith("#"):
            continue
        kind = fields[0]
        if kind in ("v", "vn", "vt"):
            count = 2 if kind == "vt" else 3
            if len(fields) < count + 1:
                raise ValueError("short coordinate")
            target = uvs if kind == "vt" else normals if kind == "vn" else positions
            target.append(tuple(number(x) for x in fields[1:count + 1]))
            if len(target) > LIMIT:
                raise ValueError("mesh exceeds vertex limit")
        elif kind == "f":
            if len(fields) not in (4, 5) or len(faces) >= LIMIT:
                raise ValueError("only bounded triangle/quad faces are admitted")
            refs = []
            for field in fields[1:]:
                parts = field.split("/")
                if len(parts) != 3:
                    raise ValueError("position/UV/normal required")
                values = tuple(int(x) for x in parts)
                if any(x <= 0 or x > len(table) for x, table in zip(values, (positions, uvs, normals))):
                    raise ValueError("face index outside already owned arrays")
                refs.append(tuple(x - 1 for x in values))
            for i in range(1, len(refs) - 1):
                faces.append((refs[0], refs[i], refs[i + 1]))
        elif kind not in ("mtllib", "g", "o", "s", "usemtl"):
            raise ValueError("unrecognized OBJ operation: " + kind)
    if not positions or not faces:
        raise ValueError("empty component")
    low = [min(p[n] for p in positions) for n in range(3)]
    high = [max(p[n] for p in positions) for n in range(3)]
    center = [(a + b) / 2 for a, b in zip(low, high)]
    # Source axle X -> Native local Y; radial Z/Y -> Native local X/Z.
    # The cyclic permutation preserves winding. Normalize axle width to 1,
    # radial radius to 1; apply inverse-transpose to normals at both stages.
    width = high[0] - low[0]
    radius = max(high[n] - low[n] for n in (1, 2)) / 2
    if not .01 < width < 4 or not .01 < radius < 4:
        raise ValueError("implausible component bounds")
    result = []
    for face in faces:
        uv = [sum(uvs[r[1]][n] for r in face) / 3 for n in range(2)]
        if any(v < 0 or v > 1 for v in uv):
            raise ValueError("palette UV outside unit square")
        color = texture.getpixel((min(texture.width-1, int(uv[0]*texture.width)),
                                  min(texture.height-1, int((1-uv[1])*texture.height))))
        vertices = []
        for v, _, n in face:
            p, normal = positions[v], normals[n]
            pos = ((p[2]-center[2])/radius, (p[0]-center[0])/width, (p[1]-center[1])/radius)
            norm = (normal[2]*radius, normal[0]*width, normal[1]*radius)
            length = math.sqrt(sum(x*x for x in norm))
            if length < 1e-8:
                raise ValueError("zero normal")
            vertices.append((pos, tuple(x/length for x in norm)))
        result.append((vertices, tuple(c/255 for c in color)))
    return result, {"source_min": low, "source_max": high, "triangles": len(result),
                    "normalized_radius": 1, "normalized_axle_width": 1,
                    "palette_colors": len(set(c for _, c in result))}


def literal(value):
    text = format(0 if abs(value) < 1e-8 else value, ".8g")
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


def produce(root=ROOT):
    selection_bytes = (root / "selection.json").read_bytes()
    selection = json.loads(selection_bytes)
    if selection["license"] != "CC0-1.0" or len(selection["models"]) != 2:
        raise ValueError("unexpected component selection")
    for name, expected in selection["files"].items():
        path = Path(name)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError("unsafe selection path")
        data = (root / "source" / path).read_bytes()
        if len(data) > 256*1024 or digest(data) != expected:
            raise ValueError("source differs from pinned selection: " + name)
    with Image.open(root / "source/Textures/colormap.png") as image:
        if image.width > 1024 or image.height > 1024:
            raise ValueError("palette exceeds bound")
        texture = image.convert("RGBA")
    header = '''// Generated offline; see source/License.txt (Kenney, CC0).
#pragma once
#include <cstddef>
namespace first_person::vehicle_assets {
enum class id { road_wheel, truck_wheel };
struct vertex {float position[3],normal[3];};
struct triangle {vertex vertices[3];float color[4];};
struct mesh {const triangle *triangles;std::size_t count;};
mesh get(id component);
}
'''
    cpp = ['// Generated offline; Kenney Car Kit 3.1, CC0.\n#include "vehicle_components.generated.hpp"\nnamespace first_person::vehicle_assets {\n']
    report = {"schema": 1, "source_selection_sha256": digest(selection_bytes), "models": []}
    for item in selection["models"]:
        if item["id"] not in ("road_wheel", "truck_wheel"):
            raise ValueError("unexpected component identifier")
        triangles, stats = parse_obj((root / "source" / item["source"]).read_text(), texture)
        cpp.append("static const triangle " + item["id"] + "[]={\n")
        for vertices, color in triangles:
            cpp.append("{{" + ",".join("{{" + ",".join(map(literal, p)) + "},{" + ",".join(map(literal, n)) + "}}" for p, n in vertices)
                       + "},{" + ",".join(map(literal, color)) + "}},\n")
        cpp.append("};\n")
        report["models"].append(dict(item, **stats))
    cpp.append('mesh get(id value){switch(value){\n')
    for item in selection["models"]:
        name = item["id"]
        cpp.append(f'case id::{name}:return{{{name},sizeof({name})/sizeof(triangle)}};\n')
    cpp.append('}return{nullptr,0};}\n}\n')
    outputs = {"vehicle_components.generated.hpp": header.encode(), "vehicle_components.generated.cpp": "".join(cpp).encode()}
    report["files"] = {name: digest(body) for name, body in outputs.items()}
    outputs["manifest.json"] = (json.dumps(report, indent=2) + "\n").encode()
    return outputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    outputs = produce()
    destination = ROOT / "generated"
    if not args.check:
        destination.mkdir(exist_ok=True)
    for name, body in outputs.items():
        path = destination / name
        if args.check:
            if not path.is_file() or path.read_bytes() != body:
                raise SystemExit("stale generated vehicle component: " + name)
        else:
            path.write_bytes(body)
    print("Verified" if args.check else "Imported", "two independent CC0 wheels; source selection and output hashes pinned")


if __name__ == "__main__":
    main()
