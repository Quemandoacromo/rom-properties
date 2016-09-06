/***************************************************************************
 * ROM Properties Page shell extension. (Win32)                            *
 * RpImageWin32.hpp: rp_image to Win32 conversion functions.               *
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

#ifndef __ROMPROPERTIES_WIN32_RPIMAGEWIN32_HPP__
#define __ROMPROPERTIES_WIN32_RPIMAGEWIN32_HPP__

namespace LibRomData {
	class rp_image;
}

class RpImageWin32
{
	private:
		RpImageWin32();
		~RpImageWin32();
	private:
		RpImageWin32(const RpImageWin32 &);
		RpImageWin32 &operator=(const RpImageWin32&);

	protected:
		/**
		 * Convert an rp_image to a HBITMAP for use as an icon mask.
		 * @return image rp_image.
		 * @return HBITMAP, or nullptr on error.
		 */
		static HBITMAP toHBITMAP_mask(const LibRomData::rp_image *image);

		/**
		 * Convert an rp_image to HBITMAP. (CI8)
		 * @return image rp_image. (Must be CI8.)
		 * @return HBITMAP, or nullptr on error.
		 */
		static HBITMAP toHBITMAP_CI8(const LibRomData::rp_image *image);

		/**
		 * Convert an rp_image to HBITMAP. (ARGB32)
		 * @return image rp_image. (Must be ARGB32.)
		 * @return HBITMAP, or nullptr on error.
		 */
		static HBITMAP toHBITMAP_ARGB32(const LibRomData::rp_image *image);

	public:
		/**
		 * Convert an rp_image to HBITMAP.
		 * @return image rp_image.
		 * @return HBITMAP, or nullptr on error.
		 */
		static HBITMAP toHBITMAP(const LibRomData::rp_image *image);

		/**
		 * Convert an rp_image to HICON.
		 * @param image rp_image.
		 * @return HICON, or nullptr on error.
		 */
		static HICON toHICON(const LibRomData::rp_image *image);
};

#endif /* __ROMPROPERTIES_WIN32_RPIMAGEWIN32_HPP__ */
