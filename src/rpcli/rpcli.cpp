/***************************************************************************
 * ROM Properties Page shell extension. (rpcli)                            *
 * rpcli.cpp: Command-line interface for properties.                       *
 *                                                                         *
 * Copyright (c) 2016-2018 by Egor.                                        *
 * Copyright (c) 2016-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "config.rpcli.h"
#include "config.version.h"
#include "librpbase/config.librpbase.h"
#include "libromdata/config.libromdata.h"

// OS-specific security options.
#include "rpcli_secure.h"

// libgsvt for VT handling
#include "gsvtpp.hpp"

// librpbyteswap
#include "librpbyteswap/byteswap_rp.h"

// librpbase
#include "libi18n/i18n.h"
#include "librpbase/RomData.hpp"
#include "librpbase/SystemRegion.hpp"
#include "librpbase/img/RpPng.hpp"
#include "librpbase/img/IconAnimData.hpp"
#include "librpbase/TextOut.hpp"
using namespace LibRpBase;

// librpfile
#include "librpfile/config.librpfile.h"
#include "librpfile/FileSystem.hpp"
#include "librpfile/RpFile.hpp"
using namespace LibRpFile;

// libromdata
#include "libromdata/RomDataFactory.hpp"
using namespace LibRomData;

// librptexture
#include "librptexture/fileformat/FileFormat.hpp"
#include "librptexture/img/rp_image.hpp"
#ifdef _WIN32
#  include "libwin32common/RpWin32_sdk.h"
#  include "librptexture/img/GdiplusHelper.hpp"
#endif /* _WIN32 */
using namespace LibRpTexture;

#ifdef ENABLE_SIXEL
// Sixel
#include "rp_sixel.hpp"
#endif /* ENABLE_SIXEL */

#ifdef ENABLE_DECRYPTION
#  include "verifykeys.hpp"
#endif /* ENABLE_DECRYPTION */
#include "device.hpp"

// OS-specific userdirs
#ifdef _WIN32
#  include "libwin32common/userdirs.hpp"
#  define OS_NAMESPACE LibWin32Common
#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x4
#  endif /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
#else
#  include "libunixcommon/userdirs.hpp"
#  define OS_NAMESPACE LibUnixCommon
#endif
#include "tcharx.h"

// C includes
#include "ctypex.h"

// C++ STL classes
#include <sstream>
using std::array;
using std::cout;
using std::cerr;
using std::locale;
using std::ofstream;
using std::ostringstream;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

#include "libi18n/config.libi18n.h"
#ifdef _MSC_VER
// MSVC: Exception handling for /DELAYLOAD.
#include "libwin32common/DelayLoadHelper.h"

// DelayLoad: libromdata
// NOTE: Not using DELAYLOAD_TEST_FUNCTION_IMPL1 here due to the use of C++ functions.
#include "libromdata/config/ImageTypesConfig.hpp"
DELAYLOAD_FILTER_FUNCTION_IMPL(ImageTypesConfig_className);
static int DelayLoad_test_ImageTypesConfig_className(void) {
	static bool success = 0;
	if (!success) {
		__try {
			(void)ImageTypesConfig::className(0);
		} __except (DelayLoad_filter_ImageTypesConfig_className(GetExceptionCode())) {
			return -ENOTSUP;
		}
		success = 1;
	}
	return 0;
}

#  ifdef ENABLE_NLS
// DelayLoad: libi18n
#    include "libi18n/i18n.h"
DELAYLOAD_TEST_FUNCTION_IMPL1(libintl_textdomain, nullptr);
#  endif /* ENABLE_NLS */
#endif /* _MSC_VER */

// Mini-T2U8()
#ifdef _WIN32
#  include "librptext/wchar.hpp"
#  define T2U8c(tcs) (T2U8(tcs).c_str())
#else /* !_WIN32 */
#  define T2U8c(tcs) (tcs)
#endif /* _WIN32 */

struct ExtractParam {
	const TCHAR *filename;	// Target filename. Can be null due to argv[argc]
	int imageType;		// Image Type. -1 = iconAnimData, MUST be between -1 and IMG_INT_MAX
	int mipmapLevel;	// Mipmap level. (IMG_INT_IMAGE only) -1 = use default; 0 or higher = mipmap level

	ExtractParam(const TCHAR *filename, int imageType, int mipmapLevel = -1)
		: filename(filename)
		, imageType(imageType)
		, mipmapLevel(mipmapLevel)
	{}
};

/**
 * Extracts images from romdata
 * @param romData RomData containing the images
 * @param extract Vector of image extraction parameters
 */
static void ExtractImages(const RomData *romData, const vector<ExtractParam> &extract)
{
	const uint32_t supported = romData->supportedImageTypes();
	for (const ExtractParam &p : extract) {
		if (!p.filename) continue;
		bool found = false;
		
		if (p.imageType >= 0 && (supported & (1U << p.imageType))) {
			// normal image
			bool isMipmap = (unlikely(p.mipmapLevel >= 0));
			const RomData::ImageType imageType =
				static_cast<RomData::ImageType>(p.imageType);
			rp_image_const_ptr image;

			if (likely(!isMipmap)) {
				// normal image
				image = romData->image(imageType);
			} else {
				// mipmap level for IMG_INT_IMAGE
				image = romData->mipmap(p.mipmapLevel);
			}

			if (image && image->isValid()) {
				found = true;
				Gsvt::StdErr.textColorSet8(6, true);	// cyan
				Gsvt::StdErr.fputs("-- ");
				if (likely(!isMipmap)) {
					// tr: {0:s} == image type name, {1:s} == output filename
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Extracting {0:s} into '{1:s}'")),
						RomData::getImageTypeName(imageType), T2U8c(p.filename)));
				} else {
					// tr: {0:d} == mipmap level, {1:s} == output filename
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Extracting mipmap level {0:d} into '{1:s}'")),
						p.mipmapLevel, T2U8c(p.filename)));
				}
				Gsvt::StdErr.textColorReset();
				Gsvt::StdErr.newline();
				Gsvt::StdErr.fflush();

				int errcode = RpPng::save(p.filename, image);
				if (errcode != 0) {
					// tr: {0:s} == filename, {1:s} == error message
					Gsvt::StdErr.textColorSet8(1, true);	// red
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Couldn't create file '{0:s}': {1:s}")),
						T2U8c(p.filename), strerror(-errcode)));
					Gsvt::StdErr.textColorReset();
					Gsvt::StdErr.newline();
				} else {
					Gsvt::StdErr.fputs("   ");
					Gsvt::StdErr.fputs(C_("rpcli", "Done"));
					Gsvt::StdErr.newline();
				}
				Gsvt::StdErr.fflush();
			}
		} else if (p.imageType == -1) {
			// iconAnimData image
			auto iconAnimData = romData->iconAnimData();
			if (iconAnimData && iconAnimData->count != 0 && iconAnimData->seq_count != 0) {
				found = true;
				Gsvt::StdErr.textColorSet8(6, true);	// cyan
				Gsvt::StdErr.fputs("-- ");
				Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Extracting animated icon into '{:s}'")), T2U8c(p.filename)));
				Gsvt::StdErr.textColorReset();
				Gsvt::StdErr.newline();
				Gsvt::StdErr.fflush();

				int errcode = RpPng::save(p.filename, iconAnimData);
				if (errcode == -ENOTSUP) {
					Gsvt::StdErr.textColorSet8(3, true);	// yellow
					Gsvt::StdErr.fputs("   ");
					Gsvt::StdErr.fputs(C_("rpcli", "APNG is not supported, extracting only the first frame"));
					Gsvt::StdErr.textColorReset();
					Gsvt::StdErr.newline();
					Gsvt::StdErr.fflush();
					// falling back to outputting the first frame
					errcode = RpPng::save(p.filename, iconAnimData->frames[iconAnimData->seq_index[0]]);
				}
				if (errcode != 0) {
					Gsvt::StdErr.textColorSet8(1, true);	// red
					Gsvt::StdErr.fputs("   ");
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Couldn't create file '{0:s}': {1:s}")),
						T2U8c(p.filename), strerror(-errcode)));
					Gsvt::StdErr.textColorReset();
					Gsvt::StdErr.newline();
				} else {
					Gsvt::StdErr.fputs("   ");
					Gsvt::StdErr.fputs(C_("rpcli", "Done"));
					Gsvt::StdErr.newline();
				}
				Gsvt::StdErr.fflush();
			}
		}

		if (!found) {
			// TODO: Return an error code?
			Gsvt::StdErr.textColorSet8(1, true);	// red
			Gsvt::StdErr.fputs("-- ");
			if (p.imageType == -1) {
				Gsvt::StdErr.fputs(C_("rpcli", "Animated icon not found"));
			} else if (p.mipmapLevel >= 0) {
				Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Mipmap level {:d} not found")), p.mipmapLevel));
			} else {
				const RomData::ImageType imageType =
					static_cast<RomData::ImageType>(p.imageType);
				Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Image '{:s}' not found")),
					RomData::getImageTypeName(imageType)));
			}
			Gsvt::StdErr.textColorReset();
			Gsvt::StdErr.newline();
			Gsvt::StdErr.fflush();
		}
	}
}

/**
 * Shows info about file
 * @param filename ROM filename
 * @param json Is program running in json mode?
 * @param extract Vector of image extraction parameters
 * @param lc Language code (0 for default)
 * @param flags ROMOutput flags (see OutputFlags)
 */
static void DoFile(const TCHAR *filename, bool json, const vector<ExtractParam> &extract,
	uint32_t lc = 0, unsigned int flags = 0)
{
	RomDataPtr romData;

	if (likely(!FileSystem::is_directory(filename))) {
		// File: Open the file and call RomDataFactory::create() with the opened file.

		// FIXME: Make T2U8c() unnecessary here.
		Gsvt::StdErr.textColorSet8(6, true);	// cyan
		Gsvt::StdErr.fputs("== ");
		Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Reading file '{:s}'...")), T2U8c(filename)));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		shared_ptr<RpFile> file = std::make_shared<RpFile>(filename, RpFile::FM_OPEN_READ_GZ);
		if (!file->isOpen()) {
			// TODO: Return an error code?
			Gsvt::StdErr.textColorSet8(1, true);	// red
			Gsvt::StdErr.fputs("-- ");
			Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Couldn't open file: {:s}")), strerror(file->lastError())));
			Gsvt::StdErr.textColorReset();
			Gsvt::StdErr.newline();
			Gsvt::StdErr.fflush();
			if (json) {
				Gsvt::StdOut.fputs(fmt::format(FSTR("{{\"error\":\"couldn't open file\",\"code\":{:d}}}\n"), file->lastError()));
				Gsvt::StdOut.fflush();
			}
			return;
		}

		romData = RomDataFactory::create(file);
	} else {
		// Directory: Call RomDataFactory::create() with the filename.

		// FIXME: Make T2U8c() unnecessary here.
		Gsvt::StdErr.textColorSet8(6, true);	// cyan
		Gsvt::StdErr.fputs("== ");
		Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Reading directory '{:s}'...")), T2U8c(filename)));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		romData = RomDataFactory::create(filename);
	}

	if (romData) {
		if (json) {
			Gsvt::StdErr.textColorSet8(6, true);	// cyan
			Gsvt::StdErr.fputs("-- ");
			Gsvt::StdErr.fputs(C_("rpcli", "Outputting JSON data"));
			Gsvt::StdErr.textColorReset();
			Gsvt::StdErr.newline();
			Gsvt::StdErr.fflush();

#ifdef _WIN32
			// Windows: Use gsvt_fwrite() for faster console output where applicable.
			// FIXME: gsvt_cout wrapper.
			// FIXME: gsvt_fwrite_raw() function to skip ANSI escape parsing.
			ostringstream oss;
			oss << JSONROMOutput(romData.get(), flags) << '\n';
			const string str = oss.str();
			// TODO: Error checking.
			Gsvt::StdOut.fputs(str);
#else /* !_WIN32 */
			// Not Windows: Write directly to cout.
			// FIXME: gsvt_cout wrapper.
			cout << JSONROMOutput(romData.get(), flags) << '\n';
#endif /* _WIN32 */
		} else {
#ifdef _WIN32
			// Windows: Use gsvt_fwrite() for faster console output where applicable.
			// FIXME: gsvt_cout wrapper.
			ostringstream oss;
			oss << ROMOutput(romData.get(), lc, flags) << '\n';
			const string str = oss.str();
			// TODO: Error checking.
			Gsvt::StdOut.fputs(str);
#else /* !_WIN32 */
#ifdef ENABLE_SIXEL
			// If this is a tty, print the icon/banner using libsixel.
			if (Gsvt::StdOut.isConsole()) {
				print_sixel_icon_banner(romData);
			}
#endif /* ENABLE_SIXEL */
			// Not Windows: Write directly to cout.
			// FIXME: gsvt_cout wrapper.
			cout << ROMOutput(romData.get(), lc, flags) << '\n';
#endif /* _WIN32 */
		}
		cout.flush();
		Gsvt::StdOut.fflush();
		ExtractImages(romData.get(), extract);
	} else {
		Gsvt::StdErr.textColorSet8(1, true);	// red
		Gsvt::StdErr.fputs("-- ");
		Gsvt::StdErr.fputs(C_("rpcli", "ROM is not supported"));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		if (json) {
			Gsvt::StdOut.fputs(("{\"error\":\"rom is not supported\"}\n"));
			Gsvt::StdOut.fflush();
		}
	}
}

/**
 * Print the system region information.
 */
static void PrintSystemRegion(void)
{
	string buf;
	buf.reserve(8);

	uint32_t lc = __swab32(SystemRegion::getLanguageCode());
	if (lc != 0) {
		for (unsigned int i = 4; i > 0; i--, lc >>= 8) {
			if ((lc & 0xFF) == 0)
				continue;
			buf += static_cast<char>(lc & 0xFF);
		}
	}
	Gsvt::StdOut.fputs(fmt::format(FRUN(C_("rpcli", "System language code: {:s}")),
		(!buf.empty() ? buf.c_str() : C_("rpcli", "0 (this is a bug!)"))));
	Gsvt::StdOut.newline();

	uint32_t cc = __swab32(SystemRegion::getCountryCode());
	buf.clear();
	if (cc != 0) {
		for (unsigned int i = 4; i > 0; i--, cc >>= 8) {
			if ((cc & 0xFF) == 0)
				continue;
			buf += static_cast<char>(cc & 0xFF);
		}
	}
	Gsvt::StdOut.fputs(fmt::format(FRUN(C_("rpcli", "System country code: {:s}")),
		(!buf.empty() ? buf.c_str() : C_("rpcli", "0 (this is a bug!)"))));
	Gsvt::StdOut.newline();

	// Extra line. (TODO: Only if multiple commands are specified.)
	Gsvt::StdOut.newline();
	Gsvt::StdOut.fflush();
}

/**
 * Print pathname information.
 */
static void PrintPathnames(void)
{
	// TODO: Localize these strings?
	// TODO: Only print an extra line if multiple commands are specified.
	Gsvt::StdOut.fputs(fmt::format(FSTR(
		"User's home directory:   {:s}\n"
		"User's cache directory:  {:s}\n"
		"User's config directory: {:s}\n"
		"\n"
		"RP cache directory:      {:s}\n"
		"RP config directory:     {:s}\n"),
		OS_NAMESPACE::getHomeDirectory(),
		OS_NAMESPACE::getCacheDirectory(),
		OS_NAMESPACE::getConfigDirectory(),
		FileSystem::getCacheDirectory(),
		FileSystem::getConfigDirectory()));
	Gsvt::StdOut.newline();

	Gsvt::StdOut.fflush();
}

#ifdef RP_OS_SCSI_SUPPORTED
/**
 * Run a SCSI INQUIRY command on a device.
 * @param filename Device filename
 * @param json Is program running in json mode?
 */
static void DoScsiInquiry(const TCHAR *filename, bool json)
{
	// FIXME: Make T2U8c() unnecessary here.
	Gsvt::StdErr.textColorSet8(6, true);	// cyan
	Gsvt::StdErr.fputs("== ");
	Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Opening device file '{:s}'...")), T2U8c(filename)));
	Gsvt::StdErr.textColorReset();
	Gsvt::StdErr.newline();
	Gsvt::StdErr.fflush();

	unique_ptr<RpFile> file(new RpFile(filename, RpFile::FM_OPEN_READ_GZ));
	if (!file->isOpen()) {
		// TODO: Return an error code?
		Gsvt::StdErr.textColorSet8(1, true);	// red
		Gsvt::StdErr.fputs("-- ");
		Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Couldn't open file: {:s}")), strerror(file->lastError())));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		if (json) {
			Gsvt::StdOut.fputs(fmt::format("{{\"error\":\"couldn't open file\",\"code\":{:d}}}\n", file->lastError()));
			Gsvt::StdOut.fflush();
		}
		return;
	}

	// TODO: Check for unsupported devices? (Only CD-ROM is supported.)
	if (!file->isDevice()) {
		// TODO: Return an error code?
		Gsvt::StdErr.textColorSet8(1, true);	// red
		Gsvt::StdErr.fputs("-- ");
		Gsvt::StdErr.fputs(C_("rpcli", "Not a device file"));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		if (json) {
			Gsvt::StdOut.fputs("{\"error\":\"not a device file\"}\n");
			Gsvt::StdOut.fflush();
		}
		return;
	}

	if (json) {
		Gsvt::StdErr.textColorSet8(6, true);	// cyan
		Gsvt::StdErr.fputs("-- ");
		Gsvt::StdErr.fputs(C_("rpcli", "Outputting JSON data"));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		// TODO: JSONScsiInquiry
		//cout << JSONScsiInquiry(file.get()) << '\n';
		//cout.flush();
	} else {
#ifdef _WIN32
		// Windows: Use gsvt_fwrite() for faster console output where applicable.
		// FIXME: gsvt_cout wrapper.
		// FIXME: gsvt_fwrite_raw() function to skip ANSI escape parsing.
		ostringstream oss;
		oss << ScsiInquiry(file.get()) << '\n';
		cout.flush();
		const string str = oss.str();
		// TODO: Error checking.
		Gsvt::StdOut.fputs(str);
#else /* !_WIN32 */
		// Not Windows: Write directly to cout.
		// FIXME: gsvt_cout wrapper.
		cout << ScsiInquiry(file.get()) << '\n';
		cout.flush();
#endif /* _WIN32 */
	}
}

/**
 * Run an ATA IDENTIFY DEVICE command on a device.
 * @param filename Device filename
 * @param json Is program running in json mode?
 * @param packet If true, use ATA IDENTIFY PACKET.
 */
static void DoAtaIdentifyDevice(const TCHAR *filename, bool json, bool packet)
{
	// FIXME: Make T2U8c() unnecessary here.
	Gsvt::StdErr.textColorSet8(6, true);	// cyan
	Gsvt::StdErr.fputs("== ");
	Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Opening device file '{:s}'...")), T2U8c(filename)));
	Gsvt::StdErr.textColorReset();
	Gsvt::StdErr.newline();
	Gsvt::StdErr.fflush();

	unique_ptr<RpFile> file(new RpFile(filename, RpFile::FM_OPEN_READ_GZ));
	if (!file->isOpen()) {
		// TODO: Return an error code?
		Gsvt::StdErr.textColorSet8(1, true);	// red
		Gsvt::StdErr.fputs("-- ");
		Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Couldn't open file: {:s}")), strerror(file->lastError())));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		if (json) {
			Gsvt::StdOut.fputs(fmt::format("{{\"error\":\"couldn't open file\",\"code\":{:d}}}\n", file->lastError()));
			Gsvt::StdOut.fflush();
		}
		return;
	}

	// TODO: Check for unsupported devices? (Only CD-ROM is supported.)
	if (!file->isDevice()) {
		// TODO: Return an error code?
		Gsvt::StdErr.textColorSet8(1, true);	// red
		Gsvt::StdErr.fputs("-- ");
		Gsvt::StdErr.fputs(C_("rpcli", "Not a device file"));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		if (json) {
			Gsvt::StdOut.fputs("{\"error\":\"Not a device file\"}\n");
			Gsvt::StdOut.fflush();
		}
		return;
	}

	if (json) {
		Gsvt::StdErr.textColorSet8(6, true);	// cyan
		Gsvt::StdErr.fputs("-- ");
		Gsvt::StdErr.fputs(C_("rpcli", "Outputting JSON data"));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();

		// TODO: JSONAtaIdentifyDevice
		//cout << JSONAtaIdentifyDevice(file.get(), packet) << '\n';
		//cout.flush();
	} else {
#ifdef _WIN32
		// Windows: Use gsvt_fwrite() for faster console output where applicable.
		// FIXME: gsvt_cout wrapper.
		// FIXME: gsvt_fwrite_raw() function to skip ANSI escape parsing.
		ostringstream oss;
		oss << AtaIdentifyDevice(file.get(), packet) << '\n';
		cout.flush();
		const string str = oss.str();
		// TODO: Error checking.
		Gsvt::StdOut.fputs(str);
#else /* !_WIN32 */
		// Not Windows: Write directly to cout.
		// FIXME: gsvt_cout wrapper.
		cout << AtaIdentifyDevice(file.get(), packet) << '\n';
		cout.flush();
#endif /* _WIN32 */
	}
}
#endif /* RP_OS_SCSI_SUPPORTED */

static void ShowUsage(void)
{
	// TODO: Use argv[0] instead of hard-coding 'rpcli'?
#ifdef ENABLE_DECRYPTION	
	const char *const s_usage = C_("rpcli", "Usage: rpcli [-k] [-c] [-p] [-j] [-l lang] [[-xN outfile]... [-mN outfile]... [-a apngoutfile] filename]...");
#else /* !ENABLE_DECRYPTION */
	const char *const s_usage = C_("rpcli", "Usage: rpcli [-c] [-p] [-j] [-l lang] [[-xN outfile]... [-mN outfile]... [-a apngoutfile] filename]...");
#endif /* ENABLE_DECRYPTION */
	Gsvt::StdErr.fputs(s_usage);
	Gsvt::StdErr.newline();

	struct cmd_t {
		char opt[8];	// TODO: Automatic padding?
		const char *desc;
	};

	// Normal commands
#ifdef ENABLE_DECRYPTION
	static const array<cmd_t, 9> cmds = {{
		{"  -k:  ", NOP_C_("rpcli", "Verify encryption keys in keys.conf.")},
#else /* !ENABLE_DECRYPTION */
	static const array<cmd_t, 8> cmds = {{
#endif /* ENABLE_DECRYPTION */
		{"  -c:  ", NOP_C_("rpcli", "Print system region information.")},
		{"  -p:  ", NOP_C_("rpcli", "Print system path information.")},
		{"  -d:  ", NOP_C_("rpcli", "Skip ListData fields with more than 10 items. [text only]")},
		{"  -j:  ", NOP_C_("rpcli", "Use JSON output format.")},
		{"  -l:  ", NOP_C_("rpcli", "Retrieve the specified language from the ROM image.")},
		{"  -xN: ", NOP_C_("rpcli", "Extract image N to outfile in PNG format.")},
		{"  -mN: ", NOP_C_("rpcli", "Extract mipmap level N to outfile in PNG format.")},
		{"  -a:  ", NOP_C_("rpcli", "Extract the animated icon to outfile in APNG format.")},
	}};

	for (const auto &p : cmds) {
		Gsvt::StdErr.fputs(p.opt);
		Gsvt::StdErr.fputs(pgettext_expr("rpcli", p.desc));
		Gsvt::StdErr.newline();
	}
	Gsvt::StdErr.newline();

#ifdef RP_OS_SCSI_SUPPORTED
	// Commands for devices
	static const array<cmd_t, 3> cmds_dev = {{
		{"  -is: ", NOP_C_("rpcli", "Run a SCSI INQUIRY command.")},
		{"  -ia: ", NOP_C_("rpcli", "Run an ATA IDENTIFY DEVICE command.")},
		{"  -ip: ", NOP_C_("rpcli", "Run an ATA IDENTIFY PACKET DEVICE command.")},
	}};

	Gsvt::StdErr.fputs(C_("rpcli", "Special options for devices:"));
	Gsvt::StdErr.newline();
	for (const auto &p : cmds_dev) {
		Gsvt::StdErr.fputs(p.opt);
		Gsvt::StdErr.fputs(pgettext_expr("rpcli", p.desc));
		Gsvt::StdErr.newline();
	}
	Gsvt::StdErr.newline();
#endif /* RP_OS_SCSI_SUPPORTED */

	Gsvt::StdErr.fputs(C_("rpcli", "Examples:"));
	Gsvt::StdErr.newline();
	Gsvt::StdErr.fputs("* rpcli s3.gen\n");
	Gsvt::StdErr.fputs("\t ");
		Gsvt::StdErr.fputs(C_("rpcli", "displays info about s3.gen"));
		Gsvt::StdErr.newline();
	Gsvt::StdErr.fputs("* rpcli -x0 icon.png pokeb2.nds\n");
	Gsvt::StdErr.fputs("\t ");
		Gsvt::StdErr.fputs(C_("rpcli", "extracts icon from pokeb2.nds"));
		Gsvt::StdErr.newline();
	Gsvt::StdErr.fflush();
}

int RP_C_API _tmain(int argc, TCHAR *argv[])
{
	// Enable security options.
	rpcli_do_security_options();

#ifdef __GLIBC__
	// Reduce /etc/localtime stat() calls.
	// References:
	// - https://lwn.net/Articles/944499/
	// - https://gitlab.com/procps-ng/procps/-/merge_requests/119
	setenv("TZ", ":/etc/localtime", 0);
#endif /* __GLIBC__ */

	// Set the C and C++ locales.
#if defined(_WIN32) && defined(__MINGW32__)
	// FIXME: MinGW-w64 12.0.0 doesn't like setting the C++ locale to "".
	setlocale(LC_ALL, "");
#else /* !_WIN32 || !__MINGW32__ */
	locale::global(locale(""));
#endif /* _WIN32 && __MINGW32__ */
#ifdef _WIN32
	// NOTE: Revert LC_CTYPE to "C" to fix UTF-8 output.
	// (Needed for MSVC 2022; does nothing for MinGW-w64 11.0.0)
	setlocale(LC_CTYPE, "C");
#endif /* _WIN32 */

	// Detect console information.
	// NOTE: Technically not needed, since Gsvt::Console access
	// will call this for us...
	gsvt_init();

#if defined(ENABLE_NLS) && defined(_MSC_VER)
	// Delay load verification: libgnuintl
	// TODO: Skip this if not linked with /DELAYLOAD?
	if (DelayLoad_test_libintl_textdomain() != 0) {
		// Delay load failed.
		Gsvt::StdErr.textColorSet8(1, true);	// red
		Gsvt::StdErr.fputs("*** ERROR: " LIBGNUINTL_DLL " could not be loaded.");
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.fputs("\n\n"
			"This build of rom-properties has localization enabled,\n"
			"which requires the use of GNU gettext.\n\n"
			"Please redownload rom-properties " RP_VERSION_STRING " and copy the\n"
			LIBGNUINTL_DLL " file to the installation directory.\n");
		Gsvt::StdErr.fflush();
		return EXIT_FAILURE;
	}
#endif /* ENABLE_NLS && _MSC_VER */

#ifdef RP_LIBROMDATA_IS_DLL
#  ifdef _MSC_VER
#    define ROMDATA_PREFIX
#  else
#    define ROMDATA_PREFIX "lib"
#  endif
#  ifndef NDEBUG
#    define ROMDATA_SUFFIX "-" LIBROMDATA_SOVERSION_STR "d"
#  else
#    define ROMDATA_SUFFIX "-" LIBROMDATA_SOVERSION_STR
#  endif
#  ifdef _WIN32
#    define ROMDATA_EXT ".dll"
#  else
// TODO: macOS
#    define ROMDATA_EXT ".so"
#  endif
#  define ROMDATA_DLL ROMDATA_PREFIX "romdata" ROMDATA_SUFFIX ROMDATA_EXT

#  ifdef _MSC_VER
	// Delay load verification: romdata-X.dll
	// TODO: Skip this if not linked with /DELAYLOAD?
	if (DelayLoad_test_ImageTypesConfig_className() != 0) {
		// Delay load failed.
		Gsvt::StdErr.textColorSet8(1, true);	// red
		Gsvt::StdErr.fputs("*** ERROR: " ROMDATA_DLL " could not be loaded.");
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.fputs("\n\n"
			"Please redownload rom-properties " RP_VERSION_STRING " and copy the\n"
			ROMDATA_DLL " file to the installation directory.\n");
		Gsvt::StdErr.fflush();
		return EXIT_FAILURE;
	}
#  endif /* _MSC_VER */
#endif /* RP_LIBROMDATA_IS_DLL */

	// Initialize i18n.
	rp_i18n_init();

	if(argc < 2){
		ShowUsage();

		// Since we didn't do anything, return a failure code.
		return EXIT_FAILURE;
	}

	static_assert(RomData::IMG_INT_MIN == 0, "RomData::IMG_INT_MIN must be 0!");

	unsigned int flags = 0;	// OutputFlags
	// DoFile parameters
	bool json = false;
	vector<ExtractParam> extract;

	// TODO: Add a command line option to override color output.
	// NOTE: Only checking ci_stdout here, since the actual data is printed to stdout.
	if (Gsvt::StdOut.isConsole()) {
#ifndef _WIN32
		// Non-Windows: ANSI color support is required.
		if (Gsvt::StdOut.supportsAnsi())
#endif /* !_WIN32 */
		{
			flags |= OF_Text_UseAnsiColor;
		}
	}

	for (int i = 1; i < argc; i++) { // figure out the json mode in advance
		if (argv[i][0] == _T('-')) {
			if (argv[i][1] == _T('j')) {
				json = true;
			} else if (argv[i][1] == _T('J')) {
				json = true;
				flags |= OF_JSON_NoPrettyPrint;
			}
		}
	}
	if (json) {
		cout.flush();
		Gsvt::StdOut.fputs("[\n");
		Gsvt::StdOut.fflush();
	}

#ifdef _WIN32
	// Initialize GDI+.
	const ULONG_PTR gdipToken = GdiplusHelper::InitGDIPlus();
	assert(gdipToken != 0);
	if (gdipToken == 0) {
		Gsvt::StdErr.textColorSet8(6, true);	// red
		Gsvt::StdErr.fputs(C_("rpcli", "*** ERROR: GDI+ initialization failed."));
		Gsvt::StdErr.textColorReset();
		Gsvt::StdErr.newline();
		Gsvt::StdErr.fflush();
		return -EIO;
	}
#endif /* _WIN32 */

#ifdef RP_OS_SCSI_SUPPORTED
	bool inq_scsi = false;
	bool inq_ata = false;
	bool inq_ata_packet = false;
#endif /* RP_OS_SCSI_SUPPORTED */
	uint32_t lc = 0;
	bool first = true;
	int ret = 0;
	for (int i = 1; i < argc; i++){
		if (argv[i][0] == _T('-')){
			switch (argv[i][1]) {
			case _T('C'):
				// Force-enable ANSI escape sequences, even if it's not supported by the
				// console or we're redirected to a file.
				// TODO: Document this, and provide an option to force-disable ANSI.
				Gsvt::StdOut.forceColorOn();
				Gsvt::StdErr.forceColorOn();
				flags |= OF_Text_UseAnsiColor;
				break;

#ifdef ENABLE_DECRYPTION
			case _T('k'): {
				// Verify encryption keys.
				static bool hasVerifiedKeys = false;
				if (!hasVerifiedKeys) {
					hasVerifiedKeys = true;
					ret = VerifyKeys();
				}
				break;
			}
#endif /* ENABLE_DECRYPTION */

			case _T('c'):
				// Print the system region information.
				PrintSystemRegion();
				break;

			case _T('p'):
				// Print pathnames.
				PrintPathnames();
				break;

			case _T('l'): {
				// Language code.
				// NOTE: Actual language may be immediately after 'l',
				// or it might be a completely separate argument.
				// NOTE 2: Allowing multiple language codes, since the
				// language code affects files specified *after* it.
				const TCHAR *s_lang;
				if (argv[i][2] == _T('\0')) {
					// Separate argument.
					s_lang = argv[i+1];
					i++;
				} else {
					// Same argument.
					s_lang = &argv[i][2];
				}

				// Parse the language code.
				uint32_t new_lc = 0;
				int pos;
				for (pos = 0; pos < 4 && s_lang[pos] != _T('\0'); pos++) {
					TCHAR c = tolower_ascii(s_lang[pos]);
					if (!islower_ascii(c)) {
						// Not a valid language code character.
						new_lc = 0;
						break;
					}

					new_lc <<= 8;
					new_lc |= static_cast<uint8_t>(c);
				}
				if (new_lc == 0 || (pos == 4 && s_lang[pos] != _T('\0'))) {
					// Invalid language code.
					Gsvt::StdErr.textColorSet8(3, true);	// yellow
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Warning: ignoring invalid language code '{:s}'")), T2U8c(s_lang)));
					Gsvt::StdErr.textColorReset();
					Gsvt::StdErr.newline();
					Gsvt::StdErr.fflush();
					break;
				}

				// Language code set.
				lc = new_lc;
				break;
			}

			case _T('K'): {
				// Skip internal images. (NOTE: Not documented.)
				flags |= LibRpBase::OF_SkipInternalImages;
				break;
			}

			case _T('d'): {
				// Skip RFT_LISTDATA with more than 10 items. (Text only)
				flags |= LibRpBase::OF_SkipListDataMoreThan10;
				break;
			}

			case _T('x'): {
				const TCHAR *const ts_imgType = argv[i] + 2;
				TCHAR *endptr = nullptr;
				const long num = _tcstol(ts_imgType, &endptr, 10);
				if (*endptr != '\0') {
#ifdef _WIN32
					// fmt::print() doesn't allow mixing narrow and wide strings.
					const string s_imgType = T2U8(ts_imgType);
#else /* !_WIN32 */
					const char *const s_imgType = ts_imgType;
#endif /* _WIN32 */
					Gsvt::StdErr.textColorSet8(3, true);	// yellow
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Warning: skipping invalid image type '{:s}'")), s_imgType));
					Gsvt::StdErr.textColorReset();
					Gsvt::StdErr.newline();
					Gsvt::StdErr.fflush();
					i++; continue;
				} else if (num < RomData::IMG_INT_MIN || num > RomData::IMG_INT_MAX) {
					Gsvt::StdErr.textColorSet8(3, true);	// yellow
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Warning: skipping unknown image type {:d}")), num));
					Gsvt::StdErr.textColorReset();
					Gsvt::StdErr.newline();
					Gsvt::StdErr.fflush();
					i++; continue;
				}
				extract.emplace_back(argv[++i], num);
				break;
			}

			case _T('m'): {
				const TCHAR *const ts_mipmapLevel = argv[i] + 2;
				TCHAR *endptr = nullptr;
				const long num = _tcstol(ts_mipmapLevel, &endptr, 10);
				if (*endptr != '\0') {
#ifdef _WIN32
					// fmt::print() doesn't allow mixing narrow and wide strings.
					const string s_mipmapLevel = T2U8(ts_mipmapLevel);
#else /* !_WIN32 */
					const char *const s_mipmapLevel = ts_mipmapLevel;
#endif /* _WIN32 */
					Gsvt::StdErr.textColorSet8(3, true);	// yellow
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Warning: skipping invalid mipmap level '{:s}'")), s_mipmapLevel));
					Gsvt::StdErr.textColorReset();
					Gsvt::StdErr.newline();
					Gsvt::StdErr.fflush();
					i++; continue;
				} else if (num < -1 || num > 1024) {
					Gsvt::StdErr.textColorSet8(3, true);	// yellow
					Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Warning: skipping out-of-range mipmap level {:d}")), num));
					Gsvt::StdErr.textColorReset();
					Gsvt::StdErr.newline();
					Gsvt::StdErr.fflush();
					i++; continue;
				}
				extract.emplace_back(argv[++i], RomData::IMG_INT_IMAGE, num);
				break;
			}

			case _T('a'):
				extract.emplace_back(argv[++i], -1);
				break;

			case _T('j'): // do nothing
			case _T('J'): // still do nothing
				break;

#ifdef RP_OS_SCSI_SUPPORTED
			case _T('i'):
				// These commands take precedence over the usual rpcli functionality.
				switch (argv[i][2]) {
					case _T('s'):
						// SCSI INQUIRY command.
						inq_scsi = true;
						break;
					case _T('a'):
						// ATA IDENTIFY DEVICE command.
						inq_ata = true;
						break;
					case _T('p'):
						// ATA IDENTIFY PACKET DEVICE command.
						inq_ata_packet = true;
						break;
					default:
						Gsvt::StdErr.textColorSet8(3, true);	// yellow
						if (argv[i][2] == _T('\0')) {
							Gsvt::StdErr.fputs(C_("rpcli", "Warning: no inquiry request specified for '-i'"));
						} else {
							// FIXME: Unicode character on Windows.
							Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Warning: skipping unknown inquiry request '{:c}'")),
								(char)argv[i][2]));
						}
						Gsvt::StdErr.textColorReset();
						Gsvt::StdErr.newline();
						Gsvt::StdErr.fflush();
						break;
				}
				break;
#endif /* RP_OS_SCSI_SUPPORTED */

			default:
				// FIXME: Unicode character on Windows.
				Gsvt::StdErr.textColorSet8(3, true);	// yellow
				Gsvt::StdErr.fputs(fmt::format(FRUN(C_("rpcli", "Warning: skipping unknown switch '{:c}'")), (char)argv[i][1]));
				Gsvt::StdErr.textColorReset();
				Gsvt::StdErr.newline();
				Gsvt::StdErr.fflush();
				break;
			}
		} else {
			if (first) {
				first = false;
			} else if (json) {
				cout.flush();
				Gsvt::StdOut.fputs(",\n");
				Gsvt::StdOut.fflush();
			}

			// TODO: Return codes?
#ifdef RP_OS_SCSI_SUPPORTED
			if (inq_scsi) {
				// SCSI INQUIRY command.
				DoScsiInquiry(argv[i], json);
			} else if (inq_ata) {
				// ATA IDENTIFY DEVICE command.
				DoAtaIdentifyDevice(argv[i], json, false);
			} else if (inq_ata_packet) {
				// ATA IDENTIFY PACKET DEVICE command.
				DoAtaIdentifyDevice(argv[i], json, true);
			} else
#endif /* RP_OS_SCSI_SUPPORTED */
			{
				// Regular file.
				DoFile(argv[i], json, extract, lc, flags);
			}

#ifdef RP_OS_SCSI_SUPPORTED
			inq_scsi = false;
			inq_ata = false;
			inq_ata_packet = false;
#endif /* RP_OS_SCSI_SUPPORTED */
			extract.clear();
		}
	}
	if (json) {
		cout.flush();
		Gsvt::StdOut.fputs("]\n");
		Gsvt::StdOut.fflush();
	}

#ifdef _WIN32
	// Shut down GDI+.
	GdiplusHelper::ShutdownGDIPlus(gdipToken);
#endif /* _WIN32 */

	return ret;
}
