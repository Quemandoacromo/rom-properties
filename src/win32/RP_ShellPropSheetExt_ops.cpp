/***************************************************************************
 * ROM Properties Page shell extension. (Win32)                            *
 * RP_ShellPropSheetExt_ops.cpp: IShellPropSheetExt implementation.        *
 * (ROM operations)                                                        *
 *                                                                         *
 * Copyright (c) 2016-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"

#include "RP_ShellPropSheetExt.hpp"
#include "RP_ShellPropSheetExt_p.hpp"
#include "res/resource.h"

// Custom controls
#include "MessageWidget.hpp"
#include "LanguageComboBox.hpp"
#include "OptionsMenuButton.hpp"

// librpbase, librpfile
#include "librpbase/TextOut.hpp"
using namespace LibRpBase;
using namespace LibRpFile;

// C++ STL classes
#include <fstream>
#include <sstream>
using std::ofstream;
using std::ostringstream;
using std::string;
using std::vector;
using std::wstring;	// for tstring

/**
 * Update a field's value.
 * This is called after running a ROM operation.
 * @param fieldIdx Field index.
 * @return 0 on success; non-zero on error.
 */
int RP_ShellPropSheetExt_Private::updateField(int fieldIdx)
{
	const RomFields *const pFields = romData->fields();
	assert(pFields != nullptr);
	if (!pFields) {
		// No fields.
		// TODO: Show an error?
		return 1;
	}

	assert(fieldIdx >= 0);
	assert(fieldIdx < pFields->count());
	if (fieldIdx < 0 || fieldIdx >= pFields->count())
		return 2;

	const RomFields::Field *const field = pFields->at(fieldIdx);
	assert(field != nullptr);
	if (!field)
		return 3;

	// Get the tab dialog control the control is in.
	assert(field->tabIdx >= 0);
	assert(field->tabIdx < tabs.size());
	if (field->tabIdx < 0 || field->tabIdx >= tabs.size())
		return 4;
	HWND hDlg = tabs[field->tabIdx].hDlg;

	// Update the value widget(s).
	int ret;
	switch (field->type) {
		case RomFields::RFT_INVALID:
			assert(!"Cannot update an RFT_INVALID field.");
			ret = 5;
			break;
		default:
			assert(!"Unsupported field type.");
			ret = 6;
			break;

		case RomFields::RFT_STRING: {
			// HWND is a STATIC control.
			HWND hLabel = GetDlgItem(hDlg, IDC_RFT_STRING(fieldIdx));
			assert(hLabel != nullptr);
			if (!hLabel) {
				ret = 7;
				break;
			}

			if (field->data.str && !field->data.str->empty()) {
				const tstring ts_text = LibWin32Common::unix2dos(U82T_s(*field->data.str));
				SetWindowText(hLabel, ts_text.c_str());
			} else {
				SetWindowText(hLabel, _T(""));
			}
			ret = 0;
			break;
		}

		case RomFields::RFT_BITFIELD: {
			// Multiple checkboxes with unique dialog IDs.

			// Bits with a blank name aren't included, so we'll need to iterate
			// over the bitfield description.
			const auto &bitfieldDesc = field->desc.bitfield;
			int count = (int)bitfieldDesc.names->size();
			assert(count <= 32);
			if (count > 32)
				count = 32;

			// Unlike GTK+ and KDE, we don't need to check bitfieldDesc.names to determine
			// if a checkbox is present, since GetDlgItem() will return nullptr in that case.
			uint32_t bitfield = field->data.bitfield;
			int id = IDC_RFT_BITFIELD(fieldIdx, 0);
			for (; count >= 0; count--, id++, bitfield >>= 1) {
				HWND hCheckBox = GetDlgItem(hDlg, id);
				if (!hCheckBox)
					continue;

				// Set the checkbox.
				Button_SetCheck(hCheckBox, (bitfield & 1) ? BST_CHECKED : BST_UNCHECKED);
			}
			ret = 0;
			break;
		}
	}

	return ret;
}

/**
 * An "Options" menu button action was triggered.
 * @param menuId Menu ID. (Options ID + IDM_OPTIONS_MENU_BASE)
 */
void RP_ShellPropSheetExt_Private::btnOptions_action_triggered(int menuId)
{
	if (menuId < IDM_OPTIONS_MENU_BASE) {
		// Export to text or JSON.
		const char *const rom_filename = romData->filename();
		if (!rom_filename)
			return;

		const unsigned int id2 = static_cast<unsigned int>(abs(menuId - IDM_OPTIONS_MENU_BASE)) - 1;
		assert(id2 < 4);
		if (id2 >= 4) {
			// Out of range.
			return;
		}

		struct StdActsInfo_t {
			const char *title;	// NOP_C_
			const char *filter;	// NOP_C_
			TCHAR default_ext[7];
			bool toClipboard;
		};
		static const StdActsInfo_t stdActsInfo[] = {
			// OPTION_EXPORT_TEXT
			{NOP_C_("RomDataView", "Export to Text File"),
			 NOP_C_("RomDataView", "Text Files|*.txt|text/plain|All Files|*.*|-"),
			 _T(".txt"), false},

			// OPTION_EXPORT_JSON
			{NOP_C_("RomDataView", "Export to JSON File"),
			 NOP_C_("RomDataView", "JSON Files|*.json|application/json|All Files|*.*|-"),
			 _T(".json"), false},

			// OPTION_COPY_TEXT
			{nullptr, nullptr, _T(""), true},

			// OPTION_COPY_JSON
			{nullptr, nullptr, _T(""), true},
		};

		// Standard Actions information for this action
		const StdActsInfo_t *const info = &stdActsInfo[id2];

		ofstream ofs;
		tstring ts_out;

		if (!info->toClipboard) {
			if (ts_prevExportDir.empty()) {
				ts_prevExportDir = U82T_c(rom_filename);

				// Remove the filename portion.
				size_t bspos = ts_prevExportDir.rfind(_T('\\'));
				if (bspos != string::npos) {
					if (bspos > 2) {
						ts_prevExportDir.resize(bspos);
					} else if (bspos == 2) {
						ts_prevExportDir.resize(3);
					}
				}
			}

			tstring defaultFileName = ts_prevExportDir;
			if (!defaultFileName.empty() && defaultFileName.at(defaultFileName.size()-1) != _T('\\')) {
				defaultFileName += _T('\\');
			}

			// Get the base name of the ROM.
			tstring rom_basename;
			const char *const bspos = strrchr(rom_filename, '\\');
			if (bspos) {
				rom_basename = U82T_c(bspos+1);
			} else {
				rom_basename = U82T_c(rom_filename);
			}
			// Remove the extension, if present.
			size_t extpos = rom_basename.rfind(_T('.'));
			if (extpos != string::npos) {
				rom_basename.resize(extpos);
			}
			defaultFileName += rom_basename;
			if (info->default_ext[0] != _T('\0')) {
				defaultFileName += info->default_ext;
			}

			const tstring tfilename = LibWin32Common::getSaveFileName(hDlgSheet,
				U82T_c(dpgettext_expr(RP_I18N_DOMAIN, "RomDataView", info->title)),
				dpgettext_expr(RP_I18N_DOMAIN, "RomDataView", info->filter),
				defaultFileName.c_str());
			if (tfilename.empty())
				return;

			// Save the previous export directory.
			ts_prevExportDir = tfilename;
			size_t bspos2 = ts_prevExportDir.rfind(_T('\\'));
			if (bspos2 != tstring::npos && bspos2 > 3) {
				ts_prevExportDir.resize(bspos2);
			}

#ifdef __GNUC__
			// FIXME: MinGW doesn't have wchar_t overloads.
			ofs.open(T2U8(tfilename).c_str(), ofstream::out);
#else /* !__GNUC__ */
			ofs.open(tfilename.c_str(), ofstream::out);
#endif /* __GNUC__ */
			if (ofs.fail())
				return;
		}

		// TODO: Optimize this such that we can pass ofstream or ostringstream
		// to a factored-out function.

		switch (menuId) {
			case IDM_OPTIONS_MENU_EXPORT_TEXT: {
				const uint32_t lc = cboLanguage
					? LanguageComboBox_GetSelectedLC(cboLanguage)
					: 0;
				ofs << "== " << rp_sprintf(C_("RomDataView", "File: '%s'"), rom_filename) << '\n';
				ROMOutput ro(romData, lc);
				ofs << ro;
				ofs.flush();
				break;
			}
			case IDM_OPTIONS_MENU_EXPORT_JSON: {
				JSONROMOutput jsro(romData);
				ofs << jsro << '\n';
				ofs.flush();
				break;
			}
			case IDM_OPTIONS_MENU_COPY_TEXT: {
				// NOTE: Some fields may have embedded newlines,
				// so we'll need to convert everything afterwards.
				const uint32_t lc = cboLanguage
					? LanguageComboBox_GetSelectedLC(cboLanguage)
					: 0;
				ostringstream oss;
				oss << "== " << rp_sprintf(C_("RomDataView", "File: '%s'"), rom_filename) << '\n';
				ROMOutput ro(romData, lc);
				oss << ro;
				oss.flush();
				ts_out = LibWin32Common::unix2dos(U82T_s(oss.str()));
				break;
			}
			case IDM_OPTIONS_MENU_COPY_JSON: {
				ostringstream oss;
				JSONROMOutput jsro(romData);
				jsro.setCrlf(true);
				oss << jsro << '\n';
				oss.flush();
				ts_out = U82T_s(oss.str());
				break;
			}
			default:
				assert(!"Invalid action ID.");
				return;
		}

		if (info->toClipboard && OpenClipboard(hDlgSheet)) {
			EmptyClipboard();
			HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (ts_out.size() + 1) * sizeof(TCHAR));
			if (hglbCopy) {
				LPTSTR lpszCopy = static_cast<LPTSTR>(GlobalLock(hglbCopy));
				memcpy(lpszCopy, ts_out.data(), ts_out.size() * sizeof(TCHAR));
				lpszCopy[ts_out.size()] = _T('\0');
				GlobalUnlock(hglbCopy);
				SetClipboardData(CF_UNICODETEXT, hglbCopy);
			}
			CloseClipboard();
		}
		return;
	}

	// Run a ROM operation.
	// TODO: Don't keep rebuilding this vector...
	vector<RomData::RomOp> ops = romData->romOps();
	const int id = menuId - IDM_OPTIONS_MENU_BASE;
	assert(id < (int)ops.size());
	if (id >= (int)ops.size()) {
		// ID is out of range.
		return;
	}

	string s_save_filename;
	RomData::RomOpParams params;
	const RomData::RomOp *op = &ops[id];
	if (op->flags & RomData::RomOp::ROF_SAVE_FILE) {
		// Add the "All Files" filter.
		string filter = op->sfi.filter;
		if (!filter.empty()) {
			// Make sure the last field isn't empty.
			if (filter.at(filter.size()-1) == '|') {
				filter += '-';
			}
			filter += '|';
		}
		// tr: "All Files" filter (RP format)
		filter += C_("RomData", "All Files|*.*|-");

		// Initial file and directory, based on the current file.
		string initialFile = FileSystem::replace_ext(romData->filename(), op->sfi.ext);

		// Prompt for a save file.
		tstring t_save_filename = LibWin32Common::getSaveFileName(hDlgSheet,
			U82T_c(op->sfi.title), filter.c_str(), U82T_s(initialFile));
		if (t_save_filename.empty())
			return;
		s_save_filename = T2U8(t_save_filename);
		params.save_filename = s_save_filename.c_str();
	}

	int ret = romData->doRomOp(id, &params);
	unsigned int messageType;
	if (ret == 0) {
		// ROM operation completed.
		messageType = MB_ICONINFORMATION;

		// Update fields.
		for (int fieldIdx : params.fieldIdx) {
			this->updateField(fieldIdx);
		}

		// Update the RomOp menu entry in case it changed.
		// NOTE: Assuming the RomOps vector order hasn't changed.
		ops = romData->romOps();
		assert(id < (int)ops.size());
		if (id < (int)ops.size()) {
			OptionsMenuButton_UpdateOp(hBtnOptions, id, &ops[id]);
		}
	} else {
		// An error occurred...
		// TODO: Show an error message.
		messageType = MB_ICONWARNING;
	}

	if (!params.msg.empty()) {
		MessageBeep(messageType);

		if (!hMessageWidget) {
			// FIXME: Make sure this works if multiple tabs are present.
			MessageWidgetRegister();

			// Align to the bottom of the dialog and center-align the text.
			// 7x7 DLU margin is recommended by the Windows UX guidelines.
			// Reference: http://stackoverflow.com/questions/2118603/default-dialog-padding
			RECT tmpRect = {7, 7, 8, 8};
			MapDialogRect(hDlgSheet, &tmpRect);
			RECT winRect;
			GetClientRect(hDlgSheet, &winRect);
			// NOTE: We need to move left by 1px.
			OffsetRect(&winRect, -1, 0);

			// Determine the position.
			// TODO: Update on DPI change.
			const int cySmIcon = GetSystemMetrics(SM_CYSMICON);
			SIZE szMsgw = {winRect.right - winRect.left, cySmIcon + 8};
			POINT ptMsgw = {winRect.left, winRect.bottom - szMsgw.cy};
			if (tabs.size() > 1) {
				ptMsgw.x += tmpRect.left;
				ptMsgw.y -= tmpRect.top;
				szMsgw.cx -= (tmpRect.left * 2);
			} else {
				ptMsgw.x += (tmpRect.left / 2);
				ptMsgw.y -= (tmpRect.top / 2);
				szMsgw.cx -= tmpRect.left;
			}

			hMessageWidget = CreateWindowEx(
				WS_EX_NOPARENTNOTIFY | WS_EX_TRANSPARENT | dwExStyleRTL,
				WC_MESSAGEWIDGET, nullptr,
				WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
				ptMsgw.x, ptMsgw.y, szMsgw.cx, szMsgw.cy,
				hDlgSheet, (HMENU)IDC_MESSAGE_WIDGET,
				HINST_THISCOMPONENT, nullptr);
			SetWindowFont(hMessageWidget, GetWindowFont(hDlgSheet), false);
		}

		showMessageWidget(messageType, U82T_s(params.msg));
	}
}