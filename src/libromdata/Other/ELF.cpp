/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * ELF.cpp: Executable and Linkable Format reader.                         *
 *                                                                         *
 * Copyright (c) 2016-2021 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "ELF.hpp"
#include "data/ELFData.hpp"
#include "elf_structs.h"

// librpbase, librpfile
using namespace LibRpBase;
using LibRpFile::IRpFile;

// cinttypes was added in MSVC 2013.
// For older versions, we'll need to manually define PRIX64.
// TODO: Split into a separate header file?
// FIXME: MinGW-w64 on AppVeyor seems to use the C99 "%llx" even though
// we set -D__USE_MINGW_ANSI_STDIO=0, so it ends up failing with a
// format warning.
// FIXME: What if we use MinGW-w64 with later MSVCRT or UCRT?
#define __STDC_FORMAT_MACROS
#ifdef _WIN32
#  if _MSC_VER >= 1800
#    include <cinttypes>
#  else
#    ifdef PRIX64
#      undef PRIX64
#    endif
#    define PRIX64 "I64X"
#  endif
#else /* !_WIN32 */
#  include <cinttypes>
#endif /* _WIN32 */

// C++ STL classes.
using std::string;
using std::unique_ptr;
using std::vector;

// Uninitialized vector class.
// Reference: http://andreoffringa.org/?q=uvector
#include "uvector.h"

namespace LibRomData {

class ELFPrivate final : public RomDataPrivate
{
	public:
		ELFPrivate(ELF *q, LibRpFile::IRpFile *file);

	private:
		typedef RomDataPrivate super;
		RP_DISABLE_COPY(ELFPrivate)

	public:
		/** RomDataInfo **/
		static const char *const exts[];
		static const char *const mimeTypes[];
		static const RomDataInfo romDataInfo;

	public:
		// ELF format.
		enum class Elf_Format {
			Unknown	= -1,

			_32LSB	= 0,
			_64LSB	= 1,
			_32MSB	= 2,
			_64MSB	= 3,

			Max,

			// Host/swap endian formats.

#if SYS_BYTEORDER == SYS_LIL_ENDIAN
			#define ELFDATAHOST ELFDATA2LSB
			_32HOST	= _32LSB,
			_64HOST	= _64LSB,
			_32SWAP	= _32MSB,
			_64SWAP	= _64MSB,
#else /* SYS_BYTEORDER == SYS_BIG_ENDIAN */
			#define ELFDATAHOST ELFDATA2MSB
			_32HOST	= _32MSB,
			_64HOST	= _64MSB,
			_32SWAP	= _32LSB,
			_64SWAP	= _64LSB,
#endif
		};
		Elf_Format elfFormat;

		// ELF header.
		union {
			Elf_PrimaryEhdr primary;
			Elf32_Ehdr elf32;
			Elf64_Ehdr elf64;
		} Elf_Header;

		// Header location and size.
		struct hdr_info_t {
			off64_t addr;
			uint64_t size;
		};

		/**
		 * Read an ELF program header.
		 * @param phbuf	[in] Pointer to program header.
		 * @return Header information.
		 */
		hdr_info_t readProgramHeader(const uint8_t *phbuf);

		// Program Header information.
		bool hasCheckedPH;	// Have we checked program headers yet?
		bool isPie;		// Is this a position-independent executable?
		bool isWiiU;		// Is this a Wii U executable?

		string interpreter;	// PT_INTERP value

		// PT_DYNAMIC
		hdr_info_t pt_dynamic;	// If addr == 0, not dynamic.

		// Section Header information.
		bool hasCheckedSH;	// Have we checked section headers yet?
		string osVersion;	// Operating system version.

		ao::uvector<uint8_t> build_id;	// GNU `ld` build ID. (raw data)
		const char *build_id_type;	// Build ID type.

		/**
		 * Byteswap a uint32_t value from ELF to CPU.
		 * @param x Value to swap.
		 * @return Swapped value.
		 */
		inline uint32_t elf32_to_cpu(uint32_t x)
		{
			return (Elf_Header.primary.e_data == ELFDATAHOST)
				? x
				: __swab32(x);
		}

		/**
		 * Byteswap a uint64_t value from ELF to CPU.
		 * @param x Value to swap.
		 * @return Swapped value.
		 */
		inline uint64_t elf64_to_cpu(uint64_t x)
		{
			return (Elf_Header.primary.e_data == ELFDATAHOST)
				? x
				: __swab64(x);
		}

		/**
		 * Check program headers.
		 * @return 0 on success; non-zero on error.
		 */
		int checkProgramHeaders(void);

		/**
		 * Check section headers.
		 * @return 0 on success; non-zero on error.
		 */
		int checkSectionHeaders(void);

		/**
		 * Add PT_DYNAMIC fields.
		 * @return 0 on success; non-zero on error.
		 */
		int addPtDynamicFields(void);
};

ROMDATA_IMPL(ELF)

/** ELFPrivate **/

/* RomDataInfo */
const char *const ELFPrivate::exts[] = {
	//".",		// FIXME: Does this work for files with no extension?
	".elf",		// Common for Wii homebrew.
	".so",		// Shared libraries. (TODO: Versioned .so files.)
	".o",		// Relocatable object files.
	".core",	// Core dumps.
	".debug",	// Split debug files.

	// Wii U
	".rpx",		// Cafe OS executable
	".rpl",		// Cafe OS library

	nullptr
};
const char *const ELFPrivate::mimeTypes[] = {
	// Unofficial MIME types from FreeDesktop.org.
	"application/x-object",
	"application/x-executable",
	"application/x-sharedlib",
	"application/x-core",

	nullptr
};
const RomDataInfo ELFPrivate::romDataInfo = {
	"ELF", exts, mimeTypes
};

ELFPrivate::ELFPrivate(ELF *q, IRpFile *file)
	: super(q, file, &romDataInfo)
	, elfFormat(Elf_Format::Unknown)
	, hasCheckedPH(false)
	, isPie(false)
	, isWiiU(false)
	, hasCheckedSH(false)
	, build_id_type(nullptr)
{
	// Clear the structs.
	memset(&Elf_Header, 0, sizeof(Elf_Header));
	memset(&pt_dynamic, 0, sizeof(pt_dynamic));
}

/**
 * Read an ELF program header.
 * @param phbuf	[in] Pointer to program header.
 * @return Header information.
 */
ELFPrivate::hdr_info_t ELFPrivate::readProgramHeader(const uint8_t *phbuf)
{
	hdr_info_t info;

	if (Elf_Header.primary.e_class == ELFCLASS64) {
		const Elf64_Phdr *const phdr = reinterpret_cast<const Elf64_Phdr*>(phbuf);
		if (Elf_Header.primary.e_data == ELFDATAHOST) {
			info.addr = phdr->p_offset;
			info.size = phdr->p_filesz;
		} else {
			info.addr = __swab64(phdr->p_offset);
			info.size = __swab64(phdr->p_filesz);
		}
	} else {
		const Elf32_Phdr *const phdr = reinterpret_cast<const Elf32_Phdr*>(phbuf);
		if (Elf_Header.primary.e_data == ELFDATAHOST) {
			info.addr = phdr->p_offset;
			info.size = phdr->p_filesz;
		} else {
			info.addr = __swab32(phdr->p_offset);
			info.size = __swab32(phdr->p_filesz);
		}
	}

	return info;
}

/**
 * Check program headers.
 * @return 0 on success; non-zero on error.
 */
int ELFPrivate::checkProgramHeaders(void)
{
	if (hasCheckedPH) {
		// Already checked.
		return 0;
	}

	// Now checking...
	hasCheckedPH = true;

	// Read the program headers.
	// PIE executables have a PT_INTERP header.
	// Shared libraries do not.
	// (NOTE: glibc's libc.so.6 *does* have PT_INTERP...)
	off64_t e_phoff;
	unsigned int e_phnum;
	unsigned int phsize;
	uint8_t phbuf[sizeof(Elf64_Phdr)];

	if (Elf_Header.primary.e_class == ELFCLASS64) {
		e_phoff = static_cast<off64_t>(Elf_Header.elf64.e_phoff);
		e_phnum = Elf_Header.elf64.e_phnum;
		phsize = sizeof(Elf64_Phdr);
	} else {
		e_phoff = static_cast<off64_t>(Elf_Header.elf32.e_phoff);
		e_phnum = Elf_Header.elf32.e_phnum;
		phsize = sizeof(Elf32_Phdr);
	}

	if (e_phoff == 0 || e_phnum == 0) {
		// No program headers. Can't determine anything...
		return 0;
	}

	int ret = file->seek(e_phoff);
	if (ret != 0) {
		// Seek error.
		return ret;
	}

	// Read all of the program header entries.
	const bool isHostEndian = (Elf_Header.primary.e_data == ELFDATAHOST);
	for (; e_phnum > 0; e_phnum--) {
		size_t size = file->read(phbuf, phsize);
		if (size != phsize) {
			// Read error.
			break;
		}

		// Check the type.
		uint32_t p_type;
		memcpy(&p_type, phbuf, sizeof(p_type));
		if (!isHostEndian) {
			p_type = __swab32(p_type);
		}

		switch (p_type) {
			case PT_INTERP: {
				// If the file type is ET_DYN, this is a PIE executable.
				isPie = (Elf_Header.primary.e_type == ET_DYN);

				// Get the interpreter name.
				hdr_info_t info = readProgramHeader(phbuf);

				// Sanity check: Interpreter must be 256 characters or less.
				// NOTE: Interpreter should be NULL-terminated.
				if (info.size <= 256) {
					char buf[256];
					const off64_t prevoff = file->tell();
					size = file->seekAndRead(info.addr, buf, info.size);
					if (size != info.size) {
						// Seek and/or read error.
						return -EIO;
					}
					ret = file->seek(prevoff);
					if (ret != 0) {
						// Seek error.
						return ret;
					}

					// Remove trailing NULLs.
					while (info.size > 0 && buf[info.size-1] == 0) {
						info.size--;
					}

					if (info.size > 0) {
						interpreter.assign(buf, info.size);
					}
				}

				break;
			}

			case PT_DYNAMIC:
				// Executable is dynamically linked.
				// Save the header information for later.
				pt_dynamic = readProgramHeader(phbuf);
				break;

			default:
				break;
		}
	}

	// Program headers checked.
	return 0;
}

/**
 * Check section headers.
 * @return 0 on success; non-zero on error.
 */
int ELFPrivate::checkSectionHeaders(void)
{
	if (hasCheckedSH) {
		// Already checked.
		return 0;
	}

	// Now checking...
	hasCheckedSH = true;

	// Read the section headers.
	off64_t e_shoff;
	unsigned int e_shnum;
	unsigned int shsize;
	uint8_t shbuf[sizeof(Elf64_Shdr)];

	if (Elf_Header.primary.e_class == ELFCLASS64) {
		e_shoff = static_cast<off64_t>(Elf_Header.elf64.e_shoff);
		e_shnum = Elf_Header.elf64.e_shnum;
		shsize = sizeof(Elf64_Shdr);
	} else {
		e_shoff = static_cast<off64_t>(Elf_Header.elf32.e_shoff);
		e_shnum = Elf_Header.elf32.e_shnum;
		shsize = sizeof(Elf32_Shdr);
	}

	if (e_shoff == 0 || e_shnum == 0) {
		// No section headers. Can't determine anything...
		return 0;
	}

	int ret = file->seek(e_shoff);
	if (ret != 0) {
		// Seek error.
		return ret;
	}

	// Read all of the section header entries.
	const bool isHostEndian = (Elf_Header.primary.e_data == ELFDATAHOST);
	for (; e_shnum > 0; e_shnum--) {
		size_t size = file->read(shbuf, shsize);
		if (size != shsize) {
			// Read error.
			break;
		}

		// Check the type.
		uint32_t s_type;
		memcpy(&s_type, &shbuf[4], sizeof(s_type));
		if (!isHostEndian) {
			s_type = __swab32(s_type);
		}

		// Only NOTEs are supported right now.
		if (s_type != SHT_NOTE)
			continue;

		// Get the note address and size.
		off64_t int_addr;
		uint64_t int_size;
		if (Elf_Header.primary.e_class == ELFCLASS64) {
			const Elf64_Shdr *const shdr = reinterpret_cast<const Elf64_Shdr*>(shbuf);
			if (Elf_Header.primary.e_data == ELFDATAHOST) {
				int_addr = shdr->sh_offset;
				int_size = shdr->sh_size;
			} else {
				int_addr = __swab64(shdr->sh_offset);
				int_size = __swab64(shdr->sh_size);
			}
		} else {
			const Elf32_Shdr *const shdr = reinterpret_cast<const Elf32_Shdr*>(shbuf);
			if (Elf_Header.primary.e_data == ELFDATAHOST) {
				int_addr = shdr->sh_offset;
				int_size = shdr->sh_size;
			} else {
				int_addr = __swab32(shdr->sh_offset);
				int_size = __swab32(shdr->sh_size);
			}
		}

		// Sanity check: Note must be 256 bytes or less,
		// and must be greater than sizeof(Elf32_Nhdr).
		// NOTE: Elf32_Nhdr and Elf64_Nhdr are identical.
		if (int_size < sizeof(Elf32_Nhdr) || int_size > 256) {
			// Out of range. Ignore it.
			continue;
		}

		uint8_t buf[256];
		const off64_t prevoff = file->tell();
		size = file->seekAndRead(int_addr, buf, int_size);
		if (size != int_size) {
			// Seek and/or read error.
			return -EIO;
		}
		ret = file->seek(prevoff);
		if (ret != 0) {
			// Seek error.
			return ret;
		}

		// Parse the note.
		Elf32_Nhdr *const nhdr = reinterpret_cast<Elf32_Nhdr*>(buf);
		if (Elf_Header.primary.e_data != ELFDATAHOST) {
			// Byteswap the fields.
			nhdr->n_namesz = __swab32(nhdr->n_namesz);
			nhdr->n_descsz = __swab32(nhdr->n_descsz);
			nhdr->n_type   = __swab32(nhdr->n_type);
		}

		if (nhdr->n_namesz == 0 || nhdr->n_descsz == 0) {
			// No name or description...
			continue;
		}

		if (int_size < sizeof(Elf32_Nhdr) + nhdr->n_namesz + nhdr->n_descsz) {
			// Section is too small.
			continue;
		}

		const char *const pName = reinterpret_cast<const char*>(&buf[sizeof(Elf32_Nhdr)]);
		const uint8_t *const pData = &buf[sizeof(Elf32_Nhdr) + nhdr->n_namesz];
		switch (nhdr->n_type) {
			case NT_GNU_ABI_TAG:
				// GNU ABI tag.
				if (nhdr->n_namesz == 5 && !strcmp(pName, "SuSE")) {
					// SuSE Linux
					if (nhdr->n_descsz < 2) {
						// Header is too small...
						break;
					}
					osVersion = rp_sprintf("SuSE Linux %u.%u", pData[0], pData[1]);
				} else if (nhdr->n_namesz == 4 && !strcmp(pName, ELF_NOTE_GNU)) {
					// GNU system
					if (nhdr->n_descsz < sizeof(uint32_t)*4) {
						// Header is too small...
						break;
					}
					uint32_t desc[4];
					memcpy(desc, pData, sizeof(desc));

					const uint32_t os_id = elf32_to_cpu(desc[0]);
					static const char *const os_tbl[] = {
						"Linux", "Hurd", "Solaris", "kFreeBSD", "kNetBSD"
					};

					const char *s_os;
					if (os_id < ARRAY_SIZE(os_tbl)) {
						s_os = os_tbl[os_id];
					} else {
						s_os = "<unknown>";
					}

					osVersion = rp_sprintf("GNU/%s %u.%u.%u",
						s_os, elf32_to_cpu(desc[1]),
						elf32_to_cpu(desc[2]), elf32_to_cpu(desc[3]));
				} else if (nhdr->n_namesz == 7 && !strcmp(pName, "NetBSD")) {
					// Check if the version number is valid.
					// Older versions kept this as 199905.
					// Newer versions use __NetBSD_Version__.
					if (nhdr->n_descsz < sizeof(uint32_t)) {
						// Header is too small...
						break;
					}

					uint32_t desc;
					memcpy(&desc, pData, sizeof(desc));
					desc = elf32_to_cpu(desc);

					if (desc > 100000000U) {
						const uint32_t ver_patch = (desc / 100) % 100;
						uint32_t ver_rel = (desc / 10000) % 100;
						const uint32_t ver_min = (desc / 1000000) % 100;
						const uint32_t ver_maj = desc / 100000000;
						osVersion = rp_sprintf("NetBSD %u.%u", ver_maj, ver_min);
						if (ver_rel == 0 && ver_patch != 0) {
							osVersion += rp_sprintf(".%u", ver_patch);
						} else if (ver_rel != 0) {
							while (ver_rel > 26) {
								osVersion += 'Z';
								ver_rel -= 26;
							}
							osVersion += ('A' + ver_rel - 1);
						}
					} else {
						// No version number.
						osVersion = "NetBSD";
					}
				} else if (nhdr->n_namesz == 8 && !strcmp(pName, "FreeBSD")) {
					if (nhdr->n_descsz < sizeof(uint32_t)) {
						// Header is too small...
						break;
					}

					uint32_t desc;
					memcpy(&desc, pData, sizeof(desc));
					desc = elf32_to_cpu(desc);

					if (desc == 460002) {
						osVersion = "FreeBSD 4.6.2";
					} else if (desc < 460100) {
						osVersion = rp_sprintf("FreeBSD %u.%u",
							desc / 100000, desc / 10000 % 10);
						if (desc / 1000 % 10 > 0) {
							osVersion += rp_sprintf(".%u", desc / 1000 % 10);
						}
						if ((desc % 1000 > 0) || (desc % 100000 == 0)) {
							osVersion += rp_sprintf(" (%u)", desc);
						}
					} else if (desc < 500000) {
						osVersion = rp_sprintf("FreeBSD %u.%u",
							desc / 100000, desc / 10000 % 10 + desc / 1000 % 10);
						if (desc / 100 % 10 > 0) {
							osVersion += rp_sprintf(" (%u)", desc);
						} else if (desc / 10 % 10 > 0) {
							osVersion += rp_sprintf(".%u", desc / 10 % 10);
						}
					} else {
						osVersion = rp_sprintf("FreeBSD %u.%u",
							desc / 100000, desc / 1000 % 100);
						if ((desc / 100 % 10 > 0) || (desc % 100000 / 100 == 0)) {
							osVersion += rp_sprintf(" (%u)", desc);
						} else if (desc / 10 % 10 > 0) {
							osVersion += rp_sprintf(".%u", desc / 10 % 10);
						}
					}
				} else if (nhdr->n_namesz == 8 && !strcmp(pName, "OpenBSD")) {
					osVersion = "OpenBSD";
				} else if (nhdr->n_namesz == 10 && !strcmp(pName, "DragonFly")) {
					if (nhdr->n_descsz < sizeof(uint32_t)) {
						// Header is too small...
						break;
					}

					uint32_t desc;
					memcpy(&desc, pData, sizeof(desc));
					desc = elf32_to_cpu(desc);

					osVersion = rp_sprintf("DragonFlyBSD %u.%u.%u",
						desc / 100000, desc / 10000 % 10, desc % 10000);
				}
				break;

			case NT_GNU_BUILD_ID:
				if (nhdr->n_namesz != 4 || strcmp(pName, ELF_NOTE_GNU) != 0) {
					// Not a GNU note.
					break;
				}

				// Build ID.
				switch (nhdr->n_descsz) {
					case 8:
						build_id_type = "xxHash";
						break;
					case 16:
						build_id_type = "md5/uuid";
						break;
					case 20:
						build_id_type = "sha1";
						break;
					default:
						build_id_type = nullptr;
						break;
				}

				// Hexdump will be done when parsing the data.
				build_id.resize(nhdr->n_descsz);
				memcpy(build_id.data(), pData, nhdr->n_descsz);
				break;

			default:
				break;
		}
	}

	// Program headers checked.
	return 0;
}

/**
 * Add PT_DYNAMIC fields.
 * @return 0 on success; non-zero on error.
 */
int ELFPrivate::addPtDynamicFields(void)
{
	if (isWiiU || pt_dynamic.addr == 0) {
		// Not a dynamic object.
		// (Wii U dynamic objects don't work the same way as
		// standard POSIX dynamic objects.)
		return -1;
	}

	if (pt_dynamic.size > 1U*1024*1024) {
		// PT_DYNAMIC is larger than 1 MB.
		// That's no good.
		return -2;
	}

	// Read the header.
	const unsigned int sz_to_read = static_cast<unsigned int>(pt_dynamic.size);
	unique_ptr<uint8_t[]> pt_dyn_buf(new uint8_t[sz_to_read]);
	size_t size = file->seekAndRead(pt_dynamic.addr, pt_dyn_buf.get(), sz_to_read);
	if (size != sz_to_read) {
		// Read error.
		return -3;
	}

	// Process headers.
	// NOTE: Separate loops for 32-bit vs. 64-bit.
	bool has_DT_FLAGS = false, has_DT_FLAGS_1 = false;
	uint32_t val_DT_FLAGS = 0, val_DT_FLAGS_1 = 0;

	// TODO: DT_RPATH/DT_RUNPATH
	// Requires string table parsing too?
	if (Elf_Header.primary.e_class == ELFCLASS64) {
		const Elf64_Dyn *phdr = reinterpret_cast<const Elf64_Dyn*>(pt_dyn_buf.get());
		const Elf64_Dyn *const phdr_end = phdr + (size / sizeof(*phdr));
		// TODO: Don't allow duplicates?
		for (; phdr < phdr_end; phdr++) {
			Elf64_Sxword d_tag = elf64_to_cpu(phdr->d_tag);
			switch (d_tag) {
				case DT_FLAGS:
					has_DT_FLAGS = true;
					val_DT_FLAGS = static_cast<uint32_t>(elf64_to_cpu(phdr->d_un.d_val));
					break;
				case DT_FLAGS_1:
					has_DT_FLAGS_1 = true;
					val_DT_FLAGS_1 = static_cast<uint32_t>(elf64_to_cpu(phdr->d_un.d_val));
					break;
				default:
					break;
			}
		}
	} else {
		const Elf32_Dyn *phdr = reinterpret_cast<const Elf32_Dyn*>(pt_dyn_buf.get());
		const Elf32_Dyn *const phdr_end = phdr + (size / sizeof(*phdr));
		for (; phdr < phdr_end; phdr++) {
			Elf32_Sword d_tag = elf32_to_cpu(phdr->d_tag);
			switch (d_tag) {
				case DT_FLAGS:
					has_DT_FLAGS = true;
					val_DT_FLAGS = elf32_to_cpu(phdr->d_un.d_val);
					break;
				case DT_FLAGS_1:
					has_DT_FLAGS_1 = true;
					val_DT_FLAGS_1 = elf32_to_cpu(phdr->d_un.d_val);
					break;
				default:
					break;
			}
		}
	}

	if (!has_DT_FLAGS && !has_DT_FLAGS_1) {
		// No relevant PT_DYNAMIC entries.
		return 0;
	}

	// Add the PT_DYNAMIC tab.
	fields->addTab("PT_DYNAMIC");

	if (has_DT_FLAGS) {
		// DT_FLAGS
		static const char *const dt_flags_names[] = {
			// 0x00000000
			"ORIGIN", "SYMBOLIC", "TEXTREL", "BIND_NOW",
			// 0x00000010
			"STATIC_TLS",
		};
		vector<string> *const v_dt_flags_names = RomFields::strArrayToVector(
			dt_flags_names, ARRAY_SIZE(dt_flags_names));
		fields->addField_bitfield("DT_FLAGS",
			v_dt_flags_names, 3, val_DT_FLAGS);
	}

	if (has_DT_FLAGS_1) {
		// DT_FLAGS_1
		// NOTE: Internal-use symbols are left as nullptr.
		static const char *const dt_flags_1_names[] = {
			// 0x00000000
			"Now", "Global", "Group", "NoDelete",
			// 0x00000010
			"LoadFltr", "InitFirst", "NoOpen", "Origin",
			// 0x00000100
			"Direct", nullptr /*"Trans"*/, "Interpose", "NoDefLib",
			// 0x00001000
			"NoDump", "ConfAlt", "EndFiltee", "DispRelDNE",
			// 0x00010000
			"DispRelPND", "NoDirect", nullptr /*"IgnMulDef"*/, nullptr /*"NokSyms"*/,
			// 0x00100000
			nullptr /*"NoHdr"*/, "Edited", nullptr /*"NoReloc"*/, "SymIntpose",
			// 0x01000000
			"GlobAudit", "Singleton", "Stub", "PIE"
		};
		vector<string> *const v_dt_flags_1_names = RomFields::strArrayToVector(
			dt_flags_1_names, ARRAY_SIZE(dt_flags_1_names));
		fields->addField_bitfield("DT_FLAGS_1",
			v_dt_flags_1_names, 3, val_DT_FLAGS_1);
	}

	// We're done here.
	return 0;
}

/** ELF **/

/**
 * Read an ELF executable.
 *
 * A ROM file must be opened by the caller. The file handle
 * will be ref()'d and must be kept open in order to load
 * data from the ROM.
 *
 * To close the file, either delete this object or call close().
 *
 * NOTE: Check isValid() to determine if this is a valid ROM.
 *
 * @param file Open ROM image.
 */
ELF::ELF(IRpFile *file)
	: super(new ELFPrivate(this, file))
{
	// This class handles different types of files.
	// d->fileType will be set later.
	RP_D(ELF);
	d->fileType = FileType::Unknown;

	if (!d->file) {
		// Could not ref() the file handle.
		return;
	}

	// Assume this is a 64-bit ELF executable and read a 64-bit header.
	// 32-bit executables have a smaller header, but they should have
	// more data than just the header.
	d->file->rewind();
	size_t size = d->file->read(&d->Elf_Header, sizeof(d->Elf_Header));
	if (size != sizeof(d->Elf_Header)) {
		UNREF_AND_NULL_NOCHK(d->file);
		return;
	}

	// Check if this executable is supported.
	DetectInfo info;
	info.header.addr = 0;
	info.header.size = sizeof(d->Elf_Header);
	info.header.pData = reinterpret_cast<const uint8_t*>(&d->Elf_Header);
	info.ext = nullptr;	// Not needed for ELF.
	info.szFile = 0;	// Not needed for ELF.
	d->elfFormat = static_cast<ELFPrivate::Elf_Format>(isRomSupported_static(&info));

	d->isValid = ((int)d->elfFormat >= 0);
	if (!d->isValid) {
		// Not an ELF executable.
		UNREF_AND_NULL_NOCHK(d->file);
		return;
	}

	// Swap endianness if needed.
	switch (d->elfFormat) {
		default:
			// Should not get here...
			assert(!"Should not get here...");
			d->isValid = false;
			d->elfFormat = ELFPrivate::Elf_Format::Unknown;
			UNREF_AND_NULL_NOCHK(d->file);
			return;

		case ELFPrivate::Elf_Format::_32HOST:
		case ELFPrivate::Elf_Format::_64HOST:
			// Host-endian. Nothing to do.
			break;

		case ELFPrivate::Elf_Format::_32SWAP: {
			// 32-bit, swapped endian.
			// NOTE: Not swapping the magic number.
			Elf32_Ehdr *const elf32 = &d->Elf_Header.elf32;
			elf32->e_type		= __swab16(elf32->e_type);
			elf32->e_machine	= __swab16(elf32->e_machine);
			elf32->e_version	= __swab32(elf32->e_version);
			elf32->e_entry		= __swab32(elf32->e_entry);
			elf32->e_phoff		= __swab32(elf32->e_phoff);
			elf32->e_shoff		= __swab32(elf32->e_shoff);
			elf32->e_flags		= __swab32(elf32->e_flags);
			elf32->e_ehsize		= __swab16(elf32->e_ehsize);
			elf32->e_phentsize	= __swab16(elf32->e_phentsize);
			elf32->e_phnum		= __swab16(elf32->e_phnum);
			elf32->e_shentsize	= __swab16(elf32->e_shentsize);
			elf32->e_shnum		= __swab16(elf32->e_shnum);
			elf32->e_shstrndx	= __swab16(elf32->e_shstrndx);
			break;
		}

		case ELFPrivate::Elf_Format::_64SWAP: {
			// 64-bit, swapped endian.
			// NOTE: Not swapping the magic number.
			Elf64_Ehdr *const elf64 = &d->Elf_Header.elf64;
			elf64->e_type		= __swab16(elf64->e_type);
			elf64->e_machine	= __swab16(elf64->e_machine);
			elf64->e_version	= __swab32(elf64->e_version);
			elf64->e_entry		= __swab64(elf64->e_entry);
			elf64->e_phoff		= __swab64(elf64->e_phoff);
			elf64->e_shoff		= __swab64(elf64->e_shoff);
			elf64->e_flags		= __swab32(elf64->e_flags);
			elf64->e_ehsize		= __swab16(elf64->e_ehsize);
			elf64->e_phentsize	= __swab16(elf64->e_phentsize);
			elf64->e_phnum		= __swab16(elf64->e_phnum);
			elf64->e_shentsize	= __swab16(elf64->e_shentsize);
			elf64->e_shnum		= __swab16(elf64->e_shnum);
			elf64->e_shstrndx	= __swab16(elf64->e_shstrndx);
			break;
		}
	}

	// Primary ELF header.
	const Elf_PrimaryEhdr *const primary = &d->Elf_Header.primary;

	// Is this a Wii U executable?
	if (primary->e_osabi == ELFOSABI_CAFEOS &&
	    primary->e_osabiversion == 0xFE &&
	    d->elfFormat == ELFPrivate::Elf_Format::_32MSB &&
	    primary->e_machine == EM_PPC)
	{
		// OS ABI and version is 0xCAFE.
		// Assuming this is a Wii U executable.
		// TODO: Also verify that there's no program headers?
		d->isWiiU = true;
		d->pt_dynamic.addr = 1;	// TODO: Properly check this.

		// TODO: Determine different RPX/RPL file types.
		switch (primary->e_type) {
			default:
				// Should not happen...
				d->fileType = FileType::Unknown;
				break;
			case 0xFE01:
				// This matches some homebrew software.
				d->fileType = FileType::Executable;
				d->mimeType = "application/x-executable";	// unofficial
				break;
		}
	} else {
		// Standard ELF executable.
		// Check program and section headers.
		d->checkProgramHeaders();
		d->checkSectionHeaders();

		// Determine the file and MIME types.
		// NOTE: All of these MIME types are present on FreeDesktop.org,
		// but they're technically "unofficial".
		switch (d->Elf_Header.primary.e_type) {
			default:
				// Should not happen...
				d->fileType = FileType::Unknown;
				break;
			case ET_REL:
				d->fileType = FileType::RelocatableObject;
				d->mimeType = "application/x-object";
				break;
			case ET_EXEC:
				d->fileType = FileType::Executable;
				d->mimeType = "application/x-executable";
				break;
			case ET_DYN:
				// This may either be a shared library or a
				// position-independent executable.
				if (d->isPie) {
					d->fileType = FileType::Executable;
					d->mimeType = "application/x-executable";
				} else {
					d->fileType = FileType::SharedLibrary;
					d->mimeType = "application/x-sharedlib";
				}
				break;
			case ET_CORE:
				d->fileType = FileType::CoreDump;
				d->mimeType = "application/x-core";
				break;

			// Special cases
			case 0xFF80: {
				// PS2 IOP Relocatable Executable
				// This is basically a shared library.
				// TODO: Use something like "isWiiU"?
				const uint32_t e_flags = d->Elf_Header.elf32.e_flags;
				if (d->elfFormat == ELFPrivate::Elf_Format::_32LSB &&
				    primary->e_machine == EM_MIPS &&
				    !(e_flags & 0x20) /* O32 */ &&
				    (e_flags >> 28) == 0 /* MIPS-I */)
				{
					// This is likely for PS2 IOP.
					d->fileType = FileType::SharedLibrary;
					d->mimeType = "application/x-sharedlib";
				} else {
					// Unknown type...
					d->fileType = FileType::Unknown;
				}
				break;
			}
		}
	}
}

/** ROM detection functions. **/

/**
 * Is a ROM image supported by this class?
 * @param info DetectInfo containing ROM detection information.
 * @return Class-specific system ID (>= 0) if supported; -1 if not.
 */
int ELF::isRomSupported_static(const DetectInfo *info)
{
	assert(info != nullptr);
	assert(info->header.pData != nullptr);
	assert(info->header.addr == 0);
	if (!info || !info->header.pData ||
	    info->header.addr != 0 ||
	    info->header.size < sizeof(Elf_PrimaryEhdr))
	{
		// Either no detection information was specified,
		// or the header is too small.
		return static_cast<int>(ELFPrivate::Elf_Format::Unknown);
	}

	const Elf_PrimaryEhdr *const pHdr =
		reinterpret_cast<const Elf_PrimaryEhdr*>(info->header.pData);

	// Check the magic number.
	if (pHdr->e_magic != cpu_to_be32(ELF_MAGIC)) {
		// Invalid magic.
		return static_cast<int>(ELFPrivate::Elf_Format::Unknown);
	}

	// Verify the bitness and endianness fields.
	ELFPrivate::Elf_Format elfFormat;
	switch (pHdr->e_data) {
		case ELFDATA2LSB:
			// Little-endian.
			switch (pHdr->e_class) {
				case ELFCLASS32:
					// 32-bit LSB.
					elfFormat = ELFPrivate::Elf_Format::_32LSB;
					break;
				case ELFCLASS64:
					// 64-bit LSB.
					elfFormat = ELFPrivate::Elf_Format::_64LSB;
					break;
				default:
					// Unknown bitness.
					elfFormat = ELFPrivate::Elf_Format::Unknown;
					break;
			}
			break;

		case ELFDATA2MSB:
			// Big-endian.
			switch (pHdr->e_class) {
				case ELFCLASS32:
					// 32-bit MSB.
					elfFormat = ELFPrivate::Elf_Format::_32MSB;
					break;
				case ELFCLASS64:
					// 64-bit MSB.
					elfFormat = ELFPrivate::Elf_Format::_64MSB;
					break;
				default:
					// Unknown bitness.
					elfFormat = ELFPrivate::Elf_Format::Unknown;
					break;
			}
			break;

		default:
			// Unknown endianness.
			elfFormat = ELFPrivate::Elf_Format::Unknown;
			break;
	}

	return static_cast<int>(elfFormat);
}

/**
 * Get the name of the system the loaded ROM is designed for.
 * @param type System name type. (See the SystemName enum.)
 * @return System name, or nullptr if type is invalid.
 */
const char *ELF::systemName(unsigned int type) const
{
	RP_D(const ELF);
	if (!d->isValid || !isSystemNameTypeValid(type))
		return nullptr;

	// ELF has the sam names worldwide, so we can
	// ignore the region selection.
	// TODO: Identify the OS, or list that in the fields instead?
	static_assert(SYSNAME_TYPE_MASK == 3,
		"ELF::systemName() array index optimization needs to be updated.");

	type &= SYSNAME_TYPE_MASK;

	if (d->isWiiU) {
		// This is a Wii U RPX/RPL executable.
		static const char *const sysNames_WiiU[4] = {
			"Nintendo Wii U", "Wii U", "Wii U", nullptr
		};
		return sysNames_WiiU[type];
	}

	// Standard ELF executable.
	static const char *const sysNames[4] = {
		"Executable and Linkable Format", "ELF", "ELF", nullptr
	};

	return sysNames[type];
}

/**
 * Load field data.
 * Called by RomData::fields() if the field data hasn't been loaded yet.
 * @return Number of fields read on success; negative POSIX error code on error.
 */
int ELF::loadFieldData(void)
{
	RP_D(ELF);
	if (!d->fields->empty()) {
		// Field data *has* been loaded...
		return 0;
	} else if (!d->file || !d->file->isOpen()) {
		// File isn't open.
		return -EBADF;
	} else if (!d->isValid) {
		// Unsupported file.
		return -EIO;
	}

	// Primary ELF header.
	const Elf_PrimaryEhdr *const primary = &d->Elf_Header.primary;
	d->fields->reserve(12);	// Maximum of 12 fields. [3 for machine subtype] [TODO verify this]

	d->fields->reserveTabs(2);
	d->fields->setTabName(0, "ELF");

	// NOTE: Executable type is used as File Type.

	// Bitness/Endianness. (consolidated as "format")
	static const char *const exec_type_tbl[] = {
		NOP_C_("RomData|ExecType", "32-bit Little-Endian"),
		NOP_C_("RomData|ExecType", "64-bit Little-Endian"),
		NOP_C_("RomData|ExecType", "32-bit Big-Endian"),
		NOP_C_("RomData|ExecType", "64-bit Big-Endian"),
	};
	const char *const format_title = C_("ELF", "Format");
	if (d->elfFormat > ELFPrivate::Elf_Format::Unknown &&
	    (int)d->elfFormat < ARRAY_SIZE_I(exec_type_tbl))
	{
		d->fields->addField_string(format_title,
			dpgettext_expr(RP_I18N_DOMAIN, "RomData|ExecType", exec_type_tbl[(int)d->elfFormat]));
	}
	else
	{
		// TODO: Show individual values.
		// NOTE: This shouldn't happen...
		d->fields->addField_string(format_title, C_("RomData", "Unknown"));
	}

	// CPU.
	const char *const cpu_title = C_("ELF", "CPU");
	const char *const cpu = ELFData::lookup_cpu(primary->e_machine);
	if (cpu) {
		d->fields->addField_string(cpu_title, cpu);
	} else {
		d->fields->addField_string(cpu_title,
			rp_sprintf(C_("RomData", "Unknown (0x%04X)"), primary->e_machine));
	}

	// CPU flags.
	// TODO: Needs testing.
	// TODO: Add more information from the following binutils ELF headers:
	// - i370.h, frv.h, h8.h, ia64.h, iq2000.h, lm32.h, m68hc11.h, mep.h,
	// - mn10300.h, mt.h, nds32.h, nfp.h, nios2.h, or1k.h, pj.h, ppc.h,
	// - ppc64.h, rl78.h, rx.h, s390.h, score.h, sparc.h, tic6x.h, v850.h,
	// - vax.h, visium.h, xgate.h, xtensa.h
	Elf32_Word e_flags = (primary->e_class == ELFCLASS64
		? d->Elf_Header.elf64.e_flags
		: d->Elf_Header.elf32.e_flags);
	switch (primary->e_machine) {
		case EM_68K: {
			// binutils: include/elf/m68k.h
			if (primary->e_class != ELFCLASS32) {
				// M68K is 32-bit only.
				break;
			}

			// Instruction set.
			// NOTE: `file` can show both 68000 and CPU32
			// at the same time, but that doesn't make sense.
			const char *m68k_insn;
			if (e_flags == 0) {
				m68k_insn = "68020";
			} else if (e_flags & 0x01000000) {
				m68k_insn = "68000";
			} else if ((e_flags & 0x00810000) == 0x00810000) {
				m68k_insn = "CPU32";
			} else if (e_flags & 0x02000000) {
				m68k_insn = "Fido";
			} else {
				m68k_insn = nullptr;
			}

			if (m68k_insn) {
				d->fields->addField_string(C_("ELF", "Instruction Set"), m68k_insn);
			}
			break;
		}

		case EM_SPARC32PLUS:
		case EM_SPARCV9: {
			// Verify bitness.
			if (primary->e_machine == EM_SPARC32PLUS &&
			    primary->e_class != ELFCLASS32)
			{
				// SPARC32PLUS must be 32-bit.
				break;
			}
			else if (primary->e_machine == EM_SPARCV9 &&
				 primary->e_class != ELFCLASS64)
			{
				// SPARCV9 must be 64-bit.
				break;
			}

			// SPARC memory ordering.
			static const char *const sparc_mm[] = {
				NOP_C_("ELF|SPARC_MM", "Total Store Ordering"),
				NOP_C_("ELF|SPARC_MM", "Partial Store Ordering"),
				NOP_C_("ELF|SPARC_MM", "Relaxed Memory Ordering"),
				NOP_C_("ELF|SPARC_MM", "Invalid"),
			};
			d->fields->addField_string(C_("ELF", "Memory Ordering"),
				dpgettext_expr(RP_I18N_DOMAIN, "ELF|SPARC_MM", sparc_mm[e_flags & 3]));

			// SPARC CPU flags. (rshifted by 8)
			static const char *const sparc_flags_names[] = {
				// 0x100-0x800
				"SPARC V8+", "UltraSPARC I", "HaL R1", "UltraSPARC III",
				// 0x1000-0x8000
				nullptr, nullptr, nullptr, nullptr,
				// 0x10000-0x80000
				nullptr, nullptr, nullptr, nullptr,
				// 0x100000-0x800000
				nullptr, nullptr, nullptr,
				// tr: Little-Endian Data
				NOP_C_("ELF|SPARCFlags", "LE Data")
			};
			vector<string> *const v_sparc_flags_names = RomFields::strArrayToVector_i18n(
				"ELF|SPARCFlags", sparc_flags_names, ARRAY_SIZE(sparc_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_sparc_flags_names, 4, (e_flags >> 8));
			break;
		}

		case EM_MIPS:
		case EM_MIPS_RS3_LE: {
			// 32-bit: O32 vs. N32
			if (primary->e_class == ELFCLASS32) {
				d->fields->addField_string(C_("ELF", "MIPS ABI"),
					(e_flags & 0x20) ? "N32" : "O32");
			}

			// MIPS architecture level.
			static const char *const mips_levels[] = {
				"MIPS-I", "MIPS-II", "MIPS-III", "MIPS-IV",
				"MIPS-V", "MIPS32", "MIPS64", "MIPS32 rel2",
				"MIPS64 rel2", "MIPS32 rel6", "MIPS64 rel6"
			};
			const unsigned int level = (e_flags >> 28);
			const char *const cpu_level_title = C_("ELF", "CPU Level");
			if (level < ARRAY_SIZE(mips_levels)) {
				d->fields->addField_string(cpu_level_title, mips_levels[level]);
			} else {
				d->fields->addField_string(cpu_level_title,
					rp_sprintf(C_("RomData", "Unknown (0x%02X)"), level));
			}

			// MIPS CPU flags.
			// NOTE: Shifting ASE flags from bits 24-27 to bits 12-15.
			const unsigned int mips_cpu_flags = (e_flags & 0xFFF) | ((e_flags >> 12) & 0xF000);
			static const char *const mips_flags_names[] = {
				// 0x1-0x8
				NOP_C_("ELF|MIPSFlags", "No Reorder"),
				"PIC", "CPIC", "XGOT",
				// 0x10-0x80
				"UCODE", "ABI2", "ABI ON32",
				NOP_C_("ELF|MIPSFlags", "Options First"),
				// 0x100-0x400
				"32-bit", "FP64", "NaN 2008",
				nullptr,
				// 0x1000-0x8000 (shifted from 0x01000000-0x08000000)
				nullptr, "MicroMIPS", "MIPS-16", "MDMX",
			};
			vector<string> *const v_mips_flags_names = RomFields::strArrayToVector_i18n(
				"ELF|MIPSFlags", mips_flags_names, ARRAY_SIZE(mips_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_mips_flags_names, 4, mips_cpu_flags);
			break;
		}

		case EM_PARISC: {
			// binutils: include/elf/hppa.h

			// PA-RISC version.
			string parisc_version;
			parisc_version.reserve(12);
			switch (e_flags & 0xFFFF) {
				default:
				case 0x020B:
					parisc_version = "1.0";
					break;
				case 0x0210:
					parisc_version = "1.1";
					break;
				case 0x0214:
					parisc_version = "2.0";
					break;
			}
			if (e_flags & 0x0008) {
				parisc_version += " (LP64)";
			}
			d->fields->addField_string(C_("ELF", "PA-RISC Version"), parisc_version);

			// PA-RISC CPU flags.
			static const char *const parisc_flags_names[] = {
				// 0x1-0x8
				NOP_C_("ELF|PARISCFlags", "Trap NULL"),
				"EXT", "LSB",
				NOP_C_("ELF|PARISCFlags", "Wide"),
				// 0x10-0x40
				NOP_C_("ELF|PARISCFlags", "No KABP"),
				nullptr,
				NOP_C_("ELF|PARISCFlags", "Lazy Swap"),
			};
			vector<string> *const v_parisc_flags_names = RomFields::strArrayToVector_i18n(
				"ELF|PARISCFlags", parisc_flags_names, ARRAY_SIZE(parisc_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_parisc_flags_names, 4, ((e_flags >> 16) & 0x7F));
			break;
		}

		case EM_ARM: {
			if (primary->e_class != ELFCLASS32) {
				// 32-bit only.
				break;
			}

			// ARM EABI
			string arm_eabi = rp_sprintf("EABI%u", (e_flags >> 24));
			if (e_flags & 0x00800000) {
				arm_eabi += " BE8";
			}
			if (e_flags & 0x00400000) {
				arm_eabi += " LE8";
			}
			d->fields->addField_string(C_("ELF", "ARM EABI"), arm_eabi);

			// ARM CPU flags.
			// NOTE: Most of these are deprecated. (pre-EABI)
			static const char *const arm_flags_names[] = {
				// 0x1-0x8
				"RelExec", nullptr, "Interwork", "APCS 26",
				// 0x10-0x80
				NOP_C_("ELF|ARMFlags", "APCS Float"),
				"PIC",
				NOP_C_("ELF|ARMFlags", "Align8"),
				NOP_C_("ELF|ARMFlags", "New ABI"),
				// 0x100-0x800
				NOP_C_("ELF|ARMFlags", "Old ABI"),
				NOP_C_("ELF|ARMFlags", "Soft Float"),
				NOP_C_("ELF|ARMFlags", "VFP Float"),
				NOP_C_("ELF|ARMFlags", "Maverick Float"),
			};
			vector<string> *const v_arm_flags_names = RomFields::strArrayToVector_i18n(
				"ELF|ARMFlags", arm_flags_names, ARRAY_SIZE(arm_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_arm_flags_names, 4, (e_flags & 0xFFF));
			break;
		}

		case EM_OLD_ALPHA:
		case EM_ALPHA: {
			// binutils: include/elf/alpha.h

			// Alpha CPU flags.
			static const char *const alpha_flags_names[] = {
				// 0x1-0x2
				NOP_C_("ELF|AlphaFlags", "Addresses <= 2GB"),
				NOP_C_("ELF|AlphaFlags", "Relaxed Code Movement"),
			};
			vector<string> *const v_alpha_flags_names = RomFields::strArrayToVector_i18n(
				"ELF|AlphaFlags", alpha_flags_names, ARRAY_SIZE(alpha_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_alpha_flags_names, 2, (e_flags & 0x03));
			break;
		}

		case EM_SH: {
			// binutils: include/elf/sh.h

			// CPU subtypes.
			static const char *const superh_cpu_subtype_tbl[] = {
				// 0-15
				nullptr, "SH-1", "SH-2", "SH-3",
				"SH-DSP", "SH3-DSP", "SH4AL-DSP", nullptr,
				"SH-3E", "SH-4", "SH-5", "SH-2E",
				"SH-4A", "SH-2A", nullptr, nullptr,

				// 16-24
				"SH-4 (No FPU)", "SH-4A (No FPU)", "SH-4 (No MMU or FPU)", "SH-2A (No FPU)",
				"SH-3 (No MMU)", "SH-2A/SH-4 (No FPU)", "SH-2A/SH-3 (No FPU)", "SH-2A/SH-4",
				"SH-2A/SH-3E",
			};
			const char *s_cpu_subtype = nullptr;
			const uint8_t cpu_subtype = (e_flags & 0x1F);
			if (cpu_subtype < ARRAY_SIZE(superh_cpu_subtype_tbl)) {
				s_cpu_subtype = superh_cpu_subtype_tbl[cpu_subtype];
			}
			if (s_cpu_subtype) {
				d->fields->addField_string(C_("ELF", "CPU Subtype"), s_cpu_subtype);
			}

			// SuperH CPU flags. (rshifted by 8)
			static const char *const superh_flags_names[] = {
				// 0x100-0x800
				"PIC", nullptr, nullptr, nullptr,

				// 0x1000-0x8000
				nullptr, nullptr, nullptr, "FDPIC",
			};
			vector<string> *const v_superh_flags_names = RomFields::strArrayToVector(
				superh_flags_names, ARRAY_SIZE(superh_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_superh_flags_names, 2, (e_flags >> 8));
			break;
		}

		case EM_ARC: {
			// binutils: include/elf/arc.h
			// TODO: Other ARC variants?

			// CPU subtypes.
			static const char *const arc_cpu_subtypes[] = {
				nullptr, nullptr, "ARC600", "ARC700",
				"ARC601", "ARCv2EM", "ARCv2HS",
			};
			const uint8_t cpu_subtype = (e_flags & 0xFF);
			const char *s_cpu_subtype = nullptr;
			if (cpu_subtype < ARRAY_SIZE(arc_cpu_subtypes)) {
				s_cpu_subtype = arc_cpu_subtypes[cpu_subtype];
			}
			if (s_cpu_subtype) {
				d->fields->addField_string(C_("ELF", "CPU Subtype"), s_cpu_subtype);
			}

			// ARC Linux specific ABIs.
			const uint8_t arc_linux_osabi = (e_flags >> 8) & 0x0F;
			if (arc_linux_osabi != 1 && arc_linux_osabi <= 4) {
				d->fields->addField_string(C_("ELF", "Linux OSABI"),
					rp_sprintf("ARC Linux OSABI v%u", arc_linux_osabi));
			}

			// ARC CPU flags. (rshifted by 8)
			static const char *const arc_flags_names[] = {
				// 0x100
				"PIC",
			};
			vector<string> *const v_arc_flags_names = RomFields::strArrayToVector(
				arc_flags_names, ARRAY_SIZE(arc_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_arc_flags_names, 1, ((e_flags >> 8) & 1));
			break;
		}

		case EM_COLDFIRE: {
			// binutils: include/elf/m68k.h
			if (primary->e_class != ELFCLASS32) {
				// M68K is 32-bit only.
				break;
			}

			// Instruction set.
			// Top 24 bits should be either 0 or 0x00008000.
			const uint32_t top24 = (e_flags & 0xFFFFFF00);
			if (top24 != 0 && top24 != 0x00008000) {
				// Incorrect flags.
				break;
			}

			// ISA
			static const char *const cf_isa_tbl[] = {
				nullptr,
				"ISA A (no div)", "ISA A", "ISA A+",
				"ISA B (no USP)", "ISA B",
				"ISA C", "ISA C (no div)",
			};
			const char *cf_isa = nullptr;
			const uint8_t cf_isa_flags = (e_flags & 0x0F);
			if (cf_isa_flags < ARRAY_SIZE(cf_isa_tbl)) {
				cf_isa = cf_isa_tbl[cf_isa_flags];
			}

			// MAC
			static const char *const cf_mac_tbl[] = {
				nullptr, "MAC", "EMAC", "EMAC_B"
			};
			const char *cf_mac = nullptr;
			const uint8_t cf_mac_flags = ((e_flags >> 4) & 0x03);
			if (cf_mac_flags < ARRAY_SIZE(cf_mac_tbl)) {
				cf_mac = cf_mac_tbl[cf_mac_flags];
			}

			string s_cf_isa;
			if (cf_isa) {
				s_cf_isa.assign(cf_isa);
			}
			if (cf_mac) {
				if (s_cf_isa.empty()) {
					s_cf_isa.assign(cf_mac);
				} else {
					s_cf_isa += " (";
					s_cf_isa += cf_mac;
					s_cf_isa += ')';
				}
			}

			if (!s_cf_isa.empty()) {
				d->fields->addField_string(C_("ELF", "ColdFire ISA"), s_cf_isa);
			}
			break;
		}

		case EM_AVR:
		case EM_AVR_OLD: {
			// NOTE: Using a heuristic instead of a lookup table.
			char s_avr_subtype[16];
			const uint8_t avr_subtype = (e_flags & 0xFF);
			if (avr_subtype < 10) {
				snprintf(s_avr_subtype, sizeof(s_avr_subtype), "AVR%u", avr_subtype);
			} else if (avr_subtype < 100) {
				snprintf(s_avr_subtype, sizeof(s_avr_subtype), "AVR%u.%u",
					avr_subtype / 10, avr_subtype % 10);
			} else if (avr_subtype == 100) {
				strcpy(s_avr_subtype, "AVR Tiny");
			} else if (avr_subtype < 110) {
				snprintf(s_avr_subtype, sizeof(s_avr_subtype), "XMEGA%u",
					(avr_subtype % 100));
			} else {
				s_avr_subtype[0] = '\0';
			}

			if (s_avr_subtype[0] != '\0') {
				d->fields->addField_string(C_("ELF", "CPU Subtype"), s_avr_subtype);
			}
			break;
		}

		case EM_M32R:
		case EM_CYGNUS_M32R: {
			// binutils: include/elf/m32r.h
			static const char *const m32r_insn_set_tbl[] = {
				"m32r", "m32rx", "m32r2", nullptr
			};
			const char *const m32r_insn_set = m32r_insn_set_tbl[(e_flags >> 28) & 0x03];
			if (m32r_insn_set) {
				d->fields->addField_string(C_("ELF", "Instruction Set"), m32r_insn_set);
			}

			// M32R new instructions field.
			static const char *const m32r_flags_names[] = {
				// 0x1-0x8
				"Parallel", "m32rx", "Bit Insns", "FP Insns",
			};
			vector<string> *const v_m32r_flags_names = RomFields::strArrayToVector_i18n(
				"ELF|M32RFlags", m32r_flags_names, ARRAY_SIZE(m32r_flags_names));
			d->fields->addField_bitfield(C_("ELF", "M32R New Insns"),
				v_m32r_flags_names, 4, ((e_flags >> 16) & 0x0FFF));

			// TODO: 4-bit m32r ignore to check field? (0x0000000F)
			break;
		}

		case EM_MSP430: {
			// binutils: include/elf/msp430.h
			// NOTE: Using a heuristic instead of a lookup table.
			char s_msp430_subtype[16];
			const uint8_t msp430_subtype = (e_flags & 0xFF);
			if (msp430_subtype == 45) {
				strcpy(s_msp430_subtype, "MSP430X");
			} else if (msp430_subtype == 110) {
				strcpy(s_msp430_subtype, "MSP430x11x1");
			} else if (msp430_subtype <= 54) {
				snprintf(s_msp430_subtype, sizeof(s_msp430_subtype), "MSP430x%u", msp430_subtype);
			} else {
				s_msp430_subtype[0] = '\0';
			}

			if (s_msp430_subtype[0] != '\0') {
				d->fields->addField_string(C_("ELF", "CPU Subtype"), s_msp430_subtype);
			}
			break;
		}

		case EM_BLACKFIN: {
			// binutils: include/elf/bfin.h

			// Blackfin CPU flags.
			static const char *const blackfin_flags_names[] = {
				// 0x1-0x8
				"PIC", "FDPIC", nullptr, nullptr,
				// 0x10-0x20
				NOP_C_("ELF|BlackfinFlags", "Code in L1"),
				NOP_C_("ELF|BlackfinFlags", "Data in L1"),
			};
			vector<string> *const v_blackfin_flags_names = RomFields::strArrayToVector_i18n(
				"ELF|BlackfinFlags", blackfin_flags_names, ARRAY_SIZE(blackfin_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_blackfin_flags_names, 2, (e_flags & 0x33));
			break;
		}

		case EM_M16C:
		case EM_M32C:
		case EM_M32C_OLD: {
			// binutils: include/elf/m32c.h
			// NOTE: This might only be valid for EM_M32C_OLD.
			const char *m32c_subtype;
			switch (e_flags & 0x7F) {
				case 0x75:
					m32c_subtype = "M16C";
					break;
				case 0x78:
					m32c_subtype = "M32C";
					break;
				default:
					m32c_subtype = nullptr;
					break;
			}
			if (m32c_subtype) {
				d->fields->addField_string(C_("ELF", "CPU Subtype"), m32c_subtype);
			}
			break;
		}

		case EM_Z80: {
			// binutils: include/elf/z80.h
			static const char *const z80_insn_set_tbl[] = {
				nullptr, "Z80", "Z180", "R800",
				"eZ80 (Z80 mode)", "Sharp LR35902",
			};

			const char *z80_insn_set = nullptr;
			e_flags &= 0xFF;
			if (e_flags < ARRAY_SIZE(z80_insn_set_tbl)) {
				z80_insn_set = z80_insn_set_tbl[e_flags];
			} else if (e_flags == 0x84) {
				z80_insn_set = "eZ80 (ADL mode)";
			}
			if (z80_insn_set) {
				d->fields->addField_string(C_("ELF", "Instruction Set"), z80_insn_set);
			}
			break;
		}

		case EM_RISCV: {
			// binutils: include/elf/riscv.h

			// Floating-point ABI in use.
			static const char *const riscv_fpabi_tbl[] = {
				NOP_C_("ELF|RISCVFPABI", "Soft-Float"),
				NOP_C_("ELF|RISCVFPABI", "Single-Float"),
				NOP_C_("ELF|RISCVFPABI", "Double-Float"),
				NOP_C_("ELF|RISCVFPABI", "Quad-Float"),
			};
			d->fields->addField_string(C_("ELF", "Floating-Point ABI"),
				dpgettext_expr(RP_I18N_DOMAIN, "ELF|RISCVFPABI", riscv_fpabi_tbl[((e_flags & 0x0006) >> 1)]));

			// RISC-V CPU flags.
			static const char *const riscv_flags_names[] = {
				// 0x1-0x8
				"RVC", nullptr, nullptr, "RV32E",
			};
			vector<string> *const v_riscv_flags_names = RomFields::strArrayToVector(
				riscv_flags_names, ARRAY_SIZE(riscv_flags_names));
			d->fields->addField_bitfield(C_("ELF", "CPU Flags"),
				v_riscv_flags_names, 2, (e_flags & 0x0F));
			break;
		}

		default:
			// No flags.
			break;
	}

	// OS ABI.
	const char *const osabi_title = C_("ELF", "OS ABI");
	const char *const osabi = ELFData::lookup_osabi(primary->e_osabi);
	if (osabi) {
		d->fields->addField_string(osabi_title, osabi);
	} else {
		d->fields->addField_string(osabi_title,
			rp_sprintf(C_("RomData", "Unknown (%u)"), primary->e_osabi));
	}

	// ABI version.
	if (!d->isWiiU) {
		d->fields->addField_string_numeric(C_("ELF", "ABI Version"),
			primary->e_osabiversion);
	}

	// Linkage. (Executables only)
	if (d->fileType == FileType::Executable) {
		d->fields->addField_string(C_("ELF", "Linkage"),
			d->pt_dynamic.addr != 0
				? C_("ELF|Linkage", "Dynamic")
				: C_("ELF|Linkage", "Static"));
	}

	// Interpreter.
	if (!d->interpreter.empty()) {
		d->fields->addField_string(C_("ELF", "Interpreter"), d->interpreter);
	}

	// Operating system.
	if (!d->osVersion.empty()) {
		d->fields->addField_string(C_("ELF", "OS Version"), d->osVersion);
	}

	// Entry point.
	// Also indicates PIE.
	// NOTE: Formatting using 8 digits, since 64-bit executables
	// usually have entry points within the first 4 GB.
	if (d->fileType == FileType::Executable) {
		string entry_point;
		if (primary->e_class == ELFCLASS64) {
			entry_point = rp_sprintf("0x%08" PRIX64, d->Elf_Header.elf64.e_entry);
		} else {
			entry_point = rp_sprintf("0x%08X", d->Elf_Header.elf32.e_entry);
		}
		if (d->isPie) {
			// tr: Entry point, then "Position-Independent".
			entry_point = rp_sprintf(C_("ELF", "%s (Position-Independent)"),
				entry_point.c_str());
		}
		d->fields->addField_string(C_("ELF", "Entry Point"), entry_point);
	}

	// Build ID.
	if (!d->build_id.empty()) {
		// TODO: Put the build ID type in the field itself.
		// Using field name for now.
		const string fieldName = rp_sprintf("BuildID[%s]", (d->build_id_type ? d->build_id_type : "unknown"));
		d->fields->addField_string_hexdump(fieldName.c_str(),
			d->build_id.data(), d->build_id.size(),
			RomFields::STRF_HEX_LOWER | RomFields::STRF_HEXDUMP_NO_SPACES);
	}

	// If this is a dynamically-linked executable,
	// print DT_FLAGS and DT_FLAGS_1.
	// TODO: Print required libraries?
	// Sanity check: Maximum of 1 MB.
	if (!d->isWiiU && d->pt_dynamic.addr != 0) {
		d->addPtDynamicFields();
	}

	// Finished reading the field data.
	return static_cast<int>(d->fields->count());
}

}
