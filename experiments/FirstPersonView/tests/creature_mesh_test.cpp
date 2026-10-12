#include "creature_mesh.hpp"
#include <cstdio>
#include <cstdlib>
using namespace first_person::creature;
int main(){size_t checks=0;auto check=[&](bool v){++checks;if(!v){std::fprintf(stderr,"mesh check %zu failed\n",checks);std::exit(1);}};
    sprite s;s.width=16;s.height=32;s.pixels.resize(512,0);for(int y=1;y<31;++y)for(int x=3;x<13;++x)s.pixels[y*16+x]=y<8?0xb58265ff:y<20?0x305981ff:0x35332cff;
    builder gen;for(int kind=0;kind<int(anatomy::count);++kind){phenotype p;p.kind=anatomy(kind);p.height=1.7f;p.width=.8f;p.length=1.4f;p.legs=p.kind==anatomy::centipede?16:p.kind==anatomy::arachnid?8:6;
        const auto m=gen.generate(p,s),again=gen.generate(p,s);check(!m.lod[0].faces.empty());check(m.lod[1].faces.size()<m.lod[0].faces.size());check(m.bones.size()>1);check(m.colors.sourced);check(m.colors.fingerprint==again.colors.fingerprint);
        for(size_t j=0;j<m.bones.size();++j)check(m.bones[j].parent<int(j));
        for(const auto &lod:m.lod){for(const auto &v:lod.vertices){check(std::isfinite(v.position.x+v.position.y+v.position.z));check(v.joint<m.bones.size());check(v.u>=0&&v.u<=1&&v.v>=0&&v.v<=1);check(std::abs(dot(v.normal,v.normal)-1)<.001f);}
            for(const auto &f:lod.faces){for(auto v:f.vertices)check(v<lod.vertices.size());const auto &a=lod.vertices[f.vertices[0]],&b=lod.vertices[f.vertices[1]],&c=lod.vertices[f.vertices[2]];const auto n=cross(b.position-a.position,c.position-a.position);if(dot(n,a.normal+b.normal+c.normal)<=0)std::fprintf(stderr,"inverted %s: %u %u %u %g\n",name(p.kind),f.vertices[0],f.vertices[1],f.vertices[2],dot(n,a.normal+b.normal+c.normal));check(dot(n,a.normal+b.normal+c.normal)>0);}
        }
        check(m.bytes()<2*1024*1024);std::printf("%s: %zu / %zu triangles, %zu joints\n",name(p.kind),m.lod[0].faces.size(),m.lod[1].faces.size(),m.bones.size());
    }
    // Ray winding through the centre must lie inside the connected spider body.
    // This catches an actual gap even when every individual part is closed.
    phenotype spider;spider.kind=anatomy::arachnid;spider.height=.65f;spider.width=1.55f;spider.length=1.55f;spider.legs=8;
    const auto connected=gen.generate(spider,s);std::vector<point> origins;
    for(const auto &bone:connected.bones)origins.push_back(bone.origin+(bone.parent<0?point{}:origins[size_t(bone.parent)]));
    for(const auto &lod:connected.lod){int winding=0;const point ray{0,.011f,spider.height*.55f+.009f};
        for(const auto &face:lod.faces){auto vertex=[&](uint32_t id){const auto &v=lod.vertices[id];return v.position+origins[v.joint];};
            const auto a=vertex(face.vertices[0]),b=vertex(face.vertices[1]),c=vertex(face.vertices[2]);
            const auto e1=b-a,e2=c-a,h=cross(point{1,0,0},e2);const float det=dot(e1,h);if(std::abs(det)<1e-7f)continue;
            const auto delta=ray-a;const float u=dot(delta,h)/det;if(u<0||u>1)continue;const auto q=cross(delta,e1);const float v=q.x/det;
            if(v<0||u+v>1||dot(e2,q)/det<=0)continue;winding+=cross(e1,e2).x>0?1:-1;
        }check(winding>0);
    }
    auto malformed=gen.generate(phenotype{},sprite{});check(!malformed.colors.sourced);check(!malformed.lod[0].faces.empty());
    ncmm_graphics_sprite_v2 invalid{};invalid.width=513;check(!sprite::copy(invalid).valid());
    std::printf("Creature mesh: %zu checks PASS\n",checks);
}
