/*
	Copyright (c) 2022 ByteBit/xtreme8000

	This file is part of CavEX.

	CavEX is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	CavEX is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with CavEX.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef GFX_SETTINGS_H
#define GFX_SETTINGS_H

/*
	This file is used to set video settings at compile time.
	To disable a setting, comment its line out and recompile.
	To enable a setting, uncomment its line and recompile.
	Some settings cannot be disabled or enabled, as they are a number (like resolution or GUI scale).
*/

//Render clouds
#define GFX_CLOUDS

//Render double-sided faces (tall grass, sugar cane and similar blocks)
#define GFX_DOUBLESIDED

//Transparent and animated liquids (can heavily impact FPS on low-end GPUs)
#define GFX_FANCY_LIQUIDS

//fancy 3d elements
#define GFX_3D_ELEMENTS

//Scale HUD and GUIs (the higher the number, the bigger the GUI).
//For 640x480, setting this to 2 is recommended.
//For 320x240, setting this to 1 is recommended.
//Do not set this to anything less to 1.
#define GFX_GUI_SCALE 2

//PC only: default window width and height (rendering resolution)
#define GFX_PC_WINDOW_WIDTH 640
#define GFX_PC_WINDOW_HEIGHT 400

// TODO: 240p on Wii?

//PC only: render polygons as wireframes (will break text and texture rendering, for testing purposes only)
//#define GFX_WIREFRAME

#endif

