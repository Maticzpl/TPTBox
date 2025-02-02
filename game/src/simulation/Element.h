#ifndef ELEMENT_H
#define ELEMENT_H

#include <string>
#include "stdint.h"

#include "../util/types/color.h"
#include "../util/types/bitset8.h"
#include "ElementDefs.h"

class Element {
public:
	ElementState State;
    uint32_t Properties;

	bool Enabled;            // If enabled = false element cannot be created
	uint32_t Causality;      // Radius of causality, 0 = only this part, 1 = moore neighborhood, etc...
	float Advection;         // How much particle is accelerated by moving air, generally -1 to 1
	float AirDrag;           // How much air particle produces in direction of travel
	float AirLoss;           // How much moving air is slowed down by the particle, 1 = no effect, 0 = instantly stops
	float Loss;              // Velocity multiplier per frame
	float Collision;         // Velocity multiplier upon collision
	float Gravity;           // How much particle is affected by gravity
	float NewtonianGravity;  // How much particle is affected by newtonian gravity
	float Diffusion;         // How much particle wiggles
	float HotAir;            // How much particle increases pressure by

	int Hardness;            // How much its affected by ACID, 0 = no effect, higher = more effect

	int Weight;
	unsigned char HeatConduct;
	unsigned int LatentHeat;

	float LowPressure;
	int LowPressureTransition;
	float HighPressure;
	int HighPressureTransition;
	float LowTemperature;
	int LowTemperatureTransition;
	float HighTemperature;
	int HighTemperatureTransition;

	int (*Update)(UPDATE_FUNC_ARGS);
	int (*Graphics)(GRAPHICS_FUNC_ARGS);

    // TODO: create function, create allowed, change type, ctype draw
    // todo: default properties

	RGBA Color;
	util::Bitset8 GraphicsFlags;
    std::string Identifier;  // ID string, like METL
	std::string Name;        // Display name, like METL
    std::string Description; // Menu description

	bool MenuVisible;        // Visible in menu if true
	int MenuSection;  


    Element();
	Element(const Element&) = delete;
	Element& operator=(const Element&) = delete;

    // Define void Element_NAME(); for each element
    #define ELEMENT_NUMBERS_DECLARE
    #include "ElementNumbers.h"
    #undef ELEMENT_NUMBERS_DECLARE
};

#endif