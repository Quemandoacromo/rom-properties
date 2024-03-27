/***************************************************************************
 * ROM Properties Page shell extension. (librptexture)                     *
 * GLenumStrings.cpp: OpenGL string tables.                                *
 *                                                                         *
 * Copyright (c) 2016-2024 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "GLenumStrings.hpp"
#include "fileformat/gl_defs.h"

// C++ includes
#include <limits>
using std::array;

namespace LibRpTexture { namespace GLenumStrings {

// String tables.
// NOTE: glEnum_offtbl[] must be sorted by id.
// NOTE: Leaving the "GL_" prefix off of the strings.
struct OffTbl_t {
	uint16_t id;
	uint16_t offset;
};

/**
 * OpenGL enumerations.
 */
static constexpr char glEnum_strtbl[] =
	"\0"

	"BYTE\0"
	"UNSIGNED_BYTE\0"
	"SHORT\0"
	"UNSIGNED_SHORT\0"
	"INT\0"
	"UNSIGNED_INT\0"
	"FLOAT\0"
	"HALF_FLOAT\0"
	"STENCIL_INDEX\0"
	"DEPTH_COMPONENT\0"
	"RED\0"
	"GREEN\0"
	"BLUE\0"
	"RGB\0"
	"RGBA\0"
	"LUMINANCE\0"
	"LUMINANCE_ALPHA\0"
	"R3_G3_B2\0"
	"UNSIGNED_BYTE_3_3_2\0"
	"UNSIGNED_SHORT_4_4_4_4\0"
	"UNSIGNED_SHORT_5_5_5_1\0"
	"UNSIGNED_INT_8_8_8_8\0"
	"UNSIGNED_INT_10_10_10_2\0"
	"LUMINANCE4\0"
	"LUMINANCE8\0"
	"LUMINANCE12\0"
	"LUMINANCE16\0"
	"LUMINANCE4_ALPHA4\0"
	"LUMINANCE6_ALPHA2\0"
	"LUMINANCE8_ALPHA8\0"
	"LUMINANCE12_ALPHA4\0"
	"LUMINANCE12_ALPHA12\0"
	"LUMINANCE16_ALPHA16\0"
	"INTENSITY\0"
	"INTENSITY4\0"
	"INTENSITY8\0"
	"INTENSITY12\0"
	"INTENSITY16\0"
	"RGB4\0"
	"RGB5\0"
	"RGB8\0"
	"RGB10\0"
	"RGB12\0"
	"RGB16\0"
	"RGBA2\0"
	"RGBA4\0"
	"RGB5_A1\0"
	"RGBA8\0"
	"RGB10_A2\0"
	"RGBA12\0"
	"RGBA16\0"
	"BGR\0"
	"BGRA\0"
	"DEPTH_COMPONENT16\0"
	"DEPTH_COMPONENT24\0"
	"DEPTH_COMPONENT32\0"
	"COMPRESSED_RED\0"
	"COMPRESSED_RG\0"
	"RG\0"
	"RG_INTEGER\0"
	"R8\0"
	"R16\0"
	"RG8\0"
	"RG16\0"
	"R16F\0"
	"R32F\0"
	"RG16F\0"
	"RG32F\0"
	"R8I\0"
	"R8UI\0"
	"R16I\0"
	"R16UI\0"
	"R32I\0"
	"R32UI\0"
	"RG8I\0"
	"RG8UI\0"
	"RG16I\0"
	"RG16UI\0"
	"RG32I\0"
	"RG32UI\0"
	"UNSIGNED_BYTE_2_3_3_REV\0"
	"UNSIGNED_SHORT_5_6_5\0"
	"UNSIGNED_SHORT_5_6_5_REV\0"
	"UNSIGNED_SHORT_4_4_4_4_REV\0"
	"UNSIGNED_SHORT_1_5_5_5_REV\0"
	"UNSIGNED_INT_8_8_8_8_REV\0"
	"UNSIGNED_INT_2_10_10_10_REV\0"
	"RGB_S3TC\0"
	"RGB4_S3TC\0"
	"RGBA_S3TC\0"
	"RGBA4_S3TC\0"
	"RGBA_DXT5_S3TC\0"
	"RGBA4_DXT5_S3TC\0"
	"COMPRESSED_RGB_S3TC_DXT1_EXT\0"
	"COMPRESSED_RGBA_S3TC_DXT1_EXT\0"
	"COMPRESSED_RGBA_S3TC_DXT3_EXT\0"
	"COMPRESSED_RGBA_S3TC_DXT5_EXT\0"
	"COMPRESSED_ALPHA\0"
	"COMPRESSED_LUMINANCE\0"
	"COMPRESSED_LUMINANCE_ALPHA\0"
	"COMPRESSED_INTENSITY\0"
	"COMPRESSED_RGB\0"
	"COMPRESSED_RGBA\0"
	"DEPTH_STENCIL\0"
	"UNSIGNED_INT_24_8\0"
	"RGBA32F\0"
	"RGB32F\0"
	"RGBA16F\0"
	"RGB16F\0"
	"DEPTH24_STENCIL8\0"

	// PVRTC
	"COMPRESSED_RGB_PVRTC_4BPPV1_IMG\0"
	"COMPRESSED_RGB_PVRTC_2BPPV1_IMG\0"
	"COMPRESSED_RGBA_PVRTC_4BPPV1_IMG\0"
	"COMPRESSED_RGBA_PVRTC_2BPPV1_IMG\0"

	"R11F_G11F_B10F\0"
	"UNSIGNED_INT_10F_11F_11F_REV\0"
	"RGB9_E5\0"
	"UNSIGNED_INT_5_9_9_9_REV\0"
	"SRGB\0"
	"SRGB8\0"
	"SRGB_ALPHA\0"
	"SRGB8_ALPHA8\0"
	"SLUMINANCE_ALPHA\0"
	"SLUMINANCE8_ALPHA8\0"
	"SLUMINANCE\0"
	"SLUMINANCE8\0"
	"COMPRESSED_SRGB\0"
	"COMPRESSED_SRGB_ALPHA\0"
	"COMPRESSED_SLUMINANCE\0"
	"COMPRESSED_SLUMINANCE_ALPHA\0"

	// GL_EXT_texture_compression_latc
	"COMPRESSED_LUMINANCE_LATC1_EXT\0"
	"COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT\0"
	"COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT\0"
	"COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT\0"

	"DEPTH_COMPONENT32F\0"
	"DEPTH32F_STENCIL8\0"
	"STENCIL_INDEX1\0"
	"STENCIL_INDEX4\0"
	"STENCIL_INDEX8\0"
	"STENCIL_INDEX16\0"
	"RGB565\0"
	"ETC1_RGB8_OES\0"
	"RGBA32UI\0"
	"RGB32UI\0"
	"RGBA16UI\0"
	"RGB16UI\0"
	"RGBA8UI\0"
	"RGB8UI\0"
	"RGBA32I\0"
	"RGB32I\0"
	"RGBA16I\0"
	"RGB16I\0"
	"RGBA8I\0"
	"RGB8I\0"
	"RED_INTEGER\0"
	"GREEN_INTEGER\0"
	"BLUE_INTEGER\0"
	"ALPHA_INTEGER\0"
	"RGB_INTEGER\0"
	"RGBA_INTEGER\0"
	"BGR_INTEGER\0"
	"BGRA_INTEGER\0"
	"INT_2_10_10_10_REV\0"
	"FLOAT_32_UNSIGNED_INT_24_8_REV\0"
	"COMPRESSED_RED_RGTC1\0"
	"COMPRESSED_SIGNED_RED_RGTC1\0"
	"COMPRESSED_RG_RGTC2\0"
	"COMPRESSED_SIGNED_RG_RGTC2\0"
	"COMPRESSED_RGBA_BPTC_UNORM\0"
	"COMPRESSED_SRGB_ALPHA_BPTC_UNORM\0"
	"COMPRESSED_RGB_BPTC_SIGNED_FLOAT\0"
	"COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT\0"
	"R8_SNORM\0"
	"RG8_SNORM\0"
	"RGB8_SNORM\0"
	"RGBA8_SNORM\0"
	"R16_SNORM\0"
	"RG16_SNORM\0"
	"RGB16_SNORM\0"
	"RGBA16_SNORM\0"
	"RGB10_A2UI\0"

	// PVRTC-II
	"COMPRESSED_RGBA_PVRTC_2BPPV2_IMG\0"
	"COMPRESSED_RGBA_PVRTC_4BPPV2_IMG\0"
	"COMPRESSED_SRGB_PVRTC_2BPPV1_EXT\0"
	"COMPRESSED_SRGB_PVRTC_4BPPV1_EXT\0"
	"COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT\0"
	"COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT\0"

	// ETC2
	"COMPRESSED_R11_EAC\0"
	"COMPRESSED_SIGNED_R11_EAC\0"
	"COMPRESSED_RG11_EAC\0"
	"COMPRESSED_SIGNED_RG11_EAC\0"
	"COMPRESSED_RGB8_ETC2\0"
	"COMPRESSED_SRGB8_ETC2\0"
	"COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2\0"
	"COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2\0"
	"COMPRESSED_RGBA8_ETC2_EAC\0"
	"COMPRESSED_SRGB8_ALPHA8_ETC2_EAC\0"

	// GL_KHR_texture_compression_astc_hdr
	// GL_KHR_texture_compression_astc_ldr
	"COMPRESSED_RGBA_ASTC_4x4_KHR\0"
	"COMPRESSED_RGBA_ASTC_5x4_KHR\0"
	"COMPRESSED_RGBA_ASTC_5x5_KHR\0"
	"COMPRESSED_RGBA_ASTC_6x5_KHR\0"
	"COMPRESSED_RGBA_ASTC_6x6_KHR\0"
	"COMPRESSED_RGBA_ASTC_8x5_KHR\0"
	"COMPRESSED_RGBA_ASTC_8x6_KHR\0"
	"COMPRESSED_RGBA_ASTC_8x8_KHR\0"
	"COMPRESSED_RGBA_ASTC_10x5_KHR\0"
	"COMPRESSED_RGBA_ASTC_10x6_KHR\0"
	"COMPRESSED_RGBA_ASTC_10x8_KHR\0"
	"COMPRESSED_RGBA_ASTC_10x10_KHR\0"
	"COMPRESSED_RGBA_ASTC_12x10_KHR\0"
	"COMPRESSED_RGBA_ASTC_12x12_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR\0"
	"COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR\0"

	// PVRTC-II
	"COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2_IMG\0"
	"COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2_IMG\0";

static const array<OffTbl_t, 227> glEnum_offtbl = {{
	{GL_BYTE, 1},
	{GL_UNSIGNED_BYTE, 6},
	{GL_SHORT, 20},
	{GL_UNSIGNED_SHORT, 26},
	{GL_INT, 41},
	{GL_UNSIGNED_INT, 45},
	{GL_FLOAT, 58},
	{GL_HALF_FLOAT, 64},
	{GL_STENCIL_INDEX, 75},
	{GL_DEPTH_COMPONENT, 89},
	{GL_RED, 105},
	{GL_GREEN, 109},
	{GL_BLUE, 115},
	{GL_RGB, 120},
	{GL_RGBA, 124},
	{GL_LUMINANCE, 129},
	{GL_LUMINANCE_ALPHA, 139},
	{GL_R3_G3_B2, 155},
	{GL_UNSIGNED_BYTE_3_3_2, 164},
	{GL_UNSIGNED_SHORT_4_4_4_4, 184},
	{GL_UNSIGNED_SHORT_5_5_5_1, 207},
	{GL_UNSIGNED_INT_8_8_8_8, 230},
	{GL_UNSIGNED_INT_10_10_10_2, 251},
	{GL_LUMINANCE4, 275},
	{GL_LUMINANCE8, 286},
	{GL_LUMINANCE12, 297},
	{GL_LUMINANCE16, 309},
	{GL_LUMINANCE4_ALPHA4, 321},
	{GL_LUMINANCE6_ALPHA2, 339},
	{GL_LUMINANCE8_ALPHA8, 357},
	{GL_LUMINANCE12_ALPHA4, 375},
	{GL_LUMINANCE12_ALPHA12, 394},
	{GL_LUMINANCE16_ALPHA16, 414},
	{GL_INTENSITY, 434},
	{GL_INTENSITY4, 444},
	{GL_INTENSITY8, 455},
	{GL_INTENSITY12, 466},
	{GL_INTENSITY16, 478},
	{GL_RGB4, 490},
	{GL_RGB5, 495},
	{GL_RGB8, 500},
	{GL_RGB10, 505},
	{GL_RGB12, 511},
	{GL_RGB16, 517},
	{GL_RGBA2, 523},
	{GL_RGBA4, 529},
	{GL_RGB5_A1, 535},
	{GL_RGBA8, 543},
	{GL_RGB10_A2, 549},
	{GL_RGBA12, 558},
	{GL_RGBA16, 565},
	{GL_BGR, 572},
	{GL_BGRA, 576},
	{GL_DEPTH_COMPONENT16, 581},
	{GL_DEPTH_COMPONENT24, 599},
	{GL_DEPTH_COMPONENT32, 617},
	{GL_COMPRESSED_RED, 635},
	{GL_COMPRESSED_RG, 650},
	{GL_RG, 664},
	{GL_RG_INTEGER, 667},
	{GL_R8, 678},
	{GL_R16, 681},
	{GL_RG8, 685},
	{GL_RG16, 689},
	{GL_R16F, 694},
	{GL_R32F, 699},
	{GL_RG16F, 704},
	{GL_RG32F, 710},
	{GL_R8I, 716},
	{GL_R8UI, 720},
	{GL_R16I, 725},
	{GL_R16UI, 730},
	{GL_R32I, 736},
	{GL_R32UI, 741},
	{GL_RG8I, 747},
	{GL_RG8UI, 752},
	{GL_RG16I, 758},
	{GL_RG16UI, 764},
	{GL_RG32I, 771},
	{GL_RG32UI, 777},
	{GL_UNSIGNED_BYTE_2_3_3_REV, 784},
	{GL_UNSIGNED_SHORT_5_6_5, 808},
	{GL_UNSIGNED_SHORT_5_6_5_REV, 829},
	{GL_UNSIGNED_SHORT_4_4_4_4_REV, 854},
	{GL_UNSIGNED_SHORT_1_5_5_5_REV, 881},
	{GL_UNSIGNED_INT_8_8_8_8_REV, 908},
	{GL_UNSIGNED_INT_2_10_10_10_REV, 933},
	{GL_RGB_S3TC, 961},
	{GL_RGB4_S3TC, 970},
	{GL_RGBA_S3TC, 980},
	{GL_RGBA4_S3TC, 990},
	{GL_RGBA_DXT5_S3TC, 1001},
	{GL_RGBA4_DXT5_S3TC, 1016},
	{GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 1032},
	{GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 1061},
	{GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 1091},
	{GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 1121},
	{GL_COMPRESSED_ALPHA, 1151},
	{GL_COMPRESSED_LUMINANCE, 1168},
	{GL_COMPRESSED_LUMINANCE_ALPHA, 1189},
	{GL_COMPRESSED_INTENSITY, 1216},
	{GL_COMPRESSED_RGB, 1237},
	{GL_COMPRESSED_RGBA, 1252},
	{GL_DEPTH_STENCIL, 1268},
	{GL_UNSIGNED_INT_24_8, 1282},
	{GL_RGBA32F, 1300},
	{GL_RGB32F, 1308},
	{GL_RGBA16F, 1315},
	{GL_RGB16F, 1323},
	{GL_DEPTH24_STENCIL8, 1330},

	// PVRTC
	{GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, 1347},
	{GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG, 1379},
	{GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, 1411},
	{GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, 1444},

	{GL_R11F_G11F_B10F, 1477},
	{GL_UNSIGNED_INT_10F_11F_11F_REV, 1492},
	{GL_RGB9_E5, 1521},
	{GL_UNSIGNED_INT_5_9_9_9_REV, 1529},
	{GL_SRGB, 1554},
	{GL_SRGB8, 1559},
	{GL_SRGB_ALPHA, 1565},
	{GL_SRGB8_ALPHA8, 1576},
	{GL_SLUMINANCE_ALPHA, 1589},
	{GL_SLUMINANCE8_ALPHA8, 1606},
	{GL_SLUMINANCE, 1625},
	{GL_SLUMINANCE8, 1636},
	{GL_COMPRESSED_SRGB, 1648},
	{GL_COMPRESSED_SRGB_ALPHA, 1664},
	{GL_COMPRESSED_SLUMINANCE, 1686},
	{GL_COMPRESSED_SLUMINANCE_ALPHA, 1708},

	// GL_EXT_texture_compression_latc
	{GL_COMPRESSED_LUMINANCE_LATC1_EXT, 1736},
	{GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT, 1767},
	{GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT, 1805},
	{GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT, 1842},

	{GL_DEPTH_COMPONENT32F, 1886},
	{GL_DEPTH32F_STENCIL8, 1905},
	{GL_STENCIL_INDEX1, 1923},
	{GL_STENCIL_INDEX4, 1938},
	{GL_STENCIL_INDEX8, 1953},
	{GL_STENCIL_INDEX16, 1968},
	{GL_RGB565, 1984},
	{GL_ETC1_RGB8_OES, 1991},
	{GL_RGBA32UI, 2005},
	{GL_RGB32UI, 2014},
	{GL_RGBA16UI, 2022},
	{GL_RGB16UI, 2031},
	{GL_RGBA8UI, 2039},
	{GL_RGB8UI, 2047},
	{GL_RGBA32I, 2054},
	{GL_RGB32I, 2062},
	{GL_RGBA16I, 2069},
	{GL_RGB16I, 2077},
	{GL_RGBA8I, 2084},
	{GL_RGB8I, 2091},
	{GL_RED_INTEGER, 2097},
	{GL_GREEN_INTEGER, 2109},
	{GL_BLUE_INTEGER, 2123},
	{GL_ALPHA_INTEGER, 2136},
	{GL_RGB_INTEGER, 2150},
	{GL_RGBA_INTEGER, 2162},
	{GL_BGR_INTEGER, 2175},
	{GL_BGRA_INTEGER, 2187},
	{GL_INT_2_10_10_10_REV, 2200},
	{GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 2219},
	{GL_COMPRESSED_RED_RGTC1, 2250},
	{GL_COMPRESSED_SIGNED_RED_RGTC1, 2271},
	{GL_COMPRESSED_RG_RGTC2, 2299},
	{GL_COMPRESSED_SIGNED_RG_RGTC2, 2319},
	{GL_COMPRESSED_RGBA_BPTC_UNORM, 2346},
	{GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, 2373},
	{GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, 2406},
	{GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, 2439},
	{GL_R8_SNORM, 2474},
	{GL_RG8_SNORM, 2483},
	{GL_RGB8_SNORM, 2493},
	{GL_RGBA8_SNORM, 2504},
	{GL_R16_SNORM, 2516},
	{GL_RG16_SNORM, 2526},
	{GL_RGB16_SNORM, 2537},
	{GL_RGBA16_SNORM, 2549},
	{GL_RGB10_A2UI, 2562},

	// PVRTC-II
	{GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG, 2573},
	{GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG, 2606},
	{GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT, 2639},
	{GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT, 2672},
	{GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT, 2705},
	{GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT, 2744},

	// ETC2
	{GL_COMPRESSED_R11_EAC, 2783},
	{GL_COMPRESSED_SIGNED_R11_EAC, 2802},
	{GL_COMPRESSED_RG11_EAC, 2828},
	{GL_COMPRESSED_SIGNED_RG11_EAC, 2848},
	{GL_COMPRESSED_RGB8_ETC2, 2875},
	{GL_COMPRESSED_SRGB8_ETC2, 2896},
	{GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 2918},
	{GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 2959},
	{GL_COMPRESSED_RGBA8_ETC2_EAC, 3001},
	{GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, 3027},

	// GL_KHR_texture_compression_astc_hdr
	// GL_KHR_texture_compression_astc_ldr
	{GL_COMPRESSED_RGBA_ASTC_4x4_KHR, 3060},
	{GL_COMPRESSED_RGBA_ASTC_5x4_KHR, 3089},
	{GL_COMPRESSED_RGBA_ASTC_5x5_KHR, 3118},
	{GL_COMPRESSED_RGBA_ASTC_6x5_KHR, 3147},
	{GL_COMPRESSED_RGBA_ASTC_6x6_KHR, 3176},
	{GL_COMPRESSED_RGBA_ASTC_8x5_KHR, 3205},
	{GL_COMPRESSED_RGBA_ASTC_8x6_KHR, 3234},
	{GL_COMPRESSED_RGBA_ASTC_8x8_KHR, 3263},
	{GL_COMPRESSED_RGBA_ASTC_10x5_KHR, 3292},
	{GL_COMPRESSED_RGBA_ASTC_10x6_KHR, 3322},
	{GL_COMPRESSED_RGBA_ASTC_10x8_KHR, 3352},
	{GL_COMPRESSED_RGBA_ASTC_10x10_KHR, 3382},
	{GL_COMPRESSED_RGBA_ASTC_12x10_KHR, 3413},
	{GL_COMPRESSED_RGBA_ASTC_12x12_KHR, 3444},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, 3475},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR, 3512},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR, 3549},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR, 3586},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, 3623},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR, 3660},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR, 3697},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, 3734},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR, 3771},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR, 3809},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR, 3847},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, 3885},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, 3924},
	{GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, 3963},

	// PVRTC-II
	{GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2_IMG, 4002},
	{GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2_IMG, 4041},
}};

/** Public functions **/

/**
 * Look up an OpenGL GLenum string.
 * @param glEnum	[in] glEnum
 * @return String, or nullptr if not found.
 */
const char *lookup_glEnum(unsigned int glEnum)
{
	static_assert(sizeof(glEnum_strtbl) == 4081, "glEnum_offtbl[] needs to be recalculated");

	// Offset table uses uint16_t for glEnum.
	assert(glEnum <= std::numeric_limits<uint16_t>::max());
	if (glEnum > std::numeric_limits<uint16_t>::max()) {
		return nullptr;
	}

	// Do a binary search.
	auto pEntry = std::lower_bound(glEnum_offtbl.cbegin(), glEnum_offtbl.cend(), glEnum,
		[](const OffTbl_t &entry, unsigned int glEnum) noexcept -> bool {
			return (entry.id < glEnum);
		});
	if (pEntry == glEnum_offtbl.cend() || pEntry->id != glEnum || pEntry->offset == 0) {
		return nullptr;
	}
	return &glEnum_strtbl[pEntry->offset];
}

} }
