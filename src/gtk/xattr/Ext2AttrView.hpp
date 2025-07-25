/***************************************************************************
 * ROM Properties Page shell extension. (GTK+ common)                      *
 * Ext2AttrView.hpp: Ext2 file system attribute viewer widget.             *
 *                                                                         *
 * Copyright (c) 2017-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include "gtk-compat.h"

#ifdef __cplusplus
#  include "librpfile/xattr/XAttrReader.hpp"
#endif /* __cplusplus */

G_BEGIN_DECLS

#define RP_TYPE_EXT2_ATTR_VIEW (rp_ext2_attr_view_get_type())

#if GTK_CHECK_VERSION(3, 0, 0)
#  define _RpExt2AttrView_super		GtkBox
#  define _RpExt2AttrView_superClass	GtkBoxClass
#else /* !GTK_CHECK_VERSION(3, 0, 0) */
#  define _RpExt2AttrView_super		GtkVBox
#  define _RpExt2AttrView_superClass	GtkVBoxClass
#endif /* GTK_CHECK_VERSION(3, 0, 0) */

G_DECLARE_FINAL_TYPE(RpExt2AttrView, rp_ext2_attr_view, RP, EXT2_ATTR_VIEW, _RpExt2AttrView_super)

/* this function is implemented automatically by the G_DEFINE_TYPE macro */
void		rp_ext2_attr_view_register_type	(GtkWidget *widget) G_GNUC_INTERNAL;

GtkWidget	*rp_ext2_attr_view_new			(void) G_GNUC_MALLOC;

void		rp_ext2_attr_view_set_flags		(RpExt2AttrView *widget, int flags);
int 		rp_ext2_attr_view_get_flags		(RpExt2AttrView *widget);
void		rp_ext2_attr_view_clear_flags		(RpExt2AttrView *widget);

G_END_DECLS

#ifdef __cplusplus
void					rp_ext2_attr_view_set_zAlgorithm	(RpExt2AttrView *widget, LibRpFile::XAttrReader::ZAlgorithm zAlgorithm);
LibRpFile::XAttrReader::ZAlgorithm	rp_ext2_attr_view_get_zAlgorithm	(RpExt2AttrView *widget);
void					rp_ext2_attr_view_clear_zAlgorithm	(RpExt2AttrView *widget);

void					rp_ext2_attr_view_set_zLevel		(RpExt2AttrView *widget, int zLevel);
int					rp_ext2_attr_view_get_zLevel		(RpExt2AttrView *widget);
void					rp_ext2_attr_view_clear_zLevel		(RpExt2AttrView *widget);

void					rp_ext2_attr_view_set_zAlgorithm_and_zLevel	(RpExt2AttrView *widget, LibRpFile::XAttrReader::ZAlgorithm zAlgorithm, int zLevel);
void					rp_ext2_attr_view_clear_zAlgorithm_and_zLevel	(RpExt2AttrView *widget);
#endif /* __cplusplus */
