#include "../ElementClasses.h"

void Element::Element_PHOT() {
    Name = "PHOT";
    State = ElementState::TYPE_ENERGY;
    Color = 0xFFFFFFFF;
    GraphicsFlags = GraphicsFlags::GLOW | GraphicsFlags::NO_LIGHTING;

    Collision = -1.0f;
};
