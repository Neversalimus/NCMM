#pragma once
#include "creature_appearance.hpp"
namespace first_person::creature {
enum class motion : uint8_t { root, body, head, thigh, shin, arm, forearm, wing, rotor, tail, segment, tentacle, fixed };
struct bone {int16_t parent=-1;point origin{};motion role=motion::fixed;float phase=0;};
struct mesh_vertex {point position,normal;float u=0,v=0;uint8_t joint=0,material=0,back=0;};
struct face {std::array<uint32_t,3> vertices;};
struct mesh {std::vector<mesh_vertex> vertices;std::vector<face> faces;};
struct model {
    phenotype type;appearance colors;std::vector<bone> bones;std::array<mesh,2> lod;
    float radius=1;point low{},high{};
    size_t bytes()const{size_t n=sizeof(*this)+colors.bytes()+bones.capacity()*sizeof(bone);for(const auto &m:lod)n+=m.vertices.capacity()*sizeof(mesh_vertex)+m.faces.capacity()*sizeof(face);return n;}
};
struct part {point start,end,radius;uint8_t joint=0;region material=region::torso;bool angular=false;};
class builder {
    model result;std::vector<part> parts;std::vector<point> joint_world;
    uint8_t joint(point origin,int parent,motion role,float phase=0){
        if(result.bones.size()>=128)return 0;
        const auto id=uint8_t(result.bones.size());result.bones.push_back({int16_t(parent),parent<0?origin:origin-joint_world[size_t(parent)],role,phase});joint_world.push_back(origin);return id;
    }
    void ellipsoid(point center,point radius,uint8_t joint_id,region material,bool angular=false){parts.push_back({center-joint_world[joint_id],center-joint_world[joint_id],radius,joint_id,material,angular});}
    void link(point start,point end,float r0,float r1,uint8_t joint_id,region material,bool angular=false){
        // A stretched ellipsoid extends beyond both joints by its thickness;
        // adjoining limbs overlap even at the maximum allowed bend.
        const float length=std::sqrt(dot(end-start,end-start));parts.push_back({start-joint_world[joint_id],end-joint_world[joint_id],{r0,r1,length*.5f+std::min(r0,r1)*.55f},joint_id,material,angular});
    }
    void limb(point start,point knee,point foot,float radius,int parent,float phase,bool arm=false){
        auto upper=joint(start,parent,arm?motion::arm:motion::thigh,phase);
        const auto upper_material=result.type.kind==anatomy::humanoid?(arm?region::appendage:region::lower):region::limb;
        link(start,knee,radius,radius*.90f,upper,upper_material,result.type.kind==anatomy::robot);
        auto lower=joint(knee,upper,arm?motion::forearm:motion::shin,phase);
        link(knee,foot,radius*.75f,radius*.66f,lower,arm?upper_material:region::lower,result.type.kind==anatomy::robot);
        ellipsoid(foot+point{radius*.30f,0,0},{radius*1.20f,radius*.80f,radius*.65f},lower,arm?region::hand:region::foot);
    }
    void build_parts(){const auto &p=result.type;const float h=p.height,w=p.width,l=p.length,b=p.bulk;
        const auto root=joint({},-1,motion::root);const auto body=joint({0,0,h*.50f},root,motion::body);
        const bool robot=p.kind==anatomy::robot;
        if(p.kind==anatomy::humanoid||(robot&&p.legs==2)){
            ellipsoid({0,0,h*.66f},{l*.43f,w*.43f*b,h*.22f},body,region::torso,robot);
            ellipsoid({0,0,h*.47f},{l*.42f,w*.38f,h*.115f},body,region::lower,robot);
            if(!(p.flags&NCMM_CR_NOHEAD)){auto head=joint({0,0,h*.80f},body,motion::head);
                ellipsoid({0,0,h*.855f},{l*.17f,w*.16f,h*.070f},head,region::head);
                ellipsoid({l*.015f,0,h*.915f},{l*.34f,w*.28f,h*.092f},head,region::head,robot);}
            for(int side:{-1,1}){const float phase=side<0?0:pi;
                limb({0,side*w*.23f,h*.47f},{-.035f*l,side*w*.24f,h*.25f},{.03f*l,side*w*.25f,h*.03f},w*.135f,body,phase);
                limb({0,side*w*.43f,h*.77f},{-.04f*l,side*w*.53f,h*.565f},{.07f*l,side*w*.51f,h*.37f},w*.105f,body,phase+pi,true);
            }
        }else if(p.kind==anatomy::quadruped||p.kind==anatomy::reptile||(robot&&p.legs==4)){
            const bool reptile=p.kind==anatomy::reptile;
            ellipsoid({-.04f*l,0,h*.57f},{l*.34f,w*.34f*b,h*.27f},body,region::torso,robot);
            const auto head=joint({l*.24f,0,h*.65f},body,motion::head);
            if(!(p.flags&NCMM_CR_NOHEAD)){
                link({l*.24f,0,h*.65f},{l*.34f,0,h*.80f},w*.17f,w*.18f,head,region::torso,robot);
                ellipsoid({l*.38f,0,h*.83f},{l*.15f,w*.22f,h*.165f},head,region::head,robot);
                ellipsoid({l*.48f,0,h*.775f},{l*.12f,w*.15f,h*.09f},head,region::head,robot);
                if(!reptile&&!robot)for(int side:{-1,1})ellipsoid({l*.33f,side*w*.17f,h*.98f},{l*.055f,w*.075f,h*.12f},head,region::head);
            }
            for(int end:{-1,1})for(int side:{-1,1}){float x=end*l*.24f;float spread=reptile?.48f:.28f;
                limb({x,side*w*.25f,h*.56f},{x-end*l*.06f,side*w*spread,h*.28f},{x+.02f*l,side*w*(spread+.025f),h*.045f},w*(reptile?.095f:.105f),body,(end==side?0:pi));}
            auto tail=joint({-l*.30f,0,h*.62f},body,motion::tail);
            link({-l*.30f,0,h*.62f},{-l*.62f,0,h*(reptile?.24f:.80f)},w*.045f,w*.06f,tail,region::tail);
        }else if(p.kind==anatomy::arachnid||p.kind==anatomy::insect||p.kind==anatomy::centipede||(robot&&p.legs>=6)){
            const bool spider=p.kind==anatomy::arachnid||(robot&&p.legs==8);int segments=p.kind==anatomy::centipede||(robot&&p.legs>=12)?8:spider?2:3;
            for(int i=0;i<segments;++i){float x=(float(i)/std::max(1,segments-1)-.5f)*l*.60f;
                auto seg=joint({x,0,h*.55f},body,motion::segment,float(i)*.65f);
                float r=spider&&i==0?.28f:segments>3?.085f:.19f;
                ellipsoid({x,0,h*.55f},{l*r,w*(spider?.24f:.18f),h*(spider?.35f:.30f)},seg,i==segments-1?region::head:region::torso,robot);
            }
            // A spider's abdomen and cephalothorax have a narrow pedicel.
            // Their separate ellipsoids must not become disconnected volumes.
            if(spider)link({-l*.30f,0,h*.55f},{l*.30f,0,h*.55f},std::min(w*.085f,h*.11f),std::min(w*.085f,h*.11f),body,region::torso,robot);
            int pairs=p.legs/2;
            for(int n=0;n<pairs;++n)for(int side:{-1,1}){float t=(n+.5f)/pairs, x=(t-.5f)*l*.62f;
                const float root=spider?l*(.27f+(t-.5f)*.22f):x;
                const float knee=spider?l*(.10f+(t-.5f)*.62f):x+(t-.5f)*l*.22f;
                const float foot=spider?l*(.05f+(t-.5f)*1.2f):x+(t-.5f)*l*.42f;
                limb({root,side*w*.13f,h*.59f},{knee,side*w*.43f,h*.72f},{foot,side*w*.50f,h*.04f},std::min(w*.035f,h*.07f),body,(n+int(side>0))*pi);}
            if(!spider)for(int side:{-1,1}){auto antenna=joint({l*.36f,side*w*.08f,h*.64f},body,motion::tail,float(side));link({l*.36f,side*w*.08f,h*.64f},{l*.59f,side*w*.17f,h*.90f},w*.013f,w*.010f,antenna,region::appendage);}
        }else if(p.kind==anatomy::bird){
            ellipsoid({-.07f*l,0,h*.49f},{l*.29f,w*.27f,h*.31f},body,region::torso);
            auto head=joint({l*.16f,0,h*.65f},body,motion::head);
            link({l*.16f,0,h*.65f},{l*.21f,0,h*.85f},w*.11f,w*.10f,head,region::head);
            if(!(p.flags&NCMM_CR_NOHEAD)){ellipsoid({l*.24f,0,h*.86f},{l*.13f,w*.18f,h*.13f},head,region::head);link({l*.31f,0,h*.84f},{l*.48f,0,h*.81f},w*.06f,w*.035f,head,region::detail,true);}
            for(int side:{-1,1})limb({-.04f*l,side*w*.14f,h*.36f},{.07f*l,side*w*.16f,h*.17f},{.13f*l,side*w*.20f,h*.035f},w*.035f,body,side>0?0:pi);
            auto tail=joint({-l*.25f,0,h*.40f},body,motion::tail);ellipsoid({-l*.39f,0,h*.39f},{l*.22f,w*.22f,h*.05f},tail,region::tail);
        }else if(p.kind==anatomy::serpent){
            int parent=body;for(int i=0;i<9;++i){const float t=float(i)/8,x=(.5f-t)*l*.90f;const auto seg=joint({x,0,h*.48f},parent,motion::segment,t*5);parent=seg;
                float radius=h*(.48f-.34f*t);ellipsoid({x,0,h*.48f},{l*.085f,radius,radius},seg,i==0?region::head:region::torso);}
        }else if(p.kind==anatomy::fish){
            ellipsoid({.02f*l,0,h*.5f},{l*.37f,w*.27f,h*.46f},body,region::torso);
            auto head=joint({l*.23f,0,h*.5f},body,motion::head);ellipsoid({l*.29f,0,h*.52f},{l*.19f,w*.28f,h*.38f},head,region::head);
            auto tail=joint({-l*.25f,0,h*.5f},body,motion::tail);ellipsoid({-l*.44f,0,h*.5f},{l*.18f,w*.055f,h*.52f},tail,region::tail);
            for(int side:{-1,1}){auto fin=joint({.01f*l,side*w*.20f,h*.40f},body,motion::wing,side*pi);ellipsoid({-.05f*l,side*w*.36f,h*.30f},{l*.13f,w*.24f,h*.04f},fin,region::wing);}
        }else if(p.kind==anatomy::amorphous){
            ellipsoid({0,0,h*.40f},{l*.41f,w*.43f,h*.43f},body,region::torso);
            for(int n=0;n<6;++n){float a=n*pi/3;point rootpos{std::cos(a)*l*.22f,std::sin(a)*w*.23f,h*.22f};auto limb_id=joint(rootpos,body,motion::tentacle,a);
                ellipsoid({std::cos(a)*l*.34f,std::sin(a)*w*.36f,h*.17f},{l*.20f,w*.19f,h*.18f},limb_id,region::appendage);}
        }else if(p.kind==anatomy::exotic){
            ellipsoid({0,0,h*.61f},{l*.27f,w*.28f,h*.37f},body,region::torso);
            if(!(p.flags&NCMM_CR_NOHEAD)){auto head=joint({l*.16f,0,h*.76f},body,motion::head);ellipsoid({l*.22f,0,h*.83f},{l*.23f,w*.23f,h*.17f},head,region::head);}
            for(int n=0;n<6;++n){float a=n*pi/3;point start{std::cos(a)*l*.15f,std::sin(a)*w*.15f,h*.45f},mid{std::cos(a)*l*.35f,std::sin(a)*w*.34f,h*.25f},tip{std::cos(a)*l*.51f,std::sin(a)*w*.51f,h*.045f};
                auto j=joint(start,body,motion::tentacle,a);link(start,mid,w*.085f,w*.07f,j,region::limb);auto k=joint(mid,j,motion::tentacle,a+.5f);link(mid,tip,w*.060f,w*.035f,k,region::appendage);}
        }else{
            // Unknown mods receive a closed, conservative volume; no ID exceptions.
            ellipsoid({0,0,h*.5f},{l*.43f,w*.44f,h*.50f},body,region::torso,robot);
            if(robot&&(p.flags&NCMM_CR_IMMOBILE)){ellipsoid({0,0,h*.12f},{l*.5f,w*.5f,h*.12f},body,region::lower,true);
                auto turret=joint({0,0,h*.65f},body,motion::head);link({0,0,h*.65f},{l*.78f,0,h*.65f},w*.07f,w*.07f,turret,region::appendage,true);}
            if(robot&&(p.flags&NCMM_CR_FLIES))for(int n=0;n<4;++n){float a=(n+.5f)*pi*.5f;point end{std::cos(a)*l*.65f,std::sin(a)*w*.65f,h*.65f};
                link({0,0,h*.65f},end,w*.045f,w*.045f,body,region::appendage,true);auto rotor=joint(end,body,motion::rotor,a);
                ellipsoid(end,{l*.20f,w*.025f,h*.015f},rotor,region::detail,true);}

        }
        if(p.kind==anatomy::bird||(p.flags&NCMM_CR_FLIES&&p.kind!=anatomy::amorphous&&p.kind!=anatomy::robot)){
            for(int side:{-1,1}){auto wing=joint({0,side*w*.18f,h*.65f},body,motion::wing,side>0?0:pi);
                ellipsoid({-.1f*l,side*w*.38f,h*.62f},{l*.34f,w*.28f,h*.06f},wing,region::wing);
                if(p.flags&NCMM_CR_FLIES)ellipsoid({-.18f*l,side*w*.62f,h*.63f},{l*.28f,w*.23f,h*.04f},wing,region::wing);}
        }
    }
    void tessellate(mesh &m,const part &p,int quality){
        int sides=p.angular?(quality==0?6:4):quality==0?12:6,rings=p.angular?3:quality==0?6:3;
        point z=p.end-p.start;const bool sphere=dot(z,z)<1e-8f;z=sphere?point{0,0,1}:unit(z);
        const point x=unit(cross(std::abs(z.z)>.9f?point{0,1,0}:point{0,0,1},z)),y=cross(z,x),center=(p.start+p.end)*.5f;
        const uint32_t offset=uint32_t(m.vertices.size());
        for(int row=0;row<=rings;++row){float v=float(row)/rings,a=pi*v;for(int col=0;col<=sides;++col){float u=float(col)/sides,angle=2*pi*u;
            point q{std::sin(a)*std::cos(angle),std::sin(a)*std::sin(angle),std::cos(a)};
            point pos=center+x*(q.x*p.radius.x)+y*(q.y*p.radius.y)+z*(q.z*p.radius.z);
            point normal=unit(x*(q.x/std::max(.001f,p.radius.x))+y*(q.y/std::max(.001f,p.radius.y))+z*(q.z/std::max(.001f,p.radius.z)));
            m.vertices.push_back({pos,normal,0,0,p.joint,uint8_t(p.material),0});
        }}
        for(int row=0;row<rings;++row)for(int col=0;col<sides;++col){uint32_t a=offset+row*(sides+1)+col,b=a+1,c=a+sides+1,d=c+1;
            if(row>0)m.faces.push_back({{a,c,b}});
            if(row<rings-1)m.faces.push_back({{b,c,d}});
        }
    }
    struct bounds {point low{1e8f,1e8f,1e8f},high{-1e8f,-1e8f,-1e8f};};
    void project_materials(){
        std::array<bounds,size_t(region::count)> regions;
        for(const auto &v:result.lod[0].vertices){const auto q=v.position+joint_world[v.joint];auto &b=regions[v.material];
            b.low={std::min(b.low.x,q.x),std::min(b.low.y,q.y),std::min(b.low.z,q.z)};b.high={std::max(b.high.x,q.x),std::max(b.high.y,q.y),std::max(b.high.z,q.z)};}
        // Orthographic charts use rest-space positions, not normals as UVs.
        // All adjoining segments of a region share physical bounds, so bends
        // cannot restart the face/cloth image on each ellipsoid or limb bone.
        for(auto &m:result.lod){mesh projected;projected.faces.reserve(m.faces.size());
            std::vector<std::array<int,skin::surfaces>> remap(m.vertices.size());for(auto &r:remap)r.fill(-1);
            for(const auto &f:m.faces){const auto n=m.vertices[f.vertices[0]].normal+m.vertices[f.vertices[1]].normal+m.vertices[f.vertices[2]].normal;
                const uint8_t surface=result.colors.profile?(std::abs(n.y)>=std::abs(n.x)?0:n.x<0?2:3):
                    (std::abs(n.x)>=std::abs(n.y)?(n.x>=0?0:1):(n.y<0?2:3));
                face out{};for(size_t i=0;i<3;++i){const auto id=f.vertices[i];auto &mapped=remap[id][surface];
                    if(mapped<0){auto v=m.vertices[id];const auto q=v.position+joint_world[v.joint];const auto &b=regions[v.material];
                        const auto span=b.high-b.low;v.back=surface;
                        if(result.colors.profile)v.u=surface==0?(q.x-b.low.x)/std::max(.0001f,span.x):(q.y-b.low.y)/std::max(.0001f,span.y);
                        else v.u=surface==0?(b.high.y-q.y)/std::max(.0001f,span.y):surface==1?(q.y-b.low.y)/std::max(.0001f,span.y):(b.high.x-q.x)/std::max(.0001f,span.x);
                        v.v=(b.high.z-q.z)/std::max(.0001f,span.z);v.u=std::clamp(v.u,0.f,1.f);v.v=std::clamp(v.v,0.f,1.f);
                        mapped=int(projected.vertices.size());projected.vertices.push_back(v);}
                    out.vertices[i]=uint32_t(mapped);}
                projected.faces.push_back(out);}
            m=std::move(projected);}
    }
public:
    model generate(const phenotype &p,const sprite &s){result={};result.type=p;result.colors=extract(s,p,used_regions(p));parts.clear();joint_world.clear();
        if(!result.colors.sourced&&result.type.reason==fallback::semantic)result.type.reason=fallback::missing_sprite;
        // Silhouette can refine bulk, not overwrite reliable physical dimensions.
        if(result.colors.sourced)result.type.bulk*=std::clamp(result.colors.silhouette_ratio/(p.kind==anatomy::humanoid?.55f:1.3f),.92f,1.08f);
        build_parts();result.low={1e8f,1e8f,1e8f};result.high={-1e8f,-1e8f,-1e8f};
        for(int lod=0;lod<2;++lod)for(const auto &part:parts)tessellate(result.lod[lod],part,lod);
        project_materials();
        for(const auto &v:result.lod[0].vertices){const point q=v.position+joint_world[v.joint];result.low={std::min(result.low.x,q.x),std::min(result.low.y,q.y),std::min(result.low.z,q.z)};result.high={std::max(result.high.x,q.x),std::max(result.high.y,q.y),std::max(result.high.z,q.z)};}
        result.radius=std::sqrt(p.height*p.height+p.width*p.width+p.length*p.length);
        return std::move(result);
    }
};
inline uint32_t texel(const model &m,const mesh_vertex &v){return sample_skin(m.colors.skins[v.material],v.back,v.u,v.v);}
}
