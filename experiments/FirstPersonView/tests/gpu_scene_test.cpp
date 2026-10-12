#include "ncmm_gpu_backend.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(int argc,char **argv){
    auto require=[](bool ok,const char *why){if(!ok){std::fprintf(stderr,"Full GPU scene failed: %s (%s)\n",why,SDL_GetError());std::exit(1);}};
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM GPU scene/skin qualification",640,480,SDL_WINDOW_HIDDEN);require(window!=nullptr,"native window");
#endif
    auto *device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,false,nullptr);require(device!=nullptr,"real GPU API device; fallback prohibited");
    auto *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"shared SDL renderer");
    auto *ui=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_TARGET,640,480);require(ui&&SDL_SetRenderTarget(renderer,ui),"SDL UI target");
    ncmm_gpu::backend engine;require(engine.bind(renderer,1),"borrow existing device");
    ncmm_gpu_scene_v1 scene{};scene.struct_size=sizeof(scene);scene.width=640;scene.height=480;scene.mode=NCMM_GPU_FORCE;scene.frame_serial=1;
    auto &camera=scene.camera;camera.right[0]=1;camera.up[2]=1;camera.forward[1]=1;camera.projection[0]=640;camera.projection[1]=480;camera.projection[2]=457;camera.projection[3]=512;
    camera.sun[2]=1;camera.sun[3]=1;camera.sky_top[2]=.4f;camera.sky_bottom[0]=.3f;camera.sky_bottom[1]=.4f;camera.sky_bottom[2]=.5f;camera.grid[0]=camera.grid[1]=camera.grid[2]=1;
    ncmm_gpu_cell_v1 cell{};cell.flags=1;cell.light[0]=cell.light[1]=cell.light[2]=1;scene.cells=&cell;scene.cell_count=1;
    std::array<ncmm_gpu_vertex_v1,6> vertices{};for(int i=0;i<6;++i){float z=i<3?2.f:4.f;int corner=i%3;auto &v=vertices[i];
        v.position[0]=(corner==0?-1.f:corner==1?1.f:0.f)*z;v.position[1]=z;v.position[2]=(corner==2?1.f:-1.f)*z;
        v.normal[1]=v.face_normal[1]=-1;v.material=i<3?0:1;v.joint=i<3?1:0;}
    const uint32_t indices[]={0,1,2,3,4,5};ncmm_gpu_material_v1 materials[2]{};materials[0].color[0]=.9f;materials[0].color[3]=1;materials[0].flags=NCMM_GPU_CREATURE|NCMM_GPU_CREATURE_VISUAL;
    materials[1].color[1]=.9f;materials[1].color[3]=1;
    ncmm_gpu_mesh_v1 mesh{11,vertices.data(),indices,materials,6,6,2,0};scene.meshes=&mesh;scene.mesh_count=1;
    ncmm_gpu_matrix_v1 bones[2]{};for(auto &b:bones)for(int i=0;i<4;++i)b.columns[i*5]=1;scene.bones=bones;scene.bone_count=2;
    ncmm_gpu_draw_v1 draw{};draw.mesh_key=11;draw.transform=bones[0];draw.bone_count=2;scene.draws=&draw;scene.draw_count=1;
    require(engine.submit(scene),engine.metrics().reason);auto first=engine.metrics();require(first.active&&first.triangles==2&&first.cpu_raster_calls==0,"GPU triangles, hardware depth, CPU raster excluded");
    require(engine.has(11,0),"persistent GPU mesh resource");require(first.target_cell==0,"geometric picking matches visible near creature");
    auto capture=[&](const char *suffix){require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"GPU-to-GPU final composition");
        SDL_FRect strip{0,0,640,20};require(SDL_SetRenderDrawColor(renderer,0,0,255,255)&&SDL_RenderFillRect(renderer,&strip),"ordinary SDL UI");
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"test-only readback");auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"capture format");
        if(argc>1)require(SDL_SaveBMP(image,(std::string(argv[1])+suffix+".bmp").c_str()),"save real GPU scene");
        auto center=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+240*image->pitch)[320];
        auto top=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+10*image->pitch)[320];require(((top>>8)&255)>240,"SDL UI preserved");SDL_DestroySurface(image);return center;};
    auto center=capture("-near");require((center>>24)>120&&((center>>16)&255)<20,"near red wins over later far green in full pipeline");
    auto completed=engine.metrics();require(completed.completed_frame_serial>=1&&completed.visible_creature_pixels>1000,"fenced small GPU visibility counter");
    scene.mesh_count=0;scene.meshes=nullptr;scene.frame_serial=2;bones[1].columns[12]=5;
    require(engine.submit(scene),engine.metrics().reason);auto second=engine.metrics();require(second.active&&second.generated_meshes==0&&second.upload_bytes<1024,"only dynamic data uploaded for skeletal animation");
    require(second.target_cell==-1,"bone animation updates exact picking without CPU pixel raster");center=capture("-skin");require(((center>>16)&255)>120&&(center>>24)<20,"GPU vertex skinning moves near mesh; far green becomes visible");
    require(engine.metrics().completed_frame_serial>=2,"animated frame fence completes without a main-path wait");
    scene.mode=NCMM_GPU_CPU;require(!engine.submit(scene)&&!engine.metrics().active,"explicit CPU selection never impersonates GPU");
    scene.mode=NCMM_GPU_FORCE;scene.frame_serial=3;bones[1].columns[12]=0;require(engine.submit(scene),engine.metrics().reason);require(engine.metrics().generated_meshes==0,"GPU resumes with cached geometry");
    std::array<ncmm_gpu_vertex_v1,3> glass_vertices{};for(int i=0;i<3;++i){glass_vertices[i]=vertices[i];glass_vertices[i].position[0]*=.5f;glass_vertices[i].position[1]=1;glass_vertices[i].position[2]*=.5f;glass_vertices[i].joint=0;glass_vertices[i].material=0;}
    ncmm_gpu_material_v1 glass{};glass.color[2]=.9f;glass.color[3]=.3f;const uint32_t glass_indices[]={0,1,2};
    ncmm_gpu_mesh_v1 dynamic{12,glass_vertices.data(),glass_indices,&glass,3,3,1,NCMM_GPU_DYNAMIC_MESH};
    ncmm_gpu_draw_v1 draws[2]={draw,draw};draws[1].mesh_key=12;draws[1].bone_count=1;draws[1].pass=NCMM_GPU_TRANSPARENT;
    scene.meshes=&dynamic;scene.mesh_count=1;scene.draws=draws;scene.draw_count=2;scene.frame_serial=4;
    require(engine.submit(scene),engine.metrics().reason);require(!engine.has(12,0),"dynamic mesh explicitly requests per-frame data");
    center=capture("-glass");require((center>>24)>80&&(center>>24)<135&&((center>>8)&255)>30&&((center>>8)&255)<75,"transparent glass blends over opaque creature with depth preserved");
    require(engine.metrics().target_cell==0,"transparent glass does not erase target ownership");
    for(auto &vertex:glass_vertices)vertex.position[0]+=5;scene.frame_serial=5;require(engine.submit(scene),engine.metrics().reason);
    auto dynamic_stats=engine.metrics();require(dynamic_stats.dynamic_mesh_updates==1&&dynamic_stats.generated_meshes==0,"dynamic buffer cycles without recreating world geometry");
    center=capture("-dynamic");require((center>>24)>120&&((center>>8)&255)<20,"updated dynamic buffer moves transparent geometry");
    if(argc>1){std::ofstream out(std::string(argv[1])+".json");out<<"{\"success\":true,\"full_gpu_pipeline\":true,\"hardware_depth\":true,\"gpu_skinning\":true,\"cpu_raster_calls\":0,\"animation_upload_bytes\":"<<second.upload_bytes<<",\"resource_bytes\":"<<second.resource_bytes<<",\"software_device\":"<<second.software_device<<",\"backend\":\""<<second.backend<<"\",\"device\":\""<<second.device<<"\"}\n";require(bool(out),"device evidence");}
    std::printf("Full GPU scene PASS: %s / %s; animation upload %llu bytes; CPU raster=0\n",second.backend,second.device,(unsigned long long)second.upload_bytes);
    engine.clear();SDL_DestroyTexture(ui);SDL_DestroyRenderer(renderer);SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();
}
