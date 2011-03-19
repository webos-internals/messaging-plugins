/*
 * libfacebook
 *
 * libfacebook is the property of its developers.  See the COPYRIGHT file
 * for more details.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FACEBOOK_CHAT_H
#define FACEBOOK_CHAT_H

#include "libfacebook.h"

PurpleConversation *fb_find_chat(FacebookAccount *fba, const gchar *group);
void fb_get_groups(FacebookAccount *fba);
int fb_chat_send(PurpleConnection *, int id, const char *message, PurpleMessageFlags flags);
void fb_chat_fake_leave(PurpleConnection *, int id);
GHashTable *fb_chat_info_defaults(PurpleConnection *, const char *chat_name);
gchar *fb_get_chat_name(GHashTable *components);
GList *fb_chat_info(PurpleConnection *);
void fb_fake_join_chat(PurpleConnection *, GHashTable *components);

#endif /* FACEBOOK_CHAT_H */
