"""Visual QA of the real C++ command stream, using an existing tile atlas.

This is a deterministic test room, not a CDDA gameplay screenshot. No tileset
assets are redistributed; pass the local tileset path as an explicit argument.
"""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw

def render(commands, tileset_root, destination):
    config = json.loads((tileset_root / "tile_config.json").read_text())
    info = config["tile_info"][0]
    sprites, definitions = {}, {}
    offset = 0
    for part in config["tiles-new"]:
        sheet = Image.open(tileset_root / part["file"]).convert("RGBA")
        sw, sh = part.get("sprite_width", info["width"]), part.get("sprite_height", info["height"])
        cols, rows = sheet.width // sw, sheet.height // sh
        for i in range(cols * rows):
            sprites[offset+i] = (sheet, (i % cols * sw, i // cols * sh, (i % cols+1)*sw, (i // cols+1)*sh))
        offset += cols * rows
        for tile in part.get("tiles", []):
            ids = tile["id"] if isinstance(tile["id"], list) else [tile["id"]]
            for identity in ids: definitions[identity] = tile
    def texture(identity):
        value = definitions[identity]["fg"]
        while isinstance(value, list): value = value[0]
        if isinstance(value, dict): value = value["sprite"]
        while isinstance(value, list): value = value[0]
        sheet, box = sprites[value]
        return sheet.crop(box)
    selected = {"t_floor": texture("t_floor"), "t_wall": texture("t_wall"),
                "f_chair": texture("f_chair"), "mon_zombie": texture("mon_zombie"), "bottle_plastic": texture("bottle_plastic")}
    canvas = Image.new("RGBA", (960, 540), (11, 16, 24, 255))
    for x0,y0,x1,y1,u0,v0,u1,v1,rgba,index,layer in commands:
        x0,y0,x1,y1 = round(x0),round(y0),round(x1),round(y1)
        if x1<=x0 or y1<=y0: continue
        color = (rgba>>24, (rgba>>16)&255, (rgba>>8)&255, rgba&255)
        if layer==0:
            ImageDraw.Draw(canvas).rectangle((x0,y0,x1-1,y1-1),fill=color)
            continue
        if layer==1:
            gx,gy=index%9,index//9
            source=selected["t_wall" if gx in (0,8) or gy in (0,8) else "t_floor"]
        else: source=selected[{2:"f_chair",3:"bottle_plastic",4:"mon_zombie"}[layer]]
        ix,iy=int(u0*source.width),int(v0*source.height)
        ex,ey=max(ix+1,int(u1*source.width)),max(iy+1,int(v1*source.height))
        fragment=source.crop((ix,iy,ex,ey)).resize((x1-x0,y1-y0),Image.Resampling.NEAREST)
        tint=Image.new("RGBA",fragment.size,color)
        from PIL import ImageChops
        fragment=ImageChops.multiply(fragment,tint)
        canvas.alpha_composite(fragment,(x0,y0))
    destination.parent.mkdir(parents=True,exist_ok=True)
    canvas.convert("RGB").save(destination)

if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("commands",type=Path)
    parser.add_argument("tileset_root",type=Path)
    parser.add_argument("output",type=Path)
    args=parser.parse_args()
    render(json.loads(args.commands.read_text()),args.tileset_root,args.output)
