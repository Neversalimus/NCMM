#pragma once
#include <cstddef>
namespace first_person::props {
enum class id { bathroomCabinet, bathroomSink, bathtub, bedBunk, bedDouble, bedSingle, bench, bookcaseClosed, bookcaseOpen, books, cabinetBed, cardboardBoxClosed, cardboardBoxOpen, chair, chairDesk, coatRackStanding, computerScreen, desk, dryer, kitchenCabinet, kitchenCoffeeMachine, kitchenFridge, kitchenFridgeSmall, kitchenMicrowave, kitchenSink, kitchenStove, lampRoundFloor, lampRoundTable, laptop, loungeChair, loungeSofa, pottedPlant, radio, shower, sideTable, speaker, stoolBar, table, tableCoffee, toilet, trashcan, washer, televisionVintage, filingCabinet, count };
struct point {float x,y,z;};
struct triangle {point vertices[3];float color[4];};
struct model {const triangle *triangles;size_t count;point dimensions;};
const model &get(id value) noexcept;
}
