#include "creature_appearance_debug.hpp"
#include "creature_lod.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace first_person::creature {
// Explicit qualification output only. Gameplay never reads files, performs
// external requests or enables this exporter automatically.
static bool debug_bitmap(const std::filesystem::path &path,uint32_t width,uint32_t height,const std::vector<uint32_t> &pixels){
    if(!width||!height||pixels.size()!=size_t(width)*height)return false;
    std::ofstream out(path,std::ios::binary);if(!out)return false;
    auto word=[&](uint32_t value,int bytes){for(int i=0;i<bytes;++i)out.put(char(value>>(i*8)));};
    const uint32_t pitch=(width*3+3)&~3u;
    out.put('B');out.put('M');word(54+pitch*height,4);word(0,4);word(54,4);word(40,4);word(width,4);word(height,4);
    word(1,2);word(24,2);word(0,4);word(pitch*height,4);word(2835,4);word(2835,4);word(0,4);word(0,4);
    for(uint32_t row=height;row>0;--row){const uint32_t y=row-1;for(uint32_t x=0;x<width;++x){const auto pixel=pixels[size_t(y)*width+x];const float alpha=float(pixel&255)/255;
            const float checker=((x/4+y/4)&1)? .16f:.25f;const auto visible=rgba(rgb(pixel)*alpha+point{checker,checker,checker}*(1-alpha));
            out.put(char(visible>>8));out.put(char(visible>>16));out.put(char(visible>>24));}
        for(uint32_t x=width*3;x<pitch;++x)out.put(0);}
    return bool(out);
}
static std::string debug_json_string(std::string_view text){std::string result="\"";for(unsigned char c:text){if(c=='"'||c=='\\'){result+='\\';result+=char(c);}else if(c<32){result+="?";}else result+=char(c);}return result+'"';}
bool write_appearance_debug(const model &m,const impostor_atlas &atlas,uint64_t key,std::string_view label,const std::string &root){
    if(root.empty())return true;
    std::string stem;for(unsigned char c:label){if(stem.size()>=40)break;stem+=((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')?char(c):'_';}
    if(stem.empty())stem="appearance";std::ostringstream identity;identity<<stem<<'-'<<std::hex<<key;
    const auto folder=std::filesystem::u8path(root)/identity.str();std::filesystem::create_directories(folder);
    bool ok=true;const auto &source=m.colors.original;
    if(source.valid()){
        ok=debug_bitmap(folder/"source.bmp",source.width,source.height,source.pixels)&&ok;
        std::ofstream raw(folder/"source.rgba",std::ios::binary);for(auto pixel:source.pixels)for(int shift:{24,16,8,0})raw.put(char(pixel>>shift));ok=bool(raw)&&ok;
        auto regions=source.pixels;constexpr uint32_t colors[]={0xff6060ff,0x60ff60ff,0x6060ffff,0xffff60ff,0xff60ffff,0x60ffffff,0xffa050ff,0x60a0ffff,0xa060ffff,0xffffffff};
        for(size_t n=0;n<m.colors.skins.size();++n){if(!m.colors.skins[n].active)continue;const auto &b=m.colors.skins[n].source;
            for(int y=b.top;y<b.bottom;++y)for(int x=b.left;x<b.right;++x)if(x==b.left||x==b.right-1||y==b.top||y==b.bottom-1)regions[size_t(y)*source.width+x]=colors[n];}
        ok=debug_bitmap(folder/"regions.bmp",source.width,source.height,regions)&&ok;
    }
    for(size_t region=0;region<m.colors.skins.size();++region){const auto &s=m.colors.skins[region];
        for(size_t surface=0;surface<skin::surfaces;++surface){const std::string suffix=std::to_string(region)+'-'+std::to_string(surface)+".bmp";
            ok=debug_bitmap(folder/("texture-"+suffix),s.width,s.height,s.pixels(surface))&&ok;
            std::vector<uint32_t> uv(size_t(s.width)*s.height,0x20242aff);
            auto line=[&](const mesh_vertex &a,const mesh_vertex &b){float ax=a.u*(s.width-1),ay=a.v*(s.height-1),bx=b.u*(s.width-1),by=b.v*(s.height-1);const int steps=std::max(1,int(std::ceil(std::max(std::abs(bx-ax),std::abs(by-ay)))));
                for(int n=0;n<=steps;++n){const float t=float(n)/steps;const auto x=uint32_t(std::clamp(int(std::round(ax+(bx-ax)*t)),0,int(s.width)-1)),y=uint32_t(std::clamp(int(std::round(ay+(by-ay)*t)),0,int(s.height)-1));uv[size_t(y)*s.width+x]=0x70d0ffff;}};
            for(const auto &face:m.lod[0].faces){const auto &a=m.lod[0].vertices[face.vertices[0]];if(a.material!=region||a.back!=surface)continue;
                for(size_t edge=0;edge<3;++edge)line(m.lod[0].vertices[face.vertices[edge]],m.lod[0].vertices[face.vertices[(edge+1)%3]]);}
            ok=debug_bitmap(folder/("uv-"+suffix),s.width,s.height,uv)&&ok;
        }
    }
    for(int direction:{0,2}){auto textured=bake_impostor(m,direction,0);auto plain=m;for(auto &s:plain.colors.skins)s.fill(0xc8c8c8ff);auto untextured=bake_impostor(plain,direction,0);
        auto shade=[](impostor &image){for(size_t n=0;n<image.color.size();++n)if(image.color[n]&255){const auto normal=decode_normal(image.normal[n]);image.color[n]=rgba(rgb(image.color[n])*(.45f+.35f*std::max(0.f,dot(normal,unit({1,-1,2})))));}};
        shade(textured);shade(untextured);ok=debug_bitmap(folder/("model-textured-"+std::to_string(direction)+".bmp"),textured.width,textured.height,textured.color)&&ok;
        ok=debug_bitmap(folder/("model-untextured-"+std::to_string(direction)+".bmp"),untextured.width,untextured.height,untextured.color)&&ok;
    }
    int tile_width=0,tile_height=0;for(const auto &frame:atlas.frames){tile_width=std::max(tile_width,frame.width);tile_height=std::max(tile_height,frame.height);}
    const uint32_t width=uint32_t(tile_width*impostor_atlas::directions),height=uint32_t(tile_height*impostor_atlas::poses);
    std::vector<uint32_t> color(size_t(width)*height),normal=color,depth=color;
    for(size_t frame=0;frame<atlas.frames.size();++frame){const auto &f=atlas.frames[frame];const size_t ox=(frame%impostor_atlas::directions)*tile_width,oy=(frame/impostor_atlas::directions)*tile_height;
        for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){const size_t from=size_t(y)*f.width+x,to=(oy+y)*width+ox+x;color[to]=f.color[from];normal[to]=f.normal[from];if(f.color[from]&255){const float z=.5f+float(f.depth[from])/65520;depth[to]=rgba({z,z,z});}}}
    ok=debug_bitmap(folder/"impostor-color.bmp",width,height,color)&&ok;ok=debug_bitmap(folder/"impostor-normal.bmp",width,height,normal)&&ok;ok=debug_bitmap(folder/"impostor-depth.bmp",width,height,depth)&&ok;
    std::ofstream manifest(folder/"appearance.json");manifest<<"{\"schema\":1,\"generator\":"<<generator_version<<",\"appearance_generator\":"<<appearance_generator_version<<",\"label\":"<<debug_json_string(label)<<",\"key\":\""<<std::hex<<key<<std::dec<<"\",\"anatomy\":\""<<name(m.type.kind)<<"\",\"source_width\":"<<source.width<<",\"source_height\":"<<source.height<<",\"source_format\":\"RGBA bytes, native resolution\",\"materials_bytes\":"<<m.colors.bytes()<<",\"atlas_bytes\":"<<atlas.bytes()<<",\"hidden_details\":\"regional continuation; original rear appearance is unknown\",\"regions\":[";
    for(size_t n=0;n<m.colors.skins.size();++n){const auto &s=m.colors.skins[n];if(n)manifest<<',';manifest<<"{\"region\":"<<n<<",\"width\":"<<s.width<<",\"height\":"<<s.height<<",\"observed_pixels\":"<<s.observed_pixels<<",\"material\":"<<int(s.material)<<",\"crop\":["<<s.source.left<<','<<s.source.top<<','<<s.source.right<<','<<s.source.bottom<<"]}";}
    manifest<<"],\"success\":"<<(ok?"true":"false")<<"}\n";return bool(manifest)&&ok;
}
}
