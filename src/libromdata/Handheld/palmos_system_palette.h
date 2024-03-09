/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * palmos_system_palette.h: Palm OS system 8-bit color palette.            *
 *                                                                         *
 * Extracted from Palm-m505-4.0-en.rom.                                    *
 ***************************************************************************/

#pragma once

#include <stdint.h>

// NOTE: The original palette has an index value as the high byte.
// This has been changed to an opaque alpha channel. (0xFF)
static const uint32_t PalmOS_system_palette[256] =
{
	0xFFFFFFFF, 0xFFFFCCFF, 0xFFFF99FF, 0xFFFF66FF, 0xFFFF33FF, 0xFFFF00FF, 0xFFFFFFCC, 0xFFFFCCCC,
	0xFFFF99CC, 0xFFFF66CC, 0xFFFF33CC, 0xFFFF00CC, 0xFFFFFF99, 0xFFFFCC99, 0xFFFF9999, 0xFFFF6699,
	0xFFFF3399, 0xFFFF0099, 0xFFCCFFFF, 0xFFCCCCFF, 0xFFCC99FF, 0xFFCC66FF, 0xFFCC33FF, 0xFFCC00FF,
	0xFFCCFFCC, 0xFFCCCCCC, 0xFFCC99CC, 0xFFCC66CC, 0xFFCC33CC, 0xFFCC00CC, 0xFFCCFF99, 0xFFCCCC99,
	0xFFCC9999, 0xFFCC6699, 0xFFCC3399, 0xFFCC0099, 0xFF99FFFF, 0xFF99CCFF, 0xFF9999FF, 0xFF9966FF,
	0xFF9933FF, 0xFF9900FF, 0xFF99FFCC, 0xFF99CCCC, 0xFF9999CC, 0xFF9966CC, 0xFF9933CC, 0xFF9900CC,
	0xFF99FF99, 0xFF99CC99, 0xFF999999, 0xFF996699, 0xFF993399, 0xFF990099, 0xFF66FFFF, 0xFF66CCFF,
	0xFF6699FF, 0xFF6666FF, 0xFF6633FF, 0xFF6600FF, 0xFF66FFCC, 0xFF66CCCC, 0xFF6699CC, 0xFF6666CC,
	0xFF6633CC, 0xFF6600CC, 0xFF66FF99, 0xFF66CC99, 0xFF669999, 0xFF666699, 0xFF663399, 0xFF660099,
	0xFF33FFFF, 0xFF33CCFF, 0xFF3399FF, 0xFF3366FF, 0xFF3333FF, 0xFF3300FF, 0xFF33FFCC, 0xFF33CCCC,
	0xFF3399CC, 0xFF3366CC, 0xFF3333CC, 0xFF3300CC, 0xFF33FF99, 0xFF33CC99, 0xFF339999, 0xFF336699,
	0xFF333399, 0xFF330099, 0xFF00FFFF, 0xFF00CCFF, 0xFF0099FF, 0xFF0066FF, 0xFF0033FF, 0xFF0000FF,
	0xFF00FFCC, 0xFF00CCCC, 0xFF0099CC, 0xFF0066CC, 0xFF0033CC, 0xFF0000CC, 0xFF00FF99, 0xFF00CC99,
	0xFF009999, 0xFF006699, 0xFF003399, 0xFF000099, 0xFFFFFF66, 0xFFFFCC66, 0xFFFF9966, 0xFFFF6666,
	0xFFFF3366, 0xFFFF0066, 0xFFFFFF33, 0xFFFFCC33, 0xFFFF9933, 0xFFFF6633, 0xFFFF3333, 0xFFFF0033,
	0xFFFFFF00, 0xFFFFCC00, 0xFFFF9900, 0xFFFF6600, 0xFFFF3300, 0xFFFF0000, 0xFFCCFF66, 0xFFCCCC66,
	0xFFCC9966, 0xFFCC6666, 0xFFCC3366, 0xFFCC0066, 0xFFCCFF33, 0xFFCCCC33, 0xFFCC9933, 0xFFCC6633,
	0xFFCC3333, 0xFFCC0033, 0xFFCCFF00, 0xFFCCCC00, 0xFFCC9900, 0xFFCC6600, 0xFFCC3300, 0xFFCC0000,
	0xFF99FF66, 0xFF99CC66, 0xFF999966, 0xFF996666, 0xFF993366, 0xFF990066, 0xFF99FF33, 0xFF99CC33,
	0xFF999933, 0xFF996633, 0xFF993333, 0xFF990033, 0xFF99FF00, 0xFF99CC00, 0xFF999900, 0xFF996600,
	0xFF993300, 0xFF990000, 0xFF66FF66, 0xFF66CC66, 0xFF669966, 0xFF666666, 0xFF663366, 0xFF660066,
	0xFF66FF33, 0xFF66CC33, 0xFF669933, 0xFF666633, 0xFF663333, 0xFF660033, 0xFF66FF00, 0xFF66CC00,
	0xFF669900, 0xFF666600, 0xFF663300, 0xFF660000, 0xFF33FF66, 0xFF33CC66, 0xFF339966, 0xFF336666,
	0xFF333366, 0xFF330066, 0xFF33FF33, 0xFF33CC33, 0xFF339933, 0xFF336633, 0xFF333333, 0xFF330033,
	0xFF33FF00, 0xFF33CC00, 0xFF339900, 0xFF336600, 0xFF333300, 0xFF330000, 0xFF00FF66, 0xFF00CC66,
	0xFF009966, 0xFF006666, 0xFF003366, 0xFF000066, 0xFF00FF33, 0xFF00CC33, 0xFF009933, 0xFF006633,
	0xFF003333, 0xFF000033, 0xFF00FF00, 0xFF00CC00, 0xFF009900, 0xFF006600, 0xFF003300, 0xFF111111,
	0xFF222222, 0xFF444444, 0xFF555555, 0xFF777777, 0xFF888888, 0xFFAAAAAA, 0xFFBBBBBB, 0xFFDDDDDD,
	0xFFEEEEEE, 0xFFC0C0C0, 0xFF800000, 0xFF800080, 0xFF008000, 0xFF008080, 0xFF000000, 0xFF000000,
	0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
	0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
	0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000
};
