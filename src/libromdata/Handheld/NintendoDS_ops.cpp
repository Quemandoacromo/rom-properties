/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * NintendoDS_ops.cpp: Nintendo DS(i) ROM reader. (ROM operations)         *
 *                                                                         *
 * Copyright (c) 2016-2024 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "config.librpbase.h"

#include "NintendoDS.hpp"
#include "NintendoDS_p.hpp"
#include "ndscrypt.hpp"

// Other rom-properties libraries
using namespace LibRpBase;
using namespace LibRpText;

// C++ STL classes
using std::array;
using std::ostringstream;
using std::string;
using std::unique_ptr;
using std::vector;

// DSi Secure Area handling is disabled by default.
// Most DSi emulators expect the Secure Area to be encrypted, so there's
// no point in decrypting it, and I'm not sure if the encryption function
// is correct.
//#define ENABLE_DSi_SECURE_AREA 1

namespace LibRomData {

/** NintendoDSPrivate **/

/**
 * Check the NDS security data.
 *
 * $1000-$3FFF is normally unreadable on hardware, so this
 * area is usually blank in dumped ROMs. However, this area
 * normally has precomputed Blowfish tables and other data,
 * which are used as part of the NDS security system.
 * DSiWare and Wii U VC SRLs, as well as SRLs generated by
 * the DS SDK, will have actual data here.
 *
 * @return NDS security data flags.
 */
uint32_t NintendoDSPrivate::checkNDSSecurityData(void)
{
	// TODO: Verify the entire area. (Not sure if we can calculate the
	// Random data correctly...)
	uint32_t ret = 0;
	if (!file || !file->isOpen()) {
		// File was closed...
		return ret;
	}

	array<uint32_t, 0x3000/4> security_data;
	size_t size = file->seekAndRead(0x1000, security_data.data(), security_data.size());
	if (size != security_data.size()) {
		// Seek and/or read error.
		return 0;
	}

	// Check the S-Box and Blowfish tables for non-zero data.
	// S-Box: 0x1600
	// Blowfish: 0x1C00
	if (security_data[0x0600/4] != 0 && security_data[0x0C00/4] != 0) {
		ret |= NDS_SECDATA_BLOWFISH;
	}

	// Check the static area.
	if (security_data[0x2000/4] == be32_to_cpu(0xFF00FF00) &&
	    security_data[0x2004/4] == be32_to_cpu(0xAA55AA55) &&
	    security_data[0x2008/4] == be32_to_cpu(0x08090A0B) &&
	    security_data[0x200C/4] == be32_to_cpu(0x0C0D0E0F) &&
	    security_data[0x2200/4] == be32_to_cpu(0xFFFEFDFC) &&
	    security_data[0x2204/4] == be32_to_cpu(0xFBFAF9F8) &&
	    security_data[0x2400/4] == be32_to_cpu(0x00000000) &&
	    security_data[0x2600/4] == be32_to_cpu(0xFFFFFFFF) &&
	    security_data[0x2800/4] == be32_to_cpu(0x0F0F0F0F) &&
	    security_data[0x2A00/4] == be32_to_cpu(0xF0F0F0F0) &&
	    security_data[0x2C00/4] == be32_to_cpu(0x55555555) &&
	    security_data[0x2E00/4] == be32_to_cpu(0xAAAAAAAA))
	{
		// Static area appears to be valid.
		ret |= NDS_SECDATA_STATIC;
	}

	// Check for random values at other areas.
	if (security_data[0x0000/4] != 0 ||
	    security_data[0x0700/4] != 0 ||
	    security_data[0x1C00/4] != 0)
	{
		// Non-zero value in one of these areas.
		// Random data is present.
		ret |= NDS_SECDATA_RANDOM;
	}

	return ret;
}

/**
 * Check the NDS Secure Area type.
 * This reads from the ROM, so the ROM must be open.
 * @return Secure area type.
 */
NintendoDSPrivate::NDS_SecureArea NintendoDSPrivate::checkNDSSecureArea(void)
{
	if (!file || !file->isOpen()) {
		// File was closed...
		return NDS_SECAREA_UNKNOWN;
	}

	// Read the start of the Secure Area.
	// NOTE: We only need to check the first two DWORDs, but
	// we're reading the first four because CIAReader only
	// supports multiples of 16 bytes right now.
	array<uint32_t, 4> secure_area;
	size_t size = file->seekAndRead(0x4000, secure_area.data(), secure_area.size());
	if (size != secure_area.size()) {
		// Seek and/or read error.
		return NDS_SECAREA_UNKNOWN;
	}

	// Reference: https://github.com/devkitPro/ndstool/blob/master/source/header.cpp#L39

	NDS_SecureArea ret;
	//bool needs_encryption = false;	// TODO
	if (le32_to_cpu(romHeader.arm9.rom_offset) < 0x4000) {
		// ARM9 secure area is not present.
		// This is only valid for homebrew.
		ret = NDS_SECAREA_HOMEBREW;
	} else if (secure_area[0] == cpu_to_le32(0x00000000) && secure_area[1] == cpu_to_le32(0x00000000)) {
		// Secure area is empty. (DS Download Play)
		ret = NDS_SECAREA_MULTIBOOT;
	} else if (secure_area[0] == cpu_to_le32(0xE7FFDEFF) && secure_area[1] == cpu_to_le32(0xE7FFDEFF)) {
		// Secure area is decrypted.
		// Probably dumped using wooddumper or Decrypt9WIP.
		ret = NDS_SECAREA_DECRYPTED;
		//needs_encryption = true;	// CRC requires encryption.
	} else {
		// Secure area is encrypted.
		ret = NDS_SECAREA_ENCRYPTED;
	}

	// TODO: Verify the CRC?
	// For decrypted ROMs, this requires re-encrypting the secure area.
	return ret;
}

/**
 * Get the localized string identifying the NDS Secure Area type.
 * This uses the cached secArea value.
 * @return NDS Secure Area type string.
 */
const char *NintendoDSPrivate::getNDSSecureAreaString(void)
{
	static const array<const char*, 5> nds_secure_area_type = {{
		nullptr,
		NOP_C_("NintendoDS|SecureArea", "Homebrew"),
		NOP_C_("NintendoDS|SecureArea", "Multiboot"),
		NOP_C_("NintendoDS|SecureArea", "Decrypted"),
		NOP_C_("NintendoDS|SecureArea", "Encrypted"),
	}};

	if (secArea >= NintendoDSPrivate::NDS_SECAREA_HOMEBREW &&
	    secArea <= NintendoDSPrivate::NDS_SECAREA_ENCRYPTED)
	{
		return pgettext_expr("NintendoDS|SecureArea", nds_secure_area_type[secArea]);
	}

	return C_("RomData", "Unknown");
}

/** NintendoDS **/

/**
 * Get the list of operations that can be performed on this ROM.
 * Internal function; called by RomData::romOps().
 * @return List of operations.
 */
vector<RomData::RomOp> NintendoDS::romOps_int(void) const
{
	// Determine if the ROM is trimmed and/or encrypted.
	// TODO: Cache the vector?
	vector<RomOp> ops;
#ifdef ENABLE_DECRYPTION
	ops.resize(2);
#else /* !ENABLE_DECRYPTION */
	ops.resize(1);
#endif /* ENABLE_DECRYPTION */

	RP_D(const NintendoDS);
	uint32_t flags;

	// Trim/Untrim ROM
	bool showUntrim = false;
	if (likely(d->romSize > 0)) {
		// Determine if the ROM is trimmed.
		const uint32_t total_used_rom_size = d->totalUsedRomSize();
		if (total_used_rom_size < 1024) {
			// Invalid ROM size in header.
			// Cannot trim/untrim, so show the "Trim ROM" option but disabled.
			showUntrim = !isPow2(d->romSize);
			flags = RomOp::ROF_REQ_WRITABLE;
		} else if (total_used_rom_size == d->romSize && isPow2(d->romSize)) {
			// ROM is technically trimmed, but it's already a power of two.
			// Cannot trim/untrim, so show the "Trim ROM" option but disabled.
			showUntrim = false;
			flags = RomOp::ROF_REQ_WRITABLE;
		} else {
			showUntrim = !(total_used_rom_size < d->romSize);
			flags = RomOp::ROF_ENABLED | RomOp::ROF_REQ_WRITABLE;
		}
	} else {
		// Empty ROM?
		flags = 0;
	}
	ops[0].desc = showUntrim
		? C_("NintendoDS|RomOps", "&Untrim ROM")
		: C_("NintendoDS|RomOps", "&Trim ROM");
	ops[0].flags = flags;

#ifdef ENABLE_DECRYPTION
	// Encrypt/Decrypt ROM
	bool showEncrypt = false;
	switch (d->secArea) {
		case NintendoDSPrivate::NDS_SECAREA_DECRYPTED:
			showEncrypt = true;
			flags = RomOp::ROF_ENABLED | RomOp::ROF_REQ_WRITABLE;
			break;
		case NintendoDSPrivate::NDS_SECAREA_ENCRYPTED:
			showEncrypt = false;
			flags = RomOp::ROF_ENABLED | RomOp::ROF_REQ_WRITABLE;
			break;
		default:
			showEncrypt = false;
			flags = RomOp::ROF_REQ_WRITABLE;
			break;
	}
	ops[1].desc = showEncrypt
		? C_("NintendoDS|RomOps", "Encrypt ROM")
		: C_("NintendoDS|RomOps", "Decrypt ROM");
	ops[1].flags = flags;
#endif /* ENABLE_DECRYPTION */

	return ops;
}

/**
 * Perform a ROM operation.
 * Internal function; called by RomData::doRomOp().
 * @param id		[in] Operation index.
 * @param pParams	[in/out] Parameters and results. (for e.g. UI updates)
 * @return 0 on success; negative POSIX error code on error.
 */
int NintendoDS::doRomOp_int(int id, RomOpParams *pParams)
{
	RP_D(NintendoDS);
	int ret = 0;

	switch (id) {
		case 0: {
			// Trim/untrim ROM.
			// Trim = reduce ROM to minimum size as indicated by header.
			// Untrim = expand to power of 2 size, filled with 0xFF.
			const uint32_t total_used_rom_size = d->totalUsedRomSize();
			if (total_used_rom_size < 1024) {
				// Invalid ROM size in header.
				pParams->status = -EIO;
				pParams->msg = C_("NintendoDS", "ROM header has an invalid \"Used ROM Size\" field.");
				return -EIO;
			}

			bool doTrim;
			if (!(total_used_rom_size < d->romSize)) {
				// ROM is trimmed. Untrim it.
				doTrim = false;
				const off64_t next_pow2 = (1LL << (uilog2(total_used_rom_size) + 1));
				assert(next_pow2 > total_used_rom_size);
				if (next_pow2 <= total_used_rom_size) {
					// Something screwed up here...
					pParams->status = -EIO;
					pParams->msg = C_("NintendoDS", "ROM size would not change if untrimmed.");
					return -EIO;
				}

#define UNTRIM_BLOCK_SIZE (64*1024)
				unique_ptr<uint8_t[]> ff_block(new uint8_t[UNTRIM_BLOCK_SIZE]);
				memset(ff_block.get(), 0xFF, UNTRIM_BLOCK_SIZE);

				off64_t pos = static_cast<off64_t>(total_used_rom_size);
				int ret = d->file->seek(pos);
				if (ret != 0) {
					// Seek error.
					ret = -d->file->lastError();
					if (ret == 0) {
						ret = -EIO;
					}
					pParams->status = ret;
					pParams->msg = C_("NintendoDS", "Seek error when attempting to untrim ROM.");
					return ret;
				}

				// If we're not aligned to the untrim block size,
				// write a partial block.
				const unsigned int partial = static_cast<unsigned int>(pos % UNTRIM_BLOCK_SIZE);
				if (partial != 0) {
					const unsigned int toWrite = UNTRIM_BLOCK_SIZE - partial;
					size_t size = d->file->write(ff_block.get(), toWrite);
					if (size != toWrite) {
						// Write error.
						ret = -d->file->lastError();
						if (ret == 0) {
							ret = -EIO;
						}
						pParams->status = ret;
						pParams->msg = C_("NintendoDS", "Write error when attempting to untrim ROM.");
						return ret;
					}
					pos += toWrite;
				}

				// Write remaining full blocks.
				for (; pos < next_pow2; pos += UNTRIM_BLOCK_SIZE) {
					size_t size = d->file->write(ff_block.get(), UNTRIM_BLOCK_SIZE);
					if (size != UNTRIM_BLOCK_SIZE) {
						// Write error.
						ret = -d->file->lastError();
						if (ret == 0) {
							ret = -EIO;
						}
						pParams->status = ret;
						pParams->msg = C_("NintendoDS", "Write error when attempting to untrim ROM.");
						return ret;
					}
				}

				// ROM untrimmed.
				d->file->flush();
				d->romSize = d->file->size();
			} else if (total_used_rom_size < d->romSize) {
				// ROM is untrimmed. Trim it.
				doTrim = true;
				int ret = d->file->truncate(total_used_rom_size);
				if (ret != 0) {
					// Truncate failed.
					ret = -d->file->lastError();
					if (ret == 0) {
						ret = -EIO;
					}
					pParams->status = ret;
					pParams->msg = C_("NintendoDS", "Truncate failed when attempting to trim ROM.");
					return ret;
				}
				d->file->flush();
				d->romSize = d->file->size();
			} else {
				// ROM can't be trimmed or untrimmed...
				assert(!"ROM can't be trimmed or untrimmed; menu option should have been disabled.");
				ret = -EIO;
				pParams->status = EIO;
				pParams->msg = C_("NintendoDS", "ROM can't be trimmed or untrimmed.");
				return ret;
			}

			pParams->status = 0;
			pParams->msg = doTrim
				? C_("NintendoDS", "ROM image trimmed successfully.")
				: C_("NintendoDS", "ROM image untrimmed successfully.");
			break;
		}

#ifdef ENABLE_DECRYPTION
		case 1: {
			// Encrypt/Decrypt ROM.
			bool doEncrypt;
			if (d->secArea == NintendoDSPrivate::NDS_SECAREA_DECRYPTED) {
				// Encrypt the secure area.
				doEncrypt = true;
			} else if (d->secArea == NintendoDSPrivate::NDS_SECAREA_ENCRYPTED) {
				// Decrypt the secure area.
				doEncrypt = false;
			} else {
				// Cannot perform this ROM operation.
				ret = -ENOTSUP;
				pParams->status = ret;
				pParams->msg = C_("NintendoDS", "Secure area cannot be adjusted in this ROM.");
				break;
			}

#ifdef ENABLE_DSi_SECURE_AREA
			// If this is a DSi-enhanced game, we have two Secure Areas.
			// FIXME: DSi Secure Area encryption isn't working...
			bool dsi = false;
			const uint32_t arm9i_rom_offset = le32_to_cpu(d->romHeader.dsi.arm9i.rom_offset);
			if (d->romHeader.unitcode & 2) {
				// Verify the ARM9i secure area settings.
				// Reference: https://github.com/d0k3/GodMode9/blob/d010f2858bc2c852f81cca3ac2bc3537c96ecd1a/arm9/source/gamecart/gamecart.c#L120
				// TODO: Check the modcrypt area size in the header. Assuming 0x4000.
				if (arm9i_rom_offset >= le32_to_cpu(d->romHeader.total_used_rom_size) &&
				    (arm9i_rom_offset + 0x4000) <= le32_to_cpu(d->romHeader.dsi.total_used_rom_size))
				{
					// ARM9i offsets are in range.
					// Verify that the ARM9i offset is 0xXXX3000.
					// If it isn't, we might have a special case...
					if ((arm9i_rom_offset & 0xFFFF) == 0x3000) {
						// DSi secure area is present.
						dsi = true;
					}
				}
			}
#endif /* ENABLE_DSi_SECURE_AREA */

			// Make sure nds-blowfish.bin is loaded.
			const char *filename = "nds-blowfish.bin";
			ret = ndscrypt_load_blowfish_bin(NDSCRYPT_BF_NDS);
#ifdef ENABLE_DSi_SECURE_AREA
			if (ret == 0 && dsi) {
				// We also need dsi-blowfish.bin for the DSi secure area.
				// TODO: Identify DSi development cartridges.
				filename = "dsi-blowfish.bin";
				ret = ndscrypt_load_blowfish_bin(NDSCRYPT_BF_DSi);
			}
#endif /* ENABLE_DSi_SECURE_AREA */
			if (ret < 0) {
				pParams->status = ret;
				pParams->msg = rp_sprintf_p(C_("RomData", "Could not open '%1$s': %2$s"),
					filename, strerror(-ret));
				break;
			} else if (ret > 0) {
				pParams->status = -EIO;
				switch (ret) {
					case 1: {
						// TODO: Show actual file size?
						ostringstream oss_exp;
						oss_exp << NDS_BLOWFISH_SIZE;
						pParams->msg = rp_sprintf_p(C_("NintendoDS", "File '%1$s' has the wrong size. (should be %2$s bytes)"),
							filename, oss_exp.str().c_str());
						break;
					}
					case 2:
						// Wrong hash.
						pParams->msg = rp_sprintf(C_("NintendoDS", "File '%s' has the wrong MD5 hash."), filename);
						break;
					default:
						assert(!"Unhandled NDS Blowfish error code.");
						pParams->msg.clear();
						break;
				}

				// Silently suppress positive error codes.
				ret = -EIO;
				break;
			}

			// NDS secure area: Load the ROM header, static area, and NDS Secure Area.
			static constexpr size_t NDS_SEC_AREA_SIZE = 0x1000U * 8U;
			typedef array<uint8_t, NDS_SEC_AREA_SIZE> nds_sec_area_t;

			unique_ptr<nds_sec_area_t> ndsbuf(new nds_sec_area_t);
			d->file->rewind();
			size_t size = d->file->read(ndsbuf->data(), ndsbuf->size());
			if (size != ndsbuf->size()) {
				// Seek and/or read error.
				ret = -d->file->lastError();
				if (ret == 0) {
					ret = -EIO;
				}
				pParams->status = ret;
				pParams->msg = C_("NintendoDS", "Could not read the NDS Secure Area.");
				break;
			}

			// Run the actual encryption/decryption.
			ret = doEncrypt
				? ndscrypt_encrypt_secure_area(ndsbuf->data(), ndsbuf->size(), NDSCRYPT_BF_NDS)
				: ndscrypt_decrypt_secure_area(ndsbuf->data(), ndsbuf->size(), NDSCRYPT_BF_NDS);
			if (ret != 0) {
				// Error encrypting/decrypting.
				pParams->status = -EIO;
				pParams->msg = doEncrypt
					? C_("NintendoDS", "Encrypting the NDS Secure Area failed.")
					: C_("NintendoDS", "Decrypting the NDS Secure Area failed.");
				break;
			}

#ifdef ENABLE_DSi_SECURE_AREA
			if (dsi) {
				// DSi secure area: Load the static area and DSi Secure Area.
				// ROM header is kept from the NDS section.
				static constexpr size_t DSi_SEC_AREA_SIZE = 0x1000 * 7;

				unique_ptr<nds_sec_area_t> dsibuf(new nds_sec_area_t);
				memcpy(dsibuf->data(), ndsbuf->data(), 0x1000);
				const uint32_t dsisec_offset = arm9i_rom_offset - 0x3000;
				size_t size = d->file->seekAndRead(dsisec_offset, &(dsibuf->data())[0x1000], DSi_SEC_AREA_SIZE);
				if (size != DSi_SEC_AREA_SIZE) {
					// Seek and/or read error.
					ret = -d->file->lastError();
					if (ret == 0) {
						ret = -EIO;
					}
					pParams->status = ret;
					pParams->msg = C_("NintendoDS", "Could not read the DSi Secure Area.");
					break;
				}

				// Run the actual encryption/decryption.
				// TODO: DSi development cartridge detection.
				ret = doEncrypt
					? ndscrypt_encrypt_secure_area(dsibuf->data(), dsibuf->size(), NDSCRYPT_BF_DSi)
					: ndscrypt_decrypt_secure_area(dsibuf->data(), dsibuf->size(), NDSCRYPT_BF_DSi);
				if (ret != 0) {
					// Error encrypting/decrypting.
					pParams->status = -EIO;
					pParams->msg = doEncrypt
						? C_("NintendoDS", "Encrypting the DSi Secure Area failed.")
						: C_("NintendoDS", "Decrypting the DSi Secure Area failed.");
					break;
				}

				// Write the DSi Secure Area back to the ROM.
				size = d->file->seekAndWrite(dsisec_offset, &(dsibuf->data())[0x1000], DSi_SEC_AREA_SIZE);
				if (size != DSi_SEC_AREA_SIZE) {
					// Seek and/or write error.
					ret = -d->file->lastError();
					if (ret == 0) {
						ret = -EIO;
					}
					pParams->status = ret;
					pParams->msg = C_("NintendoDS", "Could not write the updated DSi Secure Area.");
					break;
				}
			}
#endif /* ENABLE_DSi_SECURE_AREA */

			// Write the NDS Secure Area back to the ROM.
			d->file->rewind();
			size = d->file->write(ndsbuf->data(), ndsbuf->size());
			if (size != NDS_SEC_AREA_SIZE) {
				// Seek and/or write error.
				ret = -d->file->lastError();
				if (ret == 0) {
					ret = -EIO;
				}
				pParams->status = ret;
				pParams->msg = C_("NintendoDS", "Could not write the updated NDS Secure Area.");
				break;
			}

			// Make sure the updated Secure Areas are flushed to disk.
			d->file->flush();

			// Update the secData and secArea values.
			d->secData = d->checkNDSSecurityData();
			d->secArea = d->checkNDSSecureArea();

			// Update the fields.
			// TODO: Better way to update fields.
			if (!d->fields.empty()) {
				RomFields::Field *field;
				if (d->fieldIdx_secData >= 0) {
					field = const_cast<RomFields::Field*>(d->fields.at(d->fieldIdx_secData));
					field->data.bitfield = d->secData;
				}
				if (d->fieldIdx_secArea >= 0) {
					field = const_cast<RomFields::Field*>(d->fields.at(d->fieldIdx_secArea));
					char *const old_str = const_cast<char*>(field->data.str);
					field->data.str = strdup(d->getNDSSecureAreaString());
					free(old_str);
				}
			}

			// Update fields.
#ifndef ENABLE_DSi_SECURE_AREA
			static constexpr bool dsi = false;
#endif /* ENABLE_DSi_SECURE_AREA */
			pParams->status = 0;
			pParams->msg = doEncrypt
				? NC_("NintendoDS", "Secure Area encrypted successfully.",
					"Secure Areas encrypted successfully.", (dsi ? 2 : 1))
				: NC_("NintendoDS", "Secure Area decrypted successfully.",
					"Secure Areas decrypted successfully.", (dsi ? 2 : 1));
			pParams->fieldIdx.reserve(2);
			if (d->fieldIdx_secData >= 0) {
				pParams->fieldIdx.emplace_back(d->fieldIdx_secData);
			}
			if (d->fieldIdx_secArea >= 0) {
				pParams->fieldIdx.emplace_back(d->fieldIdx_secArea);
			}
			break;
		}
#endif /* ENABLE_DECRYPTION */

		default:
			ret = -EINVAL;
			pParams->status = -EINVAL;
			pParams->msg = C_("RomData", "ROM operation ID is invalid for this object.");
			break;
	}

	return ret;
}

}
