// Generated offline; see source/License.txt (Kenney, CC0).
#pragma once
#include <cstddef>
namespace first_person::vehicle_assets {
enum class id { road_wheel, truck_wheel };
struct vertex {float position[3],normal[3];};
struct triangle {vertex vertices[3];float color[4];};
struct mesh {const triangle *triangles;std::size_t count;};
mesh get(id component);
}
