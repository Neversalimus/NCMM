#pragma once
#include "viewmodel_animation.hpp"
#include "animation.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace first_person::viewmodel {
struct region_style {point color{1,1,1};uint32_t finish=0;bool textured=true,glove=false,recolor=false;};
struct appearance {std::array<region_style,2> regions;uint32_t features=0;};
struct anchors {point primary,secondary,trigger,item_pivot,sight;};
struct texture_pixels {const uint32_t *data=nullptr;std::vector<uint32_t> owned;};
// A bounded appearance cache retains authored detail while adapting the actual
// garment/mutation colour. CPU and GPU consume exactly these same pixels.
std::shared_ptr<const texture_pixels> texture_for(const region_style &style);
void clear_textures() noexcept;
struct attachment_vertex {point position,normal;uint16_t node;};
struct attachments {std::vector<attachment_vertex> vertices;std::vector<uint32_t> indices;};
const attachments &mutation_attachments(uint32_t side,uint32_t features);
struct hand_pose {
    std::array<matrix,maximum_palettes> palette{};
    std::array<matrix,maximum_nodes> joints{};
    point grip,wrist,elbow,shoulder;
    std::array<uint16_t,maximum_palettes> palette_index{};
    std::array<uint16_t,maximum_nodes> node_palette{};
    uint32_t side=0,palette_count=0;
};
struct submission {
    const asset &geometry;
    const hand_pose &pose;
    matrix world;
    appearance style;
};
// One bounded, allocation-free preparation shared by CPU and GPU. Grip points
// come from the same physical weapon geometry used by the held item renderer.
class frame {
    friend class animator;
    const asset &source;
    std::array<local_pose,maximum_nodes> local{};
    std::array<matrix,maximum_nodes> global{};
    std::array<point,2> idle_wrists{};
    std::array<matrix,2> grip_bases{};
    std::array<int,2> hand{},forearm{},upper_arm{};
    bool ready=false,empty=false,gun=false;
public:
    frame(const animation::pose &action,bool no_item,bool firearm) noexcept;
    // Optional bottom camera plane in attachment space, with unit XY normal.
    bool attach(point grip,bool left,float lower_view_slope,hand_pose &out,const point *trigger=nullptr,
                const std::array<float,4> *lower_plane=nullptr,const point *grip_axis=nullptr)const noexcept;
    const asset &geometry()const noexcept{return source;}
};
// One fixed-size local-pose snapshot makes even repeated interruptions
// continuous. It never retains a scene/atlas pointer or an unbounded history.
class animator {
    std::optional<frame> current;
    std::array<local_pose,maximum_nodes> from{};
    double started=-1;
    uint64_t serial=0;
public:
    void reset()noexcept {current.reset();started=-1;serial=0;}
    const frame &prepare(const animation::pose &action,bool no_item,bool firearm)noexcept;
};
}
