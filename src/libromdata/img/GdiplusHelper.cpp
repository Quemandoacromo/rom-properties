/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * GdiplusHelper.cpp: GDI+ helper class. (Win32)                           *
 *                                                                         *
 * Copyright (c) 2016 by David Korth.                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#include "GdiplusHelper.hpp"

// Gdiplus for PNG decoding.
// NOTE: Gdiplus requires min/max.
#include <algorithm>
namespace Gdiplus {
	using std::min;
	using std::max;
}
#include <olectl.h>
#include <gdiplus.h>

/**
 * Initialize GDI+.
 * @return GDI+ token, or 0 on failure.
 */
ULONG_PTR GdiplusHelper::InitGDIPlus(void)
{
	Gdiplus::GdiplusStartupInput gdipSI;
	gdipSI.GdiplusVersion = 1;
	gdipSI.DebugEventCallback = nullptr;
	gdipSI.SuppressBackgroundThread = FALSE;
	gdipSI.SuppressExternalCodecs = FALSE;
	ULONG_PTR gdipToken;
	Gdiplus::Status status = GdiplusStartup(&gdipToken, &gdipSI, nullptr);
	return (status == Gdiplus::Status::Ok ? gdipToken : 0);
}

/**
 * Shut down GDI+.
 * @param gdipToken GDI+ token.
 */
void GdiplusHelper::ShutdownGDIPlus(ULONG_PTR gdipToken)
{
	Gdiplus::GdiplusShutdown(gdipToken);
}
