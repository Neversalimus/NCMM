#pragma once
#include "held_selection.hpp"
#include "animation.hpp"
#include "viewmodel.hpp"
namespace first_person::held {
struct posed {
    const model *geometry=nullptr;viewmodel::pose_buffer bones;viewmodel::matrix local=viewmodel::identity();bool magazine=true,round=false;
    viewmodel::point mount{};float riser=0;
    bool visible(part kind)const noexcept {return (kind!=part::magazine||magazine)&&(kind!=part::round||round);}
    uint32_t static_builds=0,static_reuses=0,draws=0,palette_matrices=0;
};
// Allocated once by the module. Every entry owns its evaluated matrices; only
// immutable compiled author tables are referenced across renderer callbacks.
struct prepared {
    selection selected;std::array<posed,5> entries;std::array<viewmodel::hand_pose,2> hands;uint32_t count=0;
    void reset() noexcept {selected={};count=0;}
    bool prepare(const ncmm_held_item_v1 &item,const animation::pose &action) noexcept {
        reset();selected=select(item);if(!selected.body)return false;
        auto &body=entries[0];body.geometry=selected.body;body.magazine=selected.magazine;body.round=(item.flags&NCMM_HELD_AMMO_LOADED)!=0;
        body.static_builds=body.static_reuses=body.draws=body.palette_matrices=0;body.riser=0;
        if(!viewmodel::bind_pose(body.geometry->geometry,body.bones)){selected.body=nullptr;selected.reason=failure::invalid_metadata;return false;}
        const auto &model=*body.geometry;
        // There are no invented reload/use clips. Authored action nodes move
        // only for a confirmed native shot of this currently equipped item.
        if(action.attack_serial&&action.attack_kind==2&&action.attack_phase>=0&&action.attack_phase<1){
            const float pulse=std::sin(std::min(1.f,action.attack_phase/.45f)*3.14159265f);
            if(model.slide_node>=0&&!(item.flags&NCMM_HELD_MANUAL_ACTION)){
                auto &pose=body.bones.local[size_t(model.slide_node)];pose.translation=pose.translation+model.slide_translation*std::max(0.f,pulse);}
            if(model.trigger_node>=0){auto &pose=body.bones.local[size_t(model.trigger_node)];
                const float angle=.12f*std::max(0.f,pulse);const viewmodel::quaternion turn{std::sin(angle*.5f),0,0,std::cos(angle*.5f)};
                const auto q=pose.rotation;pose.rotation={q.w*turn.x+q.x*turn.w,q.y*turn.w+q.z*turn.x,q.z*turn.w-q.y*turn.x,q.w*turn.w-q.x*turn.x};}
            if(!viewmodel::evaluate(model.geometry,body.bones)){selected.body=nullptr;selected.reason=failure::invalid_metadata;return false;}
        }
        viewmodel::point grip=uint32_t(model.kind)<=uint32_t(family::auto_shotgun)?viewmodel::point{.185f,-.267f,.67f}:viewmodel::point{.27f,-.28f,.84f};
        // A long tool's second handle must remain inside the same arm reach,
        // rather than pushing its support wrist beyond the shoulder span.
        if(model.two_handed&&uint32_t(model.kind)>uint32_t(family::auto_shotgun))grip={.25f,-.30f,.65f};
        // These real handles sit above their body. A generic low tool grip
        // placed almost the entire container outside the frame. Reposition the
        // intact model and solve the same bounded PSX arm rig; never stretch it.
        if(model.kind==family::medical)grip={.27f,-.06f,.65f};
        if(model.kind==family::gas_can)grip={.23f,.08f,.62f};
        body.local=viewmodel::transform({grip,{}, {selected.scale,selected.scale,selected.scale}});count=1;
        for(size_t i=0;i<selected.attachment_count;++i){const auto &source=selected.attachments[i];auto &out=entries[count];out.geometry=source.geometry;out.magazine=true;out.round=false;
            out.static_builds=out.static_reuses=out.draws=out.palette_matrices=0;out.riser=0;
            if(!viewmodel::bind_pose(out.geometry->geometry,out.bones)){selected.body=nullptr;count=0;selected.reason=failure::invalid_metadata;return false;}
            auto socket=viewmodel::multiply(body.local,body.bones.global[size_t(source.socket)]);
            // Socket orientation belongs to the author's rig. Its scale is
            // removed before mounting an independently dimensioned accessory.
            for(size_t column=0;column<3;++column){viewmodel::point axis{socket.columns[column*4],socket.columns[column*4+1],socket.columns[column*4+2]};axis=viewmodel::unit(axis);
                socket.columns[column*4]=axis.x;socket.columns[column*4+1]=axis.y;socket.columns[column*4+2]=axis.z;}
            const auto &asset=out.geometry->geometry;const auto &root=asset.nodes[asset.order[0]].bind;
            viewmodel::matrix undo_rotation{};
            if(!viewmodel::inverse(viewmodel::transform({{},root.rotation,{1,1,1}}),undo_rotation)){selected.body=nullptr;count=0;return false;}
            const auto &pivot=out.geometry->grips.pivot;
            const auto at=viewmodel::position(out.bones.global[pivot.node],pivot.point);
            // The GLB gun sockets are in the exported bone frame, whereas
            // the separate accessory meshes use GLB world axes. Retain the
            // socket pose and apply the declared rigid mating conversion;
            // cancelling mesh normalization alone turned the grip sideways.
            const auto mating=viewmodel::transform({{},out.geometry->mount_rotation,{1,1,1}});
            out.local=viewmodel::multiply(socket,viewmodel::multiply(mating,viewmodel::multiply(undo_rotation,
                viewmodel::transform({{}, {}, {source.scale,source.scale,source.scale}}))));
            if(out.geometry->kind==family::optic){
                // The author's gun scope sockets disagree about their local
                // bone axes. The imported sight is already normalized to the
                // barrel's +Z / upright +Y frame; retain the actual socket
                // position without rotating its bore sideways on an MP5.
                out.local=viewmodel::transform({{socket.columns[12],socket.columns[13],socket.columns[14]}, {}, {source.scale,source.scale,source.scale}});
            }
            const auto offset=viewmodel::direction(out.local,at);out.local.columns[12]-=offset.x;out.local.columns[13]-=offset.y;out.local.columns[14]-=offset.z;++count;
            if(out.geometry->kind==family::optic){
                out.mount={out.local.columns[12],out.local.columns[13],out.local.columns[14]};
                const auto &c=out.geometry->grips.sight;
                const auto sight=viewmodel::position(out.local,viewmodel::position(out.bones.global[c.node],c.point));
                float obstruction=sight.y-.008f;
                const auto &body_mesh=body.geometry->geometry;
                for(size_t n=0;n<body_mesh.vertex_count;++n){const auto &v=body_mesh.vertices[n];
                    const auto p=viewmodel::position(body.local,viewmodel::position(body.bones.palette[v.palette],v.position));
                    if(p.z>sight.z&&std::abs(p.x-sight.x)<.03f)obstruction=std::max(obstruction,p.y);}
                // Low authored sockets need a visible physical riser so the
                // receiver/iron hood cannot completely cover the optic bore.
                out.riser=std::clamp(obstruction+.008f-sight.y,0.f,.12f);out.local.columns[13]+=out.riser;
            }
        }
        return true;
    }
};
struct submission {posed &value;viewmodel::matrix world;};
}
