#include "hybrid_renderer.hpp"
#include <iostream>
#include <vector>
#include <cstring>
using namespace first_person::hybrid;
struct building {
    static constexpr int side=22, levels=4;
    ncmm_graphics_multilevel_scene_v1 w{};
    std::vector<ncmm_world_cell_v1> cells;
    std::vector<ncmm_graphics_cell_v1> appearance;
    std::vector<ncmm_graphics_cell_v2> detail;
    std::vector<ncmm_world_slice_v1> slices;
    int idx(int x,int y,int z)const{return z*side*side+y*side+x;}
    building(){
        for(int z=0;z<levels;z++){int first=int(cells.size());
            for(int y=0;y<side;y++)for(int x=0;x<side;x++){
                bool footprint=x>=8&&x<=16&&y>=5&&y<=16;
                bool floor=z==0||footprint;
                bool wall=footprint&&z<3&&(x==8||x==16||y==5||y==16);
                uint32_t flags=NCMM_WORLD_BODY_VISIBLE|(floor?NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM:0)|(wall?NCMM_WORLD_OPAQUE|NCMM_WORLD_STRUCTURE_VISIBLE:0);
                cells.push_back({x,y,z,flags,0,0,1,{.8f,.8f,.8f}});
                ncmm_graphics_cell_v1 a{};a.flags=NCMM_GFX_VISIBLE|(floor?0:NCMM_GFX_NO_FLOOR);a.light=1;
                std::strcpy(a.terrain,wall?"t_wall":"t_floor");appearance.push_back(a);
                ncmm_graphics_cell_v2 d{};d.terrain_shape=wall?NCMM_SHAPE_WALL:NCMM_SHAPE_FLAT;
                d.light_r=d.light_g=d.light_b=.8f;std::strcpy(d.ground,"t_floor");detail.push_back(d);
            }slices.push_back({z,uint32_t(first),uint32_t(cells.size()-first),0,uint64_t(z+1),1});
        }
        w.struct_size=sizeof(w);w.version=1;w.world_epoch=w.revision=1;w.origin_z=0;w.loaded_min_z=0;w.loaded_max_z=levels-1;w.level_height=NCMM_WORLD_LEVEL_HEIGHT;
        w.appearance.struct_size=sizeof(w.appearance);w.appearance.base={sizeof(ncmm_graphics_scene_v1),400,260,side,side,2.5f,10.5f,appearance.data(),appearance.size()};
        w.appearance.details=detail.data();w.appearance.detail_count=detail.size();w.appearance.environment.daylight=.8f;w.appearance.environment.sun_z=.7f;
        w.coordinates=cells.data();w.cell_count=uint32_t(cells.size());w.slices=slices.data();w.slice_count=uint32_t(slices.size());
        for(int z=0;z<3;z++)for(int y:{7,10,13}){int i=idx(8,y,z);detail[i].terrain_shape=NCMM_SHAPE_WINDOW_COVERED;detail[i].orientation=1;std::strcpy(appearance[i].terrain,"t_window_boarded");}
        for(int z=0;z<3;z++)for(int y:{8,11}){int i=idx(8,y,z);detail[i].terrain_shape=NCMM_SHAPE_DOOR_CLOSED;detail[i].orientation=1;std::strcpy(appearance[i].terrain,"t_door_c");}
        for(int y=5;y<=16;y++){int i=idx(8,y,3);detail[i].terrain_shape=NCMM_SHAPE_FENCE;detail[i].orientation=1;cells[i].flags|=NCMM_WORLD_STRUCTURE_VISIBLE;std::strcpy(appearance[i].terrain,"t_chainfence");}
    }
};
struct count_sink:geometry_sink {
    size_t count=0;std::vector<triangle> triangles;
    bool dirty(int)const override{return true;}
    void emit(const triangle &t)override{++count;triangles.push_back(t);}
    void skin(const first_person::creature::model &,const first_person::creature::skeleton_pose &,int,vec,int,vec,bool,bool,float,vec)override{}
    void impostor(const first_person::creature::asset &,const first_person::creature::pose_state &,vec,int,int,bool)override{}
};
int main(){
    building scene;const auto physical=scene.cells;const auto original=scene.appearance;
    auto query=[&](ncmm_world::voxel p){return p.x<0||p.x>=building::side||p.y<0||p.y>=building::side||p.z<0||p.z>=building::levels?uint32_t(NCMM_WORLD_UNKNOWN):physical[scene.idx(p.x,p.y,p.z)].flags;};
    render_options reference_options;reference_options.cull_geometry=false;
    for(bool corner:{false,true})for(float pitch:{0.f,.15f,.35f,.6f,.9f}){
        scene.cells=physical;scene.appearance=original;scene.w.coordinates=scene.cells.data();scene.w.appearance.base.cells=scene.appearance.data();
        scene.w.appearance.base.camera_y=corner?1.5f:10.5f;float yaw=corner?.65f:0.f;
        frame reference,filtered;build_frame(scene.w.appearance,yaw,pitch,{},reference,400,260,{},nullptr,nullptr,nullptr,reference_options,nullptr,&scene.w);
        const ncmm_world::point eye{2.5f,scene.w.appearance.base.camera_y,1.55f};
        for(size_t i=0;i<scene.cells.size();i++){
            auto &c=scene.cells[i];uint32_t flags=c.flags,permission=0;float level=c.z*NCMM_WORLD_LEVEL_HEIGHT;
            auto sides=ncmm_world::slab_trace_sides(flags,eye.z,level);
            bool body=false,top=false,bottom=false;
            for(auto xy:{std::array<float,2>{.5f,.5f},{.12f,.12f},{.88f,.12f},{.12f,.88f},{.88f,.88f}}){
                body|=ncmm_world::visible(eye,{c.x+xy[0],c.y+xy[1],level+1.25f},query);
                if(sides.top)top|=ncmm_world::visible(eye,{c.x+xy[0],c.y+xy[1],level+.003f},query,true);
                if(sides.bottom)bottom|=ncmm_world::visible(eye,{c.x+xy[0],c.y+xy[1],level-.083f},query,true);
            }
            if(body)permission|=NCMM_WORLD_BODY_VISIBLE;
            if(top)permission|=NCMM_WORLD_FLOOR_TOP;
            if(bottom)permission|=NCMM_WORLD_FLOOR_BOTTOM;
            if(ncmm_graphics_shapes::structural_surface(scene.detail[i].terrain_shape)&&(body||ncmm_world::facade_visible(eye,{c.x,c.y,c.z},query)))permission|=NCMM_WORLD_STRUCTURE_VISIBLE|ncmm_world::facade_slab_permissions(flags,eye.z,level);
            c.flags=(flags&~(NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM))|permission;
            if(!permission)scene.appearance[i].flags=0;
        }
        build_frame(scene.w.appearance,yaw,pitch,{},filtered,400,260,{},nullptr,nullptr,nullptr,{},nullptr,&scene.w);
        size_t missing=0,wrong=0;for(size_t p=0;p<reference.depth.size();p++){if(std::isfinite(reference.depth[p])&&!std::isfinite(filtered.depth[p]))missing++;if(std::isfinite(reference.depth[p])&&filtered.depth[p]>reference.depth[p]+.1f)wrong++;}
        if(missing||wrong){std::cerr<<"Facade geometry lost depth coverage after visibility filtering\n";return 1;}
        std::cout<<"corner="<<corner<<" pitch="<<pitch<<" sky_holes="<<missing<<" missing_near_surfaces="<<wrong<<"\n";
    }
    scene.cells=physical;scene.appearance=original;scene.w.coordinates=scene.cells.data();scene.w.appearance.base.cells=scene.appearance.data();
    count_sink full,pruned;frame statistics;
    renderer(scene.w.appearance,0,.15f,{},statistics,{},nullptr,reference_options,nullptr,&scene.w).collect(full,400,260);
    renderer(scene.w.appearance,0,.15f,{},statistics,{},nullptr,{},nullptr,&scene.w).collect(pruned,400,260);
    if(pruned.count>=full.count){std::cerr<<"Internal wall pruning did not remove shared geometry\n";return 1;}
    std::cout<<"Shared wall triangles "<<full.count<<" -> "<<pruned.count<<"\n";
    // A real slab rim and the wall beneath it must not cover the same
    // vertical patch: that made concrete/brick flicker as the camera turned.
    const int seam_wall=scene.idx(8,9,0),seam_slab=scene.idx(8,9,1);
    auto overlaps_seam=[&](const count_sink &mesh){
        for(const auto &a:mesh.triangles)if(a.m.cell==seam_wall&&a.normal.x<-.9f)
            for(const auto &b:mesh.triangles)if(b.m.cell==seam_slab&&b.normal.x<-.9f){
                float ay0=1e6f,ay1=-1e6f,az0=1e6f,az1=-1e6f,by0=1e6f,by1=-1e6f,bz0=1e6f,bz1=-1e6f;
                for(const auto &v:a.v){ay0=std::min(ay0,v.p.y);ay1=std::max(ay1,v.p.y);az0=std::min(az0,v.p.z);az1=std::max(az1,v.p.z);}
                for(const auto &v:b.v){by0=std::min(by0,v.p.y);by1=std::max(by1,v.p.y);bz0=std::min(bz0,v.p.z);bz1=std::max(bz1,v.p.z);}
                if(std::min(ay1,by1)>std::max(ay0,by0)+.001f&&std::min(az1,bz1)>std::max(az0,bz0)+.001f)return true;
            }return false;
    };
    if(overlaps_seam(pruned)){std::cerr<<"Slab rim overlaps the wall facade\n";return 1;}
    // Sparse neighbours and real stair openings cannot suppress an exposed rim.
    auto has_rim=[&](const count_sink &mesh){for(const auto &t:mesh.triangles)if(t.m.cell==seam_slab&&t.normal.x<-.9f)return true;return false;};
    scene.appearance[seam_wall].flags=0;count_sink sparse_rim;
    renderer(scene.w.appearance,0,.15f,{},statistics,{},nullptr,{},nullptr,&scene.w).collect(sparse_rim,400,260);
    if(!has_rim(sparse_rim)){std::cerr<<"Sparse wall hid the slab rim\n";return 1;}
    scene.appearance[seam_wall]=original[seam_wall];scene.cells[seam_wall].flags|=NCMM_WORLD_UP;count_sink stair_rim;
    renderer(scene.w.appearance,0,.15f,{},statistics,{},nullptr,{},nullptr,&scene.w).collect(stair_rim,400,260);
    if(!has_rim(stair_rim)){std::cerr<<"A stair opening hid the slab rim\n";return 1;}
    scene.cells[seam_wall]=physical[seam_wall];
    const int wall=scene.idx(8,9,1),transition=scene.idx(8,10,1);
    scene.detail[transition].terrain_shape=NCMM_SHAPE_WALL;scene.cells[transition].flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;
    count_sink transition_faces;renderer(scene.w.appearance,0,.15f,{},statistics,{},nullptr,{},nullptr,&scene.w).collect(transition_faces,400,260);
    bool shared_opening=false;for(const auto &tri:transition_faces.triangles)if(tri.m.cell==wall&&tri.normal.y>.9f)shared_opening=true;
    if(!shared_opening){std::cerr<<"A stair hint hid the adjacent wall face\n";return 1;}
    const int tree=scene.idx(4,10,2);scene.detail[tree].terrain_shape=NCMM_SHAPE_TREE;scene.cells[tree].flags=NCMM_WORLD_BODY_VISIBLE;std::strcpy(scene.appearance[tree].terrain,"t_tree");
    count_sink upper_tree;renderer(scene.w.appearance,0,.15f,{},statistics,{},nullptr,{},nullptr,&scene.w).collect(upper_tree,400,260);
    bool found_tree=false;for(const auto &tri:upper_tree.triangles)if(tri.m.cell==tree&&tri.m.cutout){found_tree=true;for(const auto &v:tri.v)if(v.p.z<2*NCMM_WORLD_LEVEL_HEIGHT+.74f){std::cerr<<"Upper-storey tree fell to ground level\n";return 1;}}
    if(!found_tree)return 1;

}
