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

#include "fb_chat.h"
#include "fb_blist.h"
#include "fb_util.h"
#include "fb_connection.h"
#include "fb_friendlist.h"
#include "fb_messages.h"
#include "fb_conversation.h"

#include "conversation.h"

void
fb_got_facepile(FacebookAccount *fba, const gchar *data, gsize data_len, gpointer user_data)
{
	gchar *group = user_data;
	JsonParser *parser;
	JsonObject *object, *payload, *user_obj;
	JsonArray *facepile;
	PurpleConversation *conv;
	PurpleConvChat *chat;
	gchar *uid;
	guint i;
	PurpleGroup *pgroup;
	
	purple_debug_info("facebook", "got facepile %s\n", data?data:"(null)");
	
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, group, fba->account);
	chat = PURPLE_CONV_CHAT(conv);
	
	parser = fb_get_parser(data, data_len);
	
	if (!parser)
	{
		purple_debug_warning("facebook",
			"could not fetch facepile for group %s\n", group);
		g_free(group);
		return;
	}
	
	object = fb_get_json_object(parser, NULL);
	payload = json_node_get_object(
		json_object_get_member(object, "payload"));
	facepile = json_node_get_array(
		json_object_get_member(payload, "facepile_click_info"));
	
	pgroup = purple_find_group(DEFAULT_GROUP_NAME);
	if (!pgroup)
	{
		pgroup = purple_group_new(DEFAULT_GROUP_NAME);
		purple_blist_add_group(pgroup, NULL);
	}

	purple_conv_chat_clear_users(chat);
	uid = g_strdup_printf("%" G_GINT64_FORMAT, fba->uid);
	purple_conv_chat_add_user(chat, uid, NULL, PURPLE_CBFLAGS_NONE, FALSE);
	if (!purple_find_buddy(fba->account, uid))
	{
		PurpleBuddy *buddy = purple_buddy_new(fba->account, uid, "You");
		purple_blist_node_set_flags((PurpleBlistNode *)buddy, PURPLE_BLIST_NODE_FLAG_NO_SAVE);
		purple_blist_add_buddy(buddy, NULL, pgroup, NULL);
	}
	g_free(uid);
	
	for (i = 0; i < json_array_get_length(facepile); i++)
	{
		user_obj = json_node_get_object(
			json_array_get_element(facepile, i));
		uid = g_strdup_printf("%" G_GINT64_FORMAT, (gint64)json_node_get_int(json_object_get_member(user_obj, "uid")));
		
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), uid, NULL, PURPLE_CBFLAGS_NONE, FALSE);
		
		if (!purple_find_buddy(fba->account, uid))
		{
			const char *alias = json_node_get_string(json_object_get_member(user_obj, "name"));
			PurpleBuddy *buddy = purple_buddy_new(fba->account, uid, alias);
			purple_blist_node_set_flags((PurpleBlistNode *)buddy, PURPLE_BLIST_NODE_FLAG_NO_SAVE);
			purple_blist_add_buddy(buddy, NULL, pgroup, NULL);
		}
		
		g_free(uid);
	}
	
	g_free(group);
}

PurpleConversation *
fb_find_chat(FacebookAccount *fba, const gchar *group)
{
	PurpleConversation *conv;
	gchar *postdata;
	
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, group, fba->account);
	
	if (conv == NULL)
	{
		conv = serv_got_joined_chat(fba->pc, atoi(group), group);
		
		postdata = g_strdup_printf("gid=%s&post_form_id=%s&fb_dtsg=%s&lsd=", group,
					fba->post_form_id, fba->dtsg);
		fb_post_or_get(fba, FB_METHOD_POST, NULL, "/ajax/groups/chat/update_facepiles.php?__a=1",
			postdata, fb_got_facepile, g_strdup(group), FALSE);
		g_free(postdata);
	}
	
	return conv;
}

void
fb_got_groups(FacebookAccount *fba, const gchar *data, gsize data_len, gpointer user_data)
{
	// look for  /home.php?sk=group_ ...
	gchar **splits;
	gint i;
	PurpleGroup *group;
	
	splits = g_strsplit(data, "<a href=\\\"\\/home.php?sk=group_", 0);
	
	if (!splits || !splits[0])
	{
		g_strfreev(splits);
		return;
	}
	
	group = purple_find_group(DEFAULT_GROUP_NAME);
	if (!group)
	{
		group = purple_group_new(DEFAULT_GROUP_NAME);
		purple_blist_add_group(group, NULL);
	}
	
	for(i = 1; splits[i]; i++)
	{
		gchar *eos;
		eos = strchr(splits[i], '\\');
		if (eos != NULL)
		{
			*eos = '\0';
			purple_debug_info("facebook", "searching for %s\n", splits[i]);
			if (!purple_blist_find_chat(fba->account, splits[i]))
			{
				gchar *alias = NULL;
				if (eos[1] == '"' && eos[2] == '>')
				{
					purple_debug_info("facebook", "searching for alias\n");
					gchar *eoa = strchr(&eos[3], '<');
					if (eoa)
					{
						*eoa = '\0';
						alias = &eos[3];
						purple_debug_info("facebook", "found chat alias %s\n", alias);
					}
				}

				purple_debug_info("facebook", "adding chat %s to buddy list...\n", splits[i]);
				// Add the group chat to the buddy list
				GHashTable *components = fb_chat_info_defaults(fba->pc, splits[i]);
				PurpleChat *chat = purple_chat_new(fba->account, alias, components);
				purple_blist_add_chat(chat, group, NULL);
				purple_debug_info("facebook", "done\n");
			}
		}
	}
	
	g_strfreev(splits);
}

void
fb_get_groups(FacebookAccount *fba)
{
	fb_post_or_get(fba, FB_METHOD_GET, NULL, "/ajax/home/groups.php?__a=1", NULL, fb_got_groups, NULL, FALSE);
}

int
fb_chat_send(PurpleConnection *pc, int id, const char *message, PurpleMessageFlags flags)
{
	PurpleConversation *conv;
	const char *group;
	
	conv = purple_find_chat(pc, id);
	if (conv != NULL)
	{
		group = purple_conversation_get_name(conv);
		
		return fb_send_im(pc, group, message, flags);
	}
	
	return -1;
}

void
fb_chat_fake_leave(PurpleConnection *pc, int id)
{
	PurpleConversation *conv;
	const char *group;
	
	conv = purple_find_chat(pc, id);
	if (conv != NULL)
	{
		group = purple_conversation_get_name(conv);
		fb_conversation_closed(pc, group);
	}
}

GHashTable *
fb_chat_info_defaults(PurpleConnection *pc, const char *chat_name)
{
	GHashTable *table;
	
	table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	
	if (chat_name != NULL)
	{
		g_hash_table_insert(table, "group", g_strdup(chat_name));
	}
	
	return table;
}

gchar *
fb_get_chat_name(GHashTable *components)
{
	gchar *group;
	
	group = (gchar *) g_hash_table_lookup(components, "group");
	
	return g_strdup(group);
}

GList *
fb_chat_info(PurpleConnection *pc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;
	
	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Group ID");
	pce->identifier = "group";
	pce->required = TRUE;
	m = g_list_append(m, pce);
	
	return m;
}

void
fb_fake_join_chat(PurpleConnection *pc, GHashTable *components)
{
	FacebookAccount *fba = pc->proto_data;
	gchar *group = (gchar *) g_hash_table_lookup(components, "group");

	if (group != NULL)
	{
		fb_find_chat(fba, group);
	}
}
