/***************************************************************************
 * ROM Properties Page shell extension. (KDE4/KF5)                         *
 * AchQtDBus.hpp: QtDBus notifications for achievements.                   *
 *                                                                         *
 * Copyright (c) 2020-2024 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

#include <cstdint>
#include "common.h"

// Achievements
#include "librpbase/Achievements.hpp"

class AchQtDBus
{
protected:
	/**
	 * AchQtDBus class.
	 *
	 * This class is a Singleton, so the caller must obtain a
	 * pointer to the class using instance().
	 */
	AchQtDBus();
	~AchQtDBus();
private:
	RP_DISABLE_COPY(AchQtDBus);

private:
	// Static AchQtDBus instance.
	// TODO: Q_GLOBAL_STATIC() equivalent, though we
	// may need special initialization if the compiler
	// doesn't support thread-safe statics.
	static AchQtDBus m_instance;
	bool m_hasRegistered;

private:
	/**
	 * Notification function (static)
	 * @param user_data	[in] User data (this)
	 * @param id		[in] Achievement ID
	 * @return 0 on success; negative POSIX error code on error.
	 */
	static int RP_C_API notifyFunc(void *user_data, LibRpBase::Achievements::ID id);

	/**
	 * Notification function (non-static)
	 * @param id	[in] Achievement ID
	 * @return 0 on success; negative POSIX error code on error.
	 */
	int notifyFunc(LibRpBase::Achievements::ID id);

public:
	/**
	 * Get the AchQtDBus instance.
	 *
	 * This automatically initializes librpbase's Achievement
	 * object and reloads the achievements data if it has been
	 * modified.
	 *
	 * @return AchQtDBus instance.
	 */
	static AchQtDBus *instance(void);
};
