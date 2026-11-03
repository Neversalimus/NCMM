#pragma once
#include "vehicle_geometry.hpp"
#include "vehicle_cabin_blueprints_fixture.hpp"
#include <array>
#include <vector>
namespace vehicle_cabin_quality_controls {
namespace v=first_person::vehicle;
inline std::vector<std::array<uint32_t,12>> glass_words(const v::model &m){std::vector<std::array<uint32_t,12>> result;
    for(const auto &t:m.triangles)if(m.materials[t.material].kind==NCMM_VM_GLASS)for(const auto &a:t.vertices){
        const auto &c=m.materials[t.material].color;const float values[]={a.position.x,a.position.y,a.position.z,a.normal.x,a.normal.y,a.normal.z,a.u,a.v,c[0],c[1],c[2],c[3]};
        std::array<uint32_t,12> words{};std::memcpy(words.data(),values,sizeof(values));result.push_back(words);}return result;
}
template<class Require> void verify_roof_contour(Require &&require){unsigned liners=0,eligible=0;
    const unsigned expected_liners[]={2,2,2,2,0,2};
    for(size_t vi=0;vi<std::size(owned_cabin_blueprints::blueprints);++vi){const unsigned previous_liners=liners;
        const auto s=owned_cabin_blueprints::construction(vi);v::topology shape(s,s.vehicles[0]);
        for(size_t pi=0;pi<s.parts.size();++pi){const auto &p=s.parts[pi];const auto pan=shape.cabin_screen_pan(p);
            if(pan.liner){++liners;require(bool(pan)&&pan.high-pan.low>=.04f&&pan.low>=-.5f&&pan.high<=.5f,"liner retains at least four centimetres of proved floor inside its actual mount");
                for(const auto &[index,wheel]:shape.sizes){(void)index;const int sign=wheel.y==shape.max_y?1:wheel.y==shape.min_y?-1:0;
                    const float cy=float(wheel.y-p.mount_y)+sign*std::max(0.f,.475f-wheel.width*.5f),r=std::hypot(wheel.radius*1.025f+.015f,wheel.width*.6f+.015f);
                    const float dx=std::max(0.f,std::abs(float(wheel.x-p.mount_x))-.5f),dy=cy<pan.low?pan.low-cy:cy>pan.high?cy-pan.high:0;
                    require(dx*dx+dy*dy>r*r,"all retained floor and liner positions stay beyond every complete possible wheel sphere");}
            }
            if(shape.cabin_screen_end(p)){++eligible;
                auto changed=p;changed.state|=NCMM_VP_BROKEN;require(!shape.cabin_screen_pan(changed),"broken screen cannot own an inferred pan");
                changed=p;changed.state|=NCMM_VP_TOPOLOGY_ONLY;require(!shape.cabin_screen_pan(changed),"hidden screen cannot own an inferred pan");
                changed=p;changed.features|=NCMM_VP_DOOR;require(!shape.cabin_screen_pan(changed),"door remains its own moving assembly");
                changed=p;changed.features&=~uint32_t(NCMM_VP_PANEL);require(!shape.cabin_screen_pan(changed),"non-obstructing glass remains transparent");
                auto other=shape;other.mounts[{p.mount_x,p.mount_y,p.mount_z}].features|=NCMM_VP_FLOOR;require(!other.cabin_screen_pan(p),"actual floor prevents duplicate pan");
                other=shape;other.mounts[{p.mount_x,p.mount_y,p.mount_z}].features&=~uint32_t(NCMM_VP_FRAME);require(!other.cabin_screen_pan(p),"removing actual supporting frame prevents invented pan");
                other=shape;for(auto &[at,n]:other.mounts){(void)at;n.features&=~uint32_t(NCMM_VP_ROOF);}require(!other.cabin_screen_pan(p),"removing all actual roof support retains open fallback");
                other=shape;for(auto &[at,n]:other.mounts){if(at!=v::mount{p.mount_x,p.mount_y,p.mount_z}&&(n.features&NCMM_VP_ROOF))n.component+=100;}require(!other.cabin_screen_pan(p),"disconnected roofs cannot prove a cabin pan");
                other=shape;other.narrow=true;require(!other.cabin_screen_pan(p),"narrow custom structures retain fallback");
                other=shape;other.kind=v::archetype::boat;require(!other.cabin_screen_pan(p),"boats retain actual hull geometry");
                other=shape;other.sizes.clear();other.sizes.emplace(0,v::topology::wheel_size{p.mount_x,p.mount_y,p.mount_z,4.f,3.f});require(!other.cabin_screen_pan(p),"wide central wheel remains uncertain");
                other=shape;if(!other.sizes.empty()){other.sizes.begin()->second.z=p.mount_z+1;require(!other.cabin_screen_pan(p),"unproved wheel mount levels keep fallback");}
                const int inward=shape.part_edge(p)==0?-1:1;
                const auto support=shape.mounts.find({p.mount_x+inward,p.mount_y,p.mount_z});
                const bool aligned=support!=shape.mounts.end()&&(support->second.features&(NCMM_VP_FRAME|NCMM_VP_ROOF))==(NCMM_VP_FRAME|NCMM_VP_ROOF);
                if(pan.liner&&!aligned){other=shape;const int side=pan.liner==3?-1:1;
                    auto &opposite=other.mounts[{p.mount_x+inward,p.mount_y+side*2,p.mount_z}];opposite.features=NCMM_VP_FRAME|NCMM_VP_ROOF;opposite.component=shape.mounts.at({p.mount_x,p.mount_y,p.mount_z}).component;
                    require(!other.cabin_screen_pan(p),"a gap between roof islands is not an outside edge");
                }
            }
            for(unsigned lod=0;lod<3;++lod){const auto m=v::generate(s,shape,p,lod);require(bool(m),"every installed part retains model ownership");
                // Cabin-end panes retain the same glass with or without the
                // new lower pan. Bus side glazing uses its own existing waist
                // shape, so removing PANEL is not an equivalent reference there.
                if(shape.cabin_screen_end(p)){auto transparent=p;transparent.features&=~uint32_t(NCMM_VP_PANEL);
                    const auto glass=v::generate(s,shape,transparent,lod);require(bool(glass)&&glass_words(*m)==glass_words(*glass),"all original glass positions, normals, UVs and colours remain exact beside new liners");}
                for(const auto &t:m->triangles){require(t.material<m->materials.size(),"triangle material remains valid");
                    for(const auto &a:t.vertices){require(std::isfinite(a.position.x)&&std::isfinite(a.position.y)&&std::isfinite(a.position.z),"positions remain finite");
                        require(std::abs(v::dot(a.normal,a.normal)-1)<.001f,"normals remain unit length");}}
            }
        }
        require(liners-previous_liners==expected_liners[vi],"actual car, SUV, van, semi, APC and bus roof contours retain their proved wheelwell geometry");
    }
    require(eligible>0&&liners==10,"owned actual cabin and wheel configurations exercise both eligibility and clearances");
}

template<class Require> void verify_lamp_optics(Require &&require){
    const uint32_t features[]={NCMM_VP_LIGHT,NCMM_VP_LIGHT|NCMM_VP_LIGHT_OMNI,NCMM_VP_LIGHT|NCMM_VP_LIGHT_OMNI|NCMM_VP_LIGHT_CEILING,
        NCMM_VP_FRAME,NCMM_VP_ROOF,NCMM_VP_PANEL,NCMM_VP_DOOR,NCMM_VP_WINDOW|NCMM_VP_WINDSHIELD|NCMM_VP_PANEL,
        NCMM_VP_SEAT,NCMM_VP_CONTROLS,NCMM_VP_ENGINE,NCMM_VP_CARGO,NCMM_VP_FLOOR|NCMM_VP_AISLE,NCMM_VP_LIGHT|NCMM_VP_PANEL};
    for(unsigned f=0;f<sizeof(features)/sizeof(features[0]);++f)for(unsigned damaged=0;damaged<2;++damaged)for(unsigned on=0;on<2;++on)for(unsigned lod=0;lod<3;++lod){
        v::snapshot s;s.enabled=true;s.scene.struct_size=sizeof(s.scene);s.scene.version=1;s.scene.world_epoch=1;s.scene.level_height=3.2f;
        ncmm_graphics_vehicle_v1 car{};car.entity_id=1;car.part_count=1;car.min_x=car.min_y=0;car.max_x=car.max_y=0;
        ncmm_graphics_vehicle_part_v1 p{};p.part_id=1;p.cell_index=0;p.features=features[f];p.health=damaged?.4f:1.f;
        p.state=NCMM_VP_EXISTS|(on?uint32_t(NCMM_VP_LIGHT_ON):0u);p.material=NCMM_VM_PAINT;p.color[0]=p.color[1]=p.color[2]=.6f;p.color[3]=1;p.wheel_radius=.32f;p.wheel_width=.18f;
        std::strcpy(p.id,f==0?"head_light":"owned-data-control");s.parts.push_back(p);s.vehicles.push_back(car);s.scene.vehicle_count=s.scene.part_count=1;
        v::topology layout(s,s.vehicles[0]);auto m=v::generate(s,layout,p,lod);require(bool(m),"installed component always owns its generated model");
        unsigned lens=0,rim=0;
        for(const auto &t:m->triangles){require(t.material<m->materials.size(),"all optical and housing material indices remain valid");
            const auto &material=m->materials[t.material];
            if(m->kind==v::component::light){
                require(!material.emissive||((p.state&NCMM_VP_LIGHT_ON)&&material.kind==NCMM_VM_GLASS),"only the existing Native-powered lens emits");
                if(material.kind==NCMM_VM_GLASS)++lens;
                if(f==0&&material.kind==NCMM_VM_METAL){++rim;require(!material.emissive,"metal bezel is not an invented emitter");}
            }
            for(const auto &x:t.vertices){const auto at=x.position,n=x.normal;
                require(std::isfinite(at.x)&&std::isfinite(at.y)&&std::isfinite(at.z)&&std::isfinite(n.x)&&std::isfinite(n.y)&&std::isfinite(n.z),"positions and normals are finite");
                require(std::abs(v::dot(n,n)-1)<.0001f,"every optical normal has unit length");
                if(f==0)require(at.x>=.42f-.00001f&&at.x<=.605f+.00001f&&std::abs(at.y)<=.13f+.00001f&&at.z>=layout.chassis+.31f-.00001f&&at.z<=layout.chassis+.49f+.00001f,"directional lamp stays within its original complete housing and lens bounds");
            }
            if(f==0&&(material.kind==NCMM_VM_GLASS||material.kind==NCMM_VM_METAL)){
                const auto area=v::cross(t.vertices[1].position-t.vertices[0].position,t.vertices[2].position-t.vertices[0].position);
                require(area.x>0&&v::dot(area,area)>1e-16f,"the lens and rim face outward without collapsed or backward triangles");
            }
        }
        if(f==0&&lod<2){require(lens==(lod==0?36u:24u)&&rim==(lod==0?24u:16u),"actual three-dimensional lens and metal rim retain all detailed levels");
            float low=1,high=-1;for(const auto &t:m->triangles)if(m->materials[t.material].kind==NCMM_VM_GLASS)for(const auto &x:t.vertices){low=std::min(low,x.position.x);high=std::max(high,x.position.x);}
            require(high-low>.002f,"directional optics have real depth inside the original attachment footprint");}
    }
}

}
