/***************************************************************************
 * ROM Properties Page shell extension. (rpcli)                            *
 * rpcli.cpp: Command-line interface for properties.                       *
 *                                                                         *
 * Copyright (c) 2016-2018 by Egor.                                        *
 * Copyright (c) 2016-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "config.rpcli.h"

// OS-specific security options.
#include "rpcli_secure.h"

// librpbase, librpcpu
#include "librpcpu/byteswap_rp.h"
#include "librpbase/config.librpbase.h"
#include "librpbase/RomData.hpp"
#include "librpbase/SystemRegion.hpp"
#include "librpbase/TextFuncs.hpp"
#include "librpbase/img/RpPng.hpp"
#include "librpbase/img/IconAnimData.hpp"
#include "librpbase/TextOut.hpp"
#include "libi18n/i18n.h"
using namespace LibRpBase;

// librpfile
#include "librpfile/config.librpfile.h"
#include "librpfile/FileSystem.hpp"
#include "librpfile/RpFile.hpp"
using namespace LibRpFile;

// libromdata
#include "libromdata/RomDataFactory.hpp"
using LibRomData::RomDataFactory;

// librptexture
#include "librptexture/img/rp_image.hpp"
using LibRpTexture::rp_image;

#ifdef _WIN32
#  include "libwin32common/RpWin32_sdk.h"
#  include "librptexture/img/GdiplusHelper.hpp"
#endif /* _WIN32 */

#ifdef ENABLE_DECRYPTION
# include "verifykeys.hpp"
#endif /* ENABLE_DECRYPTION */
#include "device.hpp"

// OS-specific userdirs
#ifdef _WIN32
# include "libwin32common/userdirs.hpp"
# define OS_NAMESPACE LibWin32Common
#else
# include "libunixcommon/userdirs.hpp"
# define OS_NAMESPACE LibUnixCommon
#endif
#include "tcharx.h"

// C includes.
#include <stdlib.h>

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>

// C++ includes.
#include <fstream>
#include <iostream>
#include <locale>
#include <string>
#include <vector>
using std::cout;
using std::cerr;
using std::endl;
using std::locale;
using std::ofstream;
using std::string;
using std::vector;

#include "libi18n/config.libi18n.h"
#if defined(_MSC_VER) && defined(ENABLE_NLS)
// MSVC: Exception handling for /DELAYLOAD.
#include "libwin32common/DelayLoadHelper.h"
// DelayLoad test implementation.
#include "libi18n/i18n.h"
DELAYLOAD_TEST_FUNCTION_IMPL1(textdomain, nullptr);
#endif /* defined(_MSC_VER) && defined(ENABLE_NLS) */

struct ExtractParam {
	const char* filename;	// Target filename. Can be null due to argv[argc]
	int imageType;		// Image Type. -1 = iconAnimData, MUST be between -1 and IMG_INT_MAX

	ExtractParam(const char *filename, int imageType)
		: filename(filename), imageType(imageType) { }
};

/**
* Extracts images from romdata
* @param romData RomData containing the images
* @param extract Vector of image extraction parameters
*/
static void ExtractImages(const RomData *romData, vector<ExtractParam>& extract) {
	int supported = romData->supportedImageTypes();
	for (auto &&p : extract) {
		if (!p.filename) continue;
		bool found = false;
		
		if (p.imageType >= 0 && supported & (1U << p.imageType)) {
			// normal image
			const RomData::ImageType imageType =
				static_cast<RomData::ImageType>(p.imageType);
			auto image = romData->image(imageType);
			if (image && image->isValid()) {
				found = true;
				cerr << "-- " <<
					// tr: %1$s == image type name, %2$s == output filename
					rp_sprintf_p(C_("rpcli", "Extracting %1$s into '%2$s'"),
						RomData::getImageTypeName(imageType),
						p.filename) << endl;
				int errcode = RpPng::save(p.filename, image);
				if (errcode != 0) {
					// tr: %1$s == filename, %2%s == error message
					cerr << rp_sprintf_p(C_("rpcli", "Couldn't create file '%1$s': %2$s"),
						p.filename, strerror(-errcode)) << endl;
				} else {
					cerr << "   " << C_("rpcli", "Done") << endl;
				}
			}
		} else if (p.imageType == -1) {
			// iconAnimData image
			auto iconAnimData = romData->iconAnimData();
			if (iconAnimData && iconAnimData->count != 0 && iconAnimData->seq_count != 0) {
				found = true;
				cerr << "-- " << rp_sprintf(C_("rpcli", "Extracting animated icon into '%s'"), p.filename) << endl;
				int errcode = RpPng::save(p.filename, iconAnimData);
				if (errcode == -ENOTSUP) {
					cerr << "   " << C_("rpcli", "APNG not supported, extracting only the first frame") << endl;
					// falling back to outputting the first frame
					errcode = RpPng::save(p.filename, iconAnimData->frames[iconAnimData->seq_index[0]]);
				}
				if (errcode != 0) {
					cerr << "   " <<
						rp_sprintf_p(C_("rpcli", "Couldn't create file '%1$s': %2$s"),
							p.filename, strerror(-errcode)) << endl;
				} else {
					cerr << "   " << C_("rpcli", "Done") << endl;
				}
			}
		}
		if (!found) {
			// TODO: Return an error code?
			if (p.imageType == -1) {
				cerr << "-- " << C_("rpcli", "Animated icon not found") << endl;
			} else {
				const RomData::ImageType imageType =
					static_cast<RomData::ImageType>(p.imageType);
				cerr << "-- " <<
					rp_sprintf(C_("rpcli", "Image '%s' not found"),
						RomData::getImageTypeName(imageType)) << endl;
			}
		}
	}
}

/**
 * Shows info about file
 * @param filename ROM filename
 * @param json Is program running in json mode?
 * @param extract Vector of image extraction parameters
 * @param languageCode Language code. (0 for default)
 * @param skipInternalImages If true, skip internal image processing.
 */
static void DoFile(const char *filename, bool json, vector<ExtractParam>& extract,
	uint32_t languageCode = 0, bool skipInternalImages = false)
{
	cerr << "== " << rp_sprintf(C_("rpcli", "Reading file '%s'..."), filename) << endl;
	RpFile *const file = new RpFile(filename, RpFile::FM_OPEN_READ_GZ);
	if (file->isOpen()) {
		RomData *romData = RomDataFactory::create(file);
		if (romData && romData->isValid()) {
			if (json) {
				cerr << "-- " << C_("rpcli", "Outputting JSON data") << endl;
				cout << JSONROMOutput(romData, languageCode, skipInternalImages) << endl;
			} else {
				cout << ROMOutput(romData, languageCode, skipInternalImages) << endl;
			}

			ExtractImages(romData, extract);
		} else {
			cerr << "-- " << C_("rpcli", "ROM is not supported") << endl;
			if (json) cout << "{\"error\":\"rom is not supported\"}" << endl;
		}

		UNREF(romData);
	} else {
		cerr << "-- " << rp_sprintf(C_("rpcli", "Couldn't open file: %s"), strerror(file->lastError())) << endl;
		if (json) cout << "{\"error\":\"couldn't open file\",\"code\":" << file->lastError() << "}" << endl;
	}
	file->unref();
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
			buf += (char)(lc & 0xFF);
		}
	}
	cout << rp_sprintf(C_("rpcli", "System language code: %s"),
		(!buf.empty() ? buf.c_str() : C_("rpcli", "0 (this is a bug!)")));
	cout << endl;

	uint32_t cc = __swab32(SystemRegion::getCountryCode());
	buf.clear();
	if (cc != 0) {
		for (unsigned int i = 4; i > 0; i--, cc >>= 8) {
			if ((cc & 0xFF) == 0)
				continue;
			buf += (char)(cc & 0xFF);
		}
	}
	cout << rp_sprintf(C_("rpcli", "System country code: %s"),
		(!buf.empty() ? buf.c_str() : C_("rpcli", "0 (this is a bug!)")));
	cout << endl;

	// Extra line. (TODO: Only if multiple commands are specified.)
	cout << endl;
}

/**
 * Print pathname information.
 */
static void PrintPathnames(void)
{
	cout << rp_sprintf("User's home directory:   ") << OS_NAMESPACE::getHomeDirectory() << '\n';
	cout << rp_sprintf("User's cache directory:  ") << OS_NAMESPACE::getCacheDirectory() << '\n';
	cout << rp_sprintf("User's config directory: ") << OS_NAMESPACE::getConfigDirectory() << '\n';
	cout << '\n';
	cout << rp_sprintf("RP cache directory:      ") << FileSystem::getCacheDirectory() << '\n';
	cout << rp_sprintf("RP config directory:     ") << FileSystem::getConfigDirectory() << '\n';

	// Extra line. (TODO: Only if multiple commands are specified.)
	cout << endl;
}

#ifdef RP_OS_SCSI_SUPPORTED
/**
 * Run a SCSI INQUIRY command on a device.
 * @param filename Device filename
 * @param json Is program running in json mode?
 */
static void DoScsiInquiry(const char *filename, bool json)
{
	cerr << "== " << rp_sprintf(C_("rpcli", "Opening device file '%s'..."), filename) << endl;
	RpFile *const file = new RpFile(filename, RpFile::FM_OPEN_READ_GZ);
	if (file->isOpen()) {
		// TODO: Check for unsupported devices? (Only CD-ROM is supported.)
		if (file->isDevice()) {
			if (json) {
				cerr << "-- " << C_("rpcli", "Outputting JSON data") << endl;
				// TODO: JSONScsiInquiry
				//cout << JSONScsiInquiry(file) << endl;
			} else {
				cout << ScsiInquiry(file) << endl;
			}
		} else {
			cerr << "-- " << C_("rpcli", "Not a device file") << endl;
			if (json) cout << "{\"error\":\"Not a device file\"}" << endl;
		}
	} else {
		cerr << "-- " << rp_sprintf(C_("rpcli", "Couldn't open file: %s"), strerror(file->lastError())) << endl;
		if (json) cout << "{\"error\":\"couldn't open file\",\"code\":" << file->lastError() << "}" << endl;
	}
	file->unref();
}

/**
 * Run an ATA IDENTIFY DEVICE command on a device.
 * @param filename Device filename
 * @param json Is program running in json mode?
 * @param packet If true, use ATA IDENTIFY PACKET.
 */
static void DoAtaIdentifyDevice(const char *filename, bool json, bool packet)
{
	cerr << "== " << rp_sprintf(C_("rpcli", "Opening device file '%s'..."), filename) << endl;
	RpFile *const file = new RpFile(filename, RpFile::FM_OPEN_READ_GZ);
	if (file->isOpen()) {
		// TODO: Check for unsupported devices? (Only CD-ROM is supported.)
		if (file->isDevice()) {
			if (json) {
				cerr << "-- " << C_("rpcli", "Outputting JSON data") << endl;
				// TODO: JSONAtaIdentifyDevice
				//cout << JSONAtaIdentifyDevice(file) << endl;
			} else {
				cout << AtaIdentifyDevice(file, packet) << endl;
			}
		} else {
			cerr << "-- " << C_("rpcli", "Not a device file") << endl;
			if (json) cout << "{\"error\":\"Not a device file\"}" << endl;
		}
	} else {
		cerr << "-- " << rp_sprintf(C_("rpcli", "Couldn't open file: %s"), strerror(file->lastError())) << endl;
		if (json) cout << "{\"error\":\"couldn't open file\",\"code\":" << file->lastError() << "}" << endl;
	}
	file->unref();
}
#endif /* RP_OS_SCSI_SUPPORTED */

int RP_C_API main(int argc, char *argv[])
{
	// Enable security options.
	rpcli_do_security_options();

	// Set the C and C++ locales.
	locale::global(locale(""));

#if defined(_MSC_VER) && defined(ENABLE_NLS)
	// Delay load verification.
	// TODO: Only if linked with /DELAYLOAD?
	if (DelayLoad_test_textdomain() != 0) {
		// Delay load failed.
		// TODO: Use a CMake macro for the soversion?
		_fputts(_T("*** ERROR: ") LIBGNUINTL_DLL _T(" could not be loaded.\n\n")
			_T("This build of rom-properties has localization enabled,\n")
			_T("which requires the use of GNU gettext.\n\n")
			_T("Please redownload rom-properties and copy the\n")
			LIBGNUINTL_DLL _T(" file to the installation directory.\n"),
			stderr);
		return EXIT_FAILURE;
	}
#endif /* defined(_MSC_VER) && defined(ENABLE_NLS) */

	// Initialize i18n.
	rp_i18n_init();

	if(argc < 2){
#ifdef ENABLE_DECRYPTION
		cerr << C_("rpcli", "Usage: rpcli [-k] [-c] [-p] [-j] [-l lang] [[-x[b]N outfile]... [-a apngoutfile] filename]...") << '\n';
		cerr << "  -k:   " << C_("rpcli", "Verify encryption keys in keys.conf.") << '\n';
#else /* !ENABLE_DECRYPTION */
		cerr << C_("rpcli", "Usage: rpcli [-c] [-p] [-j] [-l lang] [[-x[b]N outfile]... [-a apngoutfile] filename]...") << '\n';
#endif /* ENABLE_DECRYPTION */
		cerr << "  -c:   " << C_("rpcli", "Print system region information.") << '\n';
		cerr << "  -p:   " << C_("rpcli", "Print system path information.") << '\n';
		cerr << "  -j:   " << C_("rpcli", "Use JSON output format.") << '\n';
		cerr << "  -l:   " << C_("rpcli", "Retrieve the specified language from the ROM image.") << '\n';
		cerr << "  -xN:  " << C_("rpcli", "Extract image N to outfile in PNG format.") << '\n';
		cerr << "  -a:   " << C_("rpcli", "Extract the animated icon to outfile in APNG format.") << '\n';
		cerr << '\n';
#ifdef RP_OS_SCSI_SUPPORTED
		cerr << C_("rpcli", "Special options for devices:") << '\n';
		cerr << "  -is:   " << C_("rpcli", "Run a SCSI INQUIRY command.") << '\n';
		cerr << "  -ia:   " << C_("rpcli", "Run an ATA IDENTIFY DEVICE command.") << '\n';
		cerr << "  -ip:   " << C_("rpcli", "Run an ATA IDENTIFY PACKET DEVICE command.") << '\n';
		cerr << '\n';
#endif /* RP_OS_SCSI_SUPPORTED */
		cerr << C_("rpcli", "Examples:") << '\n';
		cerr << "* rpcli s3.gen" << '\n';
		cerr << "\t " << C_("rpcli", "displays info about s3.gen") << '\n';
		cerr << "* rpcli -x0 icon.png pokeb2.nds" << '\n';
		cerr << "\t " << C_("rpcli", "extracts icon from pokeb2.nds") << endl;

		// Since we didn't do anything, return a failure code.
		return EXIT_FAILURE;
	}
	
	assert(RomData::IMG_INT_MIN == 0);
	// DoFile parameters
	bool json = false;
	vector<ExtractParam> extract;

	for (int i = 1; i < argc; i++) { // figure out the json mode in advance
		if (argv[i][0] == '-' && argv[i][1] == 'j') {
			json = true;
		}
	}
	if (json) cout << "[\n";

#ifdef _WIN32
	// Initialize GDI+.
	const ULONG_PTR gdipToken = GdiplusHelper::InitGDIPlus();
	assert(gdipToken != 0);
	if (gdipToken == 0) {
		cerr << "*** ERROR: GDI+ initialization failed." << endl;
		return -EIO;
	}
#endif /* _WIN32 */

#ifdef RP_OS_SCSI_SUPPORTED
	bool inq_scsi = false;
	bool inq_ata = false;
	bool inq_ata_packet = false;
#endif /* RP_OS_SCSI_SUPPORTED */
	uint32_t languageCode = 0;
	bool skipInternalImages = false;
	bool first = true;
	int ret = 0;
	for (int i = 1; i < argc; i++){
		if (argv[i][0] == '-'){
			switch (argv[i][1]) {
#ifdef ENABLE_DECRYPTION
			case 'k': {
				// Verify encryption keys.
				static bool hasVerifiedKeys = false;
				if (!hasVerifiedKeys) {
					hasVerifiedKeys = true;
					ret = VerifyKeys();
				}
				break;
			}
#endif /* ENABLE_DECRYPTION */
			case 'c':
				// Print the system region information.
				PrintSystemRegion();
				break;
			case 'p':
				// Print pathnames.
				PrintPathnames();
				break;
			case 'l': {
				// Language code.
				// NOTE: Actual language may be immediately after 'l',
				// or it might be a completely separate argument.
				// NOTE 2: Allowing multiple language codes, since the
				// language code affects files specified *after* it.
				const char *s_lang;
				if (argv[i][2] == '\0') {
					// Separate argument.
					s_lang = argv[i+1];
					i++;
				} else {
					// Same argument.
					s_lang = &argv[i][2];
				}

				// Parse the language code.
				uint32_t lc = 0;
				int pos;
				for (pos = 0; pos < 4 && s_lang[pos] != '\0'; pos++) {
					lc <<= 8;
					lc |= (uint8_t)s_lang[pos];
				}
				if (pos == 4 && s_lang[pos] != '\0') {
					// Invalid language code.
					cerr << rp_sprintf(C_("rpcli", "Warning: ignoring invalid language code '%s'"), s_lang) << endl;
					break;
				}

				// Language code set.
				languageCode = lc;
				break;
			}
			case 'K': {
				// Skip internal images. (NOTE: Not documented.)
				skipInternalImages = true;
				break;
			}
			case 'x': {
				long num = atol(argv[i] + 2);
				if (num<RomData::IMG_INT_MIN || num>RomData::IMG_INT_MAX) {
					cerr << rp_sprintf(C_("rpcli", "Warning: skipping unknown image type %ld"), num) << endl;
					i++; continue;
				}
				extract.emplace_back(ExtractParam(argv[++i], num));
				break;
			}
			case 'a':
				extract.emplace_back(ExtractParam(argv[++i], -1));
				break;
			case 'j': // do nothing
				break;
#ifdef RP_OS_SCSI_SUPPORTED
			case 'i':
				// These commands take precedence over the usual rpcli functionality.
				switch (argv[i][2]) {
					case 's':
						// SCSI INQUIRY command.
						inq_scsi = true;
						break;
					case 'a':
						// ATA IDENTIFY DEVICE command.
						inq_ata = true;
						break;
					case 'p':
						// ATA IDENTIFY PACKET DEVICE command.
						inq_ata_packet = true;
						break;
					default:
						if (argv[i][2] == '\0') {
							cerr << C_("rpcli", "Warning: no inquiry request specified for '-i'") << endl;
						} else {
							cerr << rp_sprintf(C_("rpcli", "Warning: skipping unknown inquiry request '%c'"), argv[i][2]) << endl;
						}
						break;
				}
				break;
#endif /* RP_OS_SCSI_SUPPORTED */
			default:
				cerr << rp_sprintf(C_("rpcli", "Warning: skipping unknown switch '%c'"), argv[i][1]) << endl;
				break;
			}
		} else {
			if (first) first = false;
			else if (json) cout << "," << endl;

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
				DoFile(argv[i], json, extract, languageCode, skipInternalImages);
			}

#ifdef RP_OS_SCSI_SUPPORTED
			inq_scsi = false;
			inq_ata = false;
			inq_ata_packet = false;
#endif /* RP_OS_SCSI_SUPPORTED */
			extract.clear();
		}
	}
	if (json) cout << ']' << endl;

#ifdef _WIN32
	// Shut down GDI+.
	GdiplusHelper::ShutdownGDIPlus(gdipToken);
#endif /* _WIN32 */

	return ret;
}
