/*
 * purple - Xfire Protocol Plugin
 *
 * Copyright (C) 2000-2001, Beat Wolf <asraniel@fryx.ch>
 * Copyright (C) 2006,      Keith Geffert <keith@penguingurus.com>
 * Copyright (C) 2008,	    Laurent De Marez <laurentdemarez@gmail.com>
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
#include "gfire.h"
#include "gf_packet.h"
#include "gf_network.h"
#include "gf_games.h"


void gfire_xml_download_cb(

	PurpleUtilFetchUrlData *url_data,
	gpointer data,
	const char *buf,

	gsize len,
	const gchar *error_message
	);


/**
 * Returns via *buffer the name of the game identified by int game
 * if the game doesn't exist, it uses the game number as the name
 *
 * @param buffer			pointer to string return value will be here
 * @param game				integer ID of the game to translate
*/
char *gfire_game_name(PurpleConnection *gc, int game)
{
	xmlnode *node = NULL;
	const char *name = NULL;
	const char *num = NULL;
	char *ret = NULL;
	int found = FALSE;
	int id = 0;
	gfire_data *gfire = NULL;

	if (!gc || !(gfire = (gfire_data *)gc->proto_data)) return NULL;

	if (gfire->xml_games_list != NULL) {
		node = xmlnode_get_child(gfire->xml_games_list, "game");
		while (node) {
			name = xmlnode_get_attrib(node, "name");
			num  = xmlnode_get_attrib(node, "id");
			id = atoi((const char *)num);
			if (id == game) {
				found = TRUE;
			}
			if (found) break;
			node = xmlnode_get_next_twin(node);
		}
		/* if we didn't find the game just show game ID */
		if (!found) {
			ret = g_strdup_printf("%d",game);
		} else ret = g_strdup(gfire_escape_html(name));
	}else{
		/* uhh our gfire_games.xml was not found */
		ret = g_strdup_printf("%d",game);
	}
	return ret;
}


/**
 * Parses XML file to convert xfire game ID's to names
 *
 * @param filename		The filename to parse (xml)
 *
 * @return TRUE if parsed ok, FALSE otherwise
*/
gboolean gfire_parse_games_file(PurpleConnection *gc, const char *filename)
{
	xmlnode *node = NULL;
	GError *error = NULL;
	gchar *contents = NULL;
	gsize length;
	gfire_data *gfire = NULL;

	if (!gc || !(gfire = (gfire_data *)gc->proto_data)) return FALSE;

	purple_debug(PURPLE_DEBUG_INFO, "gfire",
			   "XML Games import, Reading %s\n", NN(filename));

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		/* file not found try to grab from url */

		purple_util_fetch_url(GFIRE_GAMES_XML_URL, TRUE, "Purple-xfire", TRUE, gfire_xml_download_cb, (void *)gc);

		return FALSE;
	}

	if (!g_file_get_contents(filename, &contents, &length, &error)) {
		purple_debug(PURPLE_DEBUG_ERROR, "gfire",
				   "XML Games import, Error reading game list: %s\n", NN(error->message));
		g_error_free(error);
		return FALSE;
	}

	node = xmlnode_from_str(contents, length);

	if (!node) {
		purple_debug(PURPLE_DEBUG_ERROR, "gfire",
				   "XML Games import, Error parsing XML file: %s\n", NN(filename));
		g_free(contents);
		return FALSE;
	}

	gfire->xml_games_list = node;
	g_free(contents);
	return TRUE;
};

/**
 * gives the integer representation of the game the buddy is playing
 *
 * @param gc	Valid PurpleConnection
 * @param b		Buddy to get game number of
 *
 * @return returns the game or 0 if the buddy is not playing
*/
int gfire_get_buddy_game(PurpleConnection *gc, PurpleBuddy *b)
{
		gfire_data *gfire = NULL;
		GList *l = NULL;
		gfire_buddy *gb = NULL;

	if (!gc || !(gfire = (gfire_data *)gc->proto_data) || !b || !b->name) return 0;

	l = gfire_find_buddy_in_list(gfire->buddies, (gpointer *)b->name, GFFB_NAME);
	if (!l || !(l->data)) return 0;
	gb = (gfire_buddy *)l->data;
	return gb->gameid;
}


/**
 * get port number of the game the buddy is playing
 *
 * @param gc	Valid PurpleConnection
 * @param b		Buddy to get game number of
 *
 * @return returns the game or 0 if the buddy is not playing
*/
int gfire_get_buddy_port(PurpleConnection *gc, PurpleBuddy *b)
{
		gfire_data *gfire = NULL;
		GList *l = NULL;
		gfire_buddy *gb = NULL;

	if (!gc || !(gfire = (gfire_data *)gc->proto_data) || !b || !b->name) return 0;

	l = gfire_find_buddy_in_list(gfire->buddies, (gpointer *)b->name, GFFB_NAME);
	if (!l || !(l->data)) return 0;
	gb = (gfire_buddy *)l->data;
	return gb->gameport;
}


/**
 * get ip address of server where buddy is in game
 *
 * @param gc	Valid PurpleConnection
 * @param b		Buddy to get game ip number of
 *
 * @return returns the ip address as a string or NULL if budy isn't playing
*/
const gchar *gfire_get_buddy_ip(PurpleConnection *gc, PurpleBuddy *b)
{
		gfire_data *gfire = NULL;
		GList *l = NULL;
		gfire_buddy *gb = NULL;
		gchar *tmp = NULL;

	if (!gc || !(gfire = (gfire_data *)gc->proto_data) || !b || !b->name) return 0;

	l = gfire_find_buddy_in_list(gfire->buddies, (gpointer *)b->name, GFFB_NAME);
	if (!l || !(l->data)) return NULL;
	gb = (gfire_buddy *)l->data;
	if(gfire_get_buddy_game(gc ,b) != 0){
		tmp = g_malloc0(XFIRE_GAMEIP_LEN);
		memcpy(tmp, gb->gameip, XFIRE_GAMEIP_LEN);
//		g_sprintf(tmp, "%d.%d.%d.%d", gb->gameip[3], gb->gameip[2], gb->gameip[1], gb->gameip[0]);
		return tmp;
	}
	return NULL;
}

/**
 *	Return ip address struct initialized with data supplied by argument
 *	When done with structure, it should be freed with xfire_ip_struct_free()
 *
 *	@param		ip		string ip address to fill struct with ex ("127.0.0.1")
 *
 *	@return				returns NULL if no string provided. Otherwise returns
 *						pointer to struct.  ex: s = struct,
 *
 *						s->octet = { 127, 0, 0, 1 }
 *						s->ipstr = "127.0.0.1"
 *
 *						Structure should be freed with xfire_ip_struct_free()
*/
gchar *gfire_ipstr_to_bin(const gchar *ip)
{
	gchar **ss = NULL;
	int i = 0; int j = 3;
	gchar *ret = NULL;

	if (strlen(ip) <= 0) return NULL;

	ss = g_strsplit(ip,".", 0);
	if (g_strv_length(ss) != 4) {
		g_strfreev(ss);
		return NULL;
	}
	ret = g_malloc0(sizeof(gchar) * XFIRE_GAMEIP_LEN);
	if (!ret) {
		g_strfreev(ss);
		return NULL;
	}
	j = 3;
	for (i=0; i < 4; i++) {
		ret[j--] = atoi(ss[i]);
	}
	g_strfreev(ss);
	return ret;
}



void gfire_xml_download_cb( PurpleUtilFetchUrlData *url_data, gpointer data, const char *buf, gsize len, const gchar *error_message)
{
	char *successmsg = NULL;
	PurpleConnection *gc = NULL;
	const char *filename = g_build_filename(purple_user_dir(), "gfire_games.xml", NULL);

	if (! data || !buf || !len) {
		purple_notify_message(NULL, PURPLE_NOTIFY_MSG_ERROR, "XFire games Download", "Will attempt to download gfire_games.xml from the Gfire server.", "Unable to download gfire_games.xml", NULL, NULL);
		return;
	}
	gc = (PurpleConnection *)data;
	if ( purple_util_write_data_to_file("gfire_games.xml", buf, len) ) {
		/* we may be called when we are no longer connected gfire-> may not be vaild */
		if ( PURPLE_CONNECTION_IS_VALID(gc) && PURPLE_CONNECTION_IS_CONNECTED(gc) ) {
			gfire_parse_games_file(gc, filename);
		}
		if(strcmp(gfire_game_name(gc, 100), "100")) {
			successmsg = g_strdup_printf("Successfully downloaded gfire_games.xml\nNew Games List Version: %s", gfire_game_name(gc, 100));
		} else {
			successmsg = g_strdup_printf("Successfully downloaded gfire_games.xml");
		}
		purple_notify_message(NULL, PURPLE_NOTIFY_MSG_INFO, "XFire games Download", "Will attempt to download gfire_games.xml from the Gfire server.", successmsg, NULL, NULL);

	} else {
		purple_notify_message(NULL, PURPLE_NOTIFY_MSG_ERROR, "XFire games Download", "Will attempt to download gfire_games.xml from the Gfire server.", "Unable to write gfire_games.xml", NULL, NULL);
	}
}

