#include "held_item.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
namespace {int checks=0;void require(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}}
int main(){try{
    using namespace first_person;size_t count=0;const auto inventory=held::models(count);
    require(inventory&&count==30,"thirty distinct authored meshes are compiled into the real module's tables");
    std::set<uint64_t> identities,textures;size_t vertices=0,triangles=0,rigged=0,magazines=0,lenses=0;
    auto pose=std::make_unique<viewmodel::pose_buffer>();
    for(size_t i=0;i<count;++i){const auto &model=*inventory[i];const auto &mesh=model.geometry;
        require(held::valid(model),model.id);require(held::find(model.id)==&model,"all selected sources are reachable in the compiled registry");
        require(identities.insert(model.identity).second,"each mesh has a distinct immutable identity");
        require(!viewmodel::valid_asset(mesh),"held meshes do not pretend to be PSX arms with invented clips");
        require(viewmodel::bind_pose(mesh,*pose),"the shared exact-weight rig evaluator accepts every held mesh");
        const auto grip=viewmodel::position(pose->global[model.grips.primary.node],model.grips.primary.point);
        require(std::max({std::abs(grip.x),std::abs(grip.y),std::abs(grip.z)})<.00005f,"offline axes/units place the shared primary contact at the origin");
        for(size_t v=0;v<mesh.vertex_count;++v){const auto &source=mesh.vertices[v];const auto p=viewmodel::position(pose->palette[source.palette],source.position);
            require(std::isfinite(p.x+p.y+p.z),"weighted source positions remain finite");
            const auto n=viewmodel::direction(pose->palette[source.palette],source.normal);
            require(std::isfinite(n.x+n.y+n.z)&&viewmodel::dot(n,n)>1e-12f,"retained authored normals survive the source rig");}
        for(size_t m=0;m<model.material_count;++m){const auto &material=model.materials[m];if(material.pixels)textures.insert(material.texture_identity);if(material.color[3]<.999f)++lenses;}
        for(size_t t=0;t<mesh.index_count/3;++t)magazines+=model.triangle_part[t]==held::part::magazine;
        vertices+=mesh.vertex_count;triangles+=mesh.index_count/3;rigged+=mesh.joint_count>1;
    }
    require(rigged>=6&&magazines>0,"source firearm bones and detachable magazine geometry were retained");
    require(textures.size()==3&&lenses>0,"authored palettes are shared and the optic's glass material remains distinct");
    require(!held::find("unmapped_mod_item"),"unmapped assets do not invent a model");
    auto broken=*inventory[0];broken.version=99;require(!held::valid(broken),"unknown resource versions fail closed");
    broken=*inventory[0];broken.grips.primary.node=uint16_t(broken.geometry.node_count);require(!held::valid(broken),"contacts cannot refer outside the retained source rig");
    std::cout<<"Held source tables: "<<count<<" models, "<<vertices<<" vertices, "<<triangles<<" triangles; "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}
