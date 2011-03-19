/*
 * purple - Xfire Protocol Plugin
 *
 * Copyright (C) 2005-2006, Beat Wolf <asraniel@fryx.ch>
 * Copyright (C) 2006,      Keith Geffert <keith@penguingurus.com>
 * Copyright (C) 2008-2009	Laurent De Marez <laurentdemarez@gmail.com>
 * Copyright (C) 2009-2010  Warren Dumortier <nwarrenfl@gmail.com>
 * Copyright (C) 2009-2010  Oliver Ney <oliver@dryder.de>
 *
 * This file is part of Gfire.
 *
 * Gfire is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Gfire.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _GF_PREFERENCES_H
#define _GF_PREFERENCES_H

typedef struct _gfire_preferences gfire_preferences;

#include "gf_base.h"

typedef struct _gf_pref
{
	guint8 id;
	gboolean set;
} gf_pref;

struct _gfire_preferences
{
	GList *prefs;
};

// Creation/freeing
gfire_preferences *gfire_preferences_create();
void gfire_preferences_free(gfire_preferences *p_prefs);

// Handling
void gfire_preferences_set(gfire_preferences *p_prefs, guint8 p_id, gboolean p_set);
gboolean gfire_preferences_get(const gfire_preferences *p_prefs, guint8 p_id);

// Sending to Xfire
void gfire_preferences_send(const gfire_preferences *p_prefs, PurpleConnection *p_gc);

#endif // _GF_PREFERENCES_H
