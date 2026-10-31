#include "ncmm_gpu_winding.hpp"
#include "opaque_depth_fixture.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {
unsigned checks=0;
void require(bool value,const char *why){++checks;if(!value){std::fprintf(stderr,"winding data proof: %s\n",why);std::exit(1);}}
ncmm_gpu_matrix_v1 identity(){ncmm_gpu_matrix_v1 m{};for(unsigned n=0;n<4;++n)m.columns[n*5]=1;return m;}
}
int main(){
    for(uint32_t finish=0;finish<32;++finish)require(ncmm_gpu::reflective_component_finish(finish)==(finish==7||finish==8),"only the original shared paint/metal reflection branch admits the exact constant");
    require(!ncmm_gpu::reflective_component_finish(UINT32_MAX),"unknown finish values keep the general component program");
    auto pose=identity();require(ncmm_gpu::winding_pose(pose),"identity is a proper rigid pose");
    require(ncmm_gpu::winding_bone(pose),"the exact static root bone is accepted");
    for(unsigned n=0;n<20000;++n){const float yaw=float(n)*.037f,pitch=float(n%101)*.013f;
        const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
        pose=identity();pose.columns[0]=cy;pose.columns[1]=sy;pose.columns[4]=-sy;pose.columns[5]=cy;pose.columns[12]=float(n%300)-150;
        require(ncmm_gpu::winding_pose(pose),"real yaw/translation keeps the normal/index handedness");
        auto mirror=pose;for(unsigned j=0;j<3;++j)mirror.columns[j]=-mirror.columns[j];
        require(!ncmm_gpu::winding_pose(mirror),"reflected draws retain two-sided rasterization");
        auto scale=pose;for(unsigned j=0;j<3;++j)scale.columns[j]*=1.1f;
        require(!ncmm_gpu::winding_pose(scale),"nonrigid scale retains the original shader path");
        ncmm_gpu_camera_v1 camera{};
        camera.right[0]=-sy;camera.right[1]=cy;camera.up[0]=-cy*sp;camera.up[1]=-sy*sp;camera.up[2]=cp;
        camera.forward[0]=cy*cp;camera.forward[1]=sy*cp;camera.forward[2]=sp;
        require(ncmm_gpu::winding_camera(camera),"the actual right/up/forward view has the proven clockwise front");
        camera.up[2]+=1.f;require(!ncmm_gpu::winding_camera(camera),"unproved projection basis falls back");
        auto animated=identity();animated.columns[12]=.01f;
        require(!ncmm_gpu::winding_bone(animated),"animated bones cannot use the static proof");
    }
    std::array<ncmm_gpu_vertex_v1,3> vertices{};vertices[1].position[0]=1.5f;vertices[2].position[1]=1.5f;
    for(auto &v:vertices)v.face_normal[2]=1;
    std::array<uint32_t,3> indices{0,1,2};ncmm_gpu_material_v1 material{};material.flags=NCMM_GPU_CULL_BACK|NCMM_GPU_SMOOTH;material.color[3]=1;
    const ncmm_gpu_mesh_v1 mesh{1,vertices.data(),indices.data(),&material,3,3,1,0};
    auto proof=[&]{return ncmm_gpu::winding_range(mesh,0,0,3);};
    require(proof(),"the normalized actual cube face proves its existing shader side condition");
    std::swap(indices[1],indices[2]);require(!proof(),"opposite index winding is not silently reinterpreted");std::swap(indices[1],indices[2]);
    vertices[2].flags=NCMM_GPU_BILLBOARD;require(!proof(),"billboard positions need the original vertex path");vertices[2].flags=0;
    vertices[2].joint=1;require(!proof(),"skinned corners need the original path");vertices[2].joint=0;
    vertices[2].face_normal[2]=std::nextafter(1.f,2.f);require(!proof(),"even one unmatched normal bit rejects optimization");vertices[2].face_normal[2]=1;
    material.color[3]=.995f;require(!proof(),"fractional opaque alpha cannot change its raster state");material.color[3]=1;
    material.flags|=NCMM_GPU_IMPOSTOR;require(!proof(),"depth-altering impostors remain unchanged");material.flags&=~NCMM_GPU_IMPOSTOR;
    material.flags&=~NCMM_GPU_CULL_BACK;require(!proof(),"two-sided surfaces remain two-sided");material.flags|=NCMM_GPU_CULL_BACK;
    vertices[2].position[0]=std::numeric_limits<float>::quiet_NaN();require(!proof(),"nonfinite source data never proves eligibility");vertices[2].position[0]=0;
    require(!ncmm_gpu::winding_range(mesh,0,1,3)&&!ncmm_gpu::winding_range(mesh,1,0,3),"range and material bounds remain strict");
    pose=identity();pose.columns[3]=.01f;require(!ncmm_gpu::winding_pose(pose),"perspective instance matrices fall back");
    {
        const auto saved=vertices;const auto old_material=material;
        auto depth=[&]{return ncmm_gpu::rigid_opaque_range(mesh,0,0,3);};
        require(depth(),"the ordinary fully opaque model proves the component fragment coverage and flag branches");
        for(uint32_t flag:{uint32_t(NCMM_GPU_TEXTURED),uint32_t(NCMM_GPU_CUTOUT),uint32_t(NCMM_GPU_IMPOSTOR),
            uint32_t(NCMM_GPU_SPILL),uint32_t(NCMM_GPU_HELD_MATERIAL),uint32_t(NCMM_GPU_CREATURE_VISUAL),uint32_t(NCMM_GPU_BILLBOARD)}){
            material.flags|=flag;require(!depth(),"coverage/depth-altering and held materials retain the general depth shader");material=old_material;}
        material.finish=12;require(!depth(),"animated water keeps the general shader");material=old_material;
        material.finish=16;require(!depth(),"contact alpha falloff keeps the general shader");material=old_material;
        material.color[3]=.995f;require(!depth(),"fractional opaque alpha keeps the general shader");material=old_material;
        material.texture_key=9;require(!depth(),"texture ownership cannot be omitted from the proof");material=old_material;
        material.normal_key=9;require(!depth(),"normal-texture ownership retains the general fragment program");material=old_material;
        material.depth_key=9;require(!depth(),"depth-texture ownership retains the general fragment program");material=old_material;
        constexpr uint32_t allowed=NCMM_GPU_CULL_BACK|NCMM_GPU_SMOOTH|NCMM_GPU_EMISSIVE;
        for(unsigned bit=0;bit<32;++bit){material.flags=old_material.flags|(uint32_t(1)<<bit);
            require(depth()==((material.flags&~allowed)==0),"every unproved flag bit keeps its original fragment branch");material=old_material;}
        for(uint32_t extra:{0u,uint32_t(NCMM_GPU_CULL_BACK),uint32_t(NCMM_GPU_EMISSIVE),uint32_t(NCMM_GPU_CULL_BACK|NCMM_GPU_EMISSIVE)}){
            material.flags=NCMM_GPU_SMOOTH|extra;
            require(depth()&&((material.flags&(512u|16384u))|8u)==material.flags&&material.color[3]==1.f,
                "all specialized integer flags and alpha constants equal their proved original values");material=old_material;}
        vertices[2].joint=1;require(!depth(),"skinned depth remains unchanged");vertices=saved;
        vertices[2].flags=NCMM_GPU_BILLBOARD;require(!depth(),"billboard depth remains unchanged");vertices=saved;
        vertices[2].position[0]=std::numeric_limits<float>::infinity();require(!depth(),"nonfinite corners fail closed");vertices=saved;
        material.flags&=~NCMM_GPU_SMOOTH;require(!depth(),"the specialization stays limited to explicit smooth component materials");material=old_material;
        require(!ncmm_gpu::rigid_opaque_range(mesh,0,1,3)&&!ncmm_gpu::rigid_opaque_range(mesh,1,0,3),"ordinary depth material and index bounds stay strict");
    }
    for(unsigned n=0;n<10000;++n){vertices={};const float angle=float(n)*.071f;
        vertices[1].position[0]=1;vertices[1].position[1]=.1f+.4f*std::sin(angle);vertices[1].position[2]=.2f;
        vertices[2].position[0]=.3f;vertices[2].position[1]=1.1f;vertices[2].position[2]=.4f+.1f*std::cos(angle);
        const auto *x=vertices[1].position,*y=vertices[2].position;
        const float normal[]={x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0]};
        const float inverse=1/std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
        for(auto &v:vertices)for(unsigned j=0;j<3;++j)v.face_normal[j]=normal[j]*inverse;
        require(proof(),"curved-source unit directions use exactly the producer's multiply-by-inverse arithmetic");
        for(auto &v:vertices)v.face_normal[0]=std::nextafter(v.face_normal[0],2.f);
        require(!proof(),"a uniformly perturbed plane is not admitted through a normal tolerance");
    }
    {
        opaque_depth_fixture depth;
        for(float distance:{.081f,1.f,12.f,80.f})for(bool coplanar:{false,true}){
            depth.observe(distance,coplanar);
            const auto reason=ncmm_gpu::validate(depth.scene,&depth.world);
            require(!reason,reason?reason:"complete mixed-depth fixture contract");
            require(ncmm_gpu::validate(depth.scene)!=nullptr,"world camera mode requires its actual coordinate extension");
            require(ncmm_gpu::rigid_opaque_range(depth.mesh,1,3,3)&&!ncmm_gpu::rigid_opaque_range(depth.mesh,0,0,3),"mixed-depth fixture proves exactly its intended ordinary range");
            require(ncmm_gpu::winding_camera(depth.scene.camera)&&ncmm_gpu::winding_pose(depth.draw.transform),"fixture camera and pose qualify the actual specialized draw");
            require(depth.draw.bone_count==1&&ncmm_gpu::winding_bone(depth.bone),"the mixed-depth specialized draw has exactly the required identity root");
            require(depth.scene.camera.grid[3]==1&&(depth.cell.flags&NCMM_GFX_CEILING),"world ceiling semantics disable outdoor distance fog in the strict color control");
        }
        for(unsigned which=0;which<4;++which){
            depth.component_case(which);const auto reason=ncmm_gpu::validate(depth.scene,&depth.world);
            require(!reason,reason?reason:"component guard source contracts are valid without a device");
            require(ncmm_gpu::rigid_opaque_range(depth.mesh,1,3,3)==(which<2),"lower and fractional materials fail the immutable range proof");
            require((depth.draw.cell<depth.scene.cell_count)==(which!=0),"valid opaque sentinel cells separately fail the batch world-cell proof");
        }
        for(unsigned which=0;which<3;++which){depth.reflective_case(which);const auto reason=ncmm_gpu::validate(depth.scene,&depth.world);
            require(!reason,reason?reason:"reflective controls retain a valid owned world contract without a device");
            require(ncmm_gpu::rigid_opaque_range(depth.mesh,1,3,3)&&depth.draw.cell<depth.scene.cell_count,"all finish controls independently qualify ordinary component coverage");
            require(ncmm_gpu::reflective_component_finish(depth.materials[1].finish)==(which<2),"the ordinary non-reflective control cannot enter the new program");}
    }

    {
        const auto old_material=material;const auto saved_vertices=vertices;
        material={};material.flags=NCMM_GPU_SMOOTH;material.color[3]=.19f;
        const auto glass=[&]{return ncmm_gpu::rigid_glass_range(mesh,0,0,3);};
        require(glass(),"ordinary smooth finish-none translucent data proves the glass fragment");
        const auto ordinary=material;
        for(unsigned bit=0;bit<32;++bit){material.flags=ordinary.flags|(uint32_t(1)<<bit);
            require(glass()==((material.flags&~uint32_t(NCMM_GPU_SMOOTH|NCMM_GPU_EMISSIVE))==0),"all unproved coverage, lower, held, texture and picking bits keep the original transparent shader");}
        material=ordinary;
        for(float alpha:{0.f,.005f,.99f,.995f,1.f,std::numeric_limits<float>::quiet_NaN()}){material.color[3]=alpha;
            require(!glass(),"discarded, opaque and nonfinite alpha cannot enter glass specialization");}
        material=ordinary;for(unsigned finish=1;finish<=16;++finish){material.finish=finish;require(!glass(),"every actual material-detail finish keeps the general transparent shader");}
        material=ordinary;material.flags=0;require(!glass(),"flat shading is not silently replaced with interpolated smooth normals");
        material=ordinary;material.texture_key=1;require(!glass(),"a real color texture requires the original sampler");
        material=ordinary;material.normal_key=1;require(!glass(),"a normal texture cannot be omitted");
        material=ordinary;material.depth_key=1;require(!glass(),"a depth texture cannot be omitted");
        material=ordinary;material.liquid=1;require(!glass(),"liquid metadata stays on the general program");
        material=ordinary;material.banks=1;require(!glass(),"shore metadata stays on the general program");
        material=ordinary;vertices[2].joint=1;require(!glass(),"animated corner bindings retain their original path");vertices=saved_vertices;
        vertices[2].flags=NCMM_GPU_BILLBOARD;require(!glass(),"billboard positions never prove ordinary glass");vertices=saved_vertices;
        vertices[2].position[0]=std::numeric_limits<float>::infinity();require(!glass(),"nonfinite corners fail closed");vertices=saved_vertices;
        require(!ncmm_gpu::rigid_glass_range(mesh,0,1,3)&&!ncmm_gpu::rigid_glass_range(mesh,1,0,3),"transparent material/index bounds remain strict");
        material=old_material;
        glass_depth_fixture fixture;opaque_depth_fixture previous;
        for(unsigned which=0;which<10;++which){fixture.observe_glass(which);previous.observe(1.f,true);
            require(fixture.mesh.key!=previous.mesh.key,"independent fixtures cannot reuse an already resident immutable mesh ID");
            const auto reason=ncmm_gpu::validate(fixture.scene,&fixture.world);
            require(!reason,reason?reason:"ordinary glass owns a valid unchanged sparse-world contract");
            require(ncmm_gpu::rigid_glass_range(fixture.mesh,fixture.glass_slot,0,6)==false,"mixed material ranges cannot be treated as one ordinary glass range");
            require(ncmm_gpu::rigid_glass_range(fixture.mesh,fixture.glass_slot,fixture.glass_slot*3,3)==(which<6||which==8),"all source-bound finish/alpha/lower negative controls retain their general shader");
            require(ncmm_gpu::winding_bone(fixture.bone)==(which!=8),"an animated root separately rejects the transparent batch proof");}
    }

    {
        for(uint32_t finish=0;finish<=16;++finish)
            require(ncmm_gpu::plain_component_finish(finish)==(finish==0),"only exact finish zero can replace the ordinary detail uniform");
        require(!ncmm_gpu::plain_component_finish(UINT32_MAX),"unknown finishes keep the original component program");
        plain_depth_fixture fixture;glass_depth_fixture previous_glass;opaque_depth_fixture previous_depth;
        for(unsigned which=0;which<10;++which){fixture.observe_plain(which);previous_glass.observe_glass(which);previous_depth.observe(1.f,true);
            require(fixture.mesh.key!=previous_glass.mesh.key&&fixture.mesh.key!=previous_depth.mesh.key,"new immutable plain probes never alias old resident geometry");
            const auto reason=ncmm_gpu::validate(fixture.scene,&fixture.world);require(!reason,reason?reason:"owned plain opaque source contract");
            const auto slot=fixture.glass_slot;
            const bool eligible=ncmm_gpu::rigid_opaque_range(fixture.mesh,slot,slot*3,3)&&
                ncmm_gpu::plain_component_finish(fixture.materials[slot].finish)&&ncmm_gpu::winding_bone(fixture.bone);
            require(eligible==(which<6),"finish, lower light, root animation and fractional alpha keep every original fallback");
        }
    }
    std::printf("GPU winding: %u pure data/math checks passed; no SDL/device execution\n",checks);
}
