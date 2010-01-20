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
#include "gf_chat.h"

/* we only include this on win32 builds */
#  ifdef _WIN32
#    include "internal.h"
#  endif /* _WIN32 */



void gfire_parse_packet(PurpleConnection *gc, int packet_len, int packet_id);


void gfire_send(PurpleConnection *gc, const guint8 *packet, int size)
{
	GTimeVal gtv;
	gfire_data *gfire = NULL;
	int tmp = 0;
	int errsv = 0;

	if (!gc || !(gfire = (gfire_data *)gc->proto_data)) return;
//	purple_debug(PURPLE_DEBUG_MISC, "gfire", "(send): fd %d, size %d\n",gfire->fd, size);
	if (gfire->fd > 0 && size > 0) {
		tmp = send(gfire->fd, packet, size, 0);
		errsv = errno;
		purple_debug(PURPLE_DEBUG_MISC, "gfire", "(send): wrote %d bytes\n",tmp);
		if (tmp < 0) purple_debug(PURPLE_DEBUG_ERROR, "gfire", "(send): error %d, %s\n",errsv, strerror(errsv));
	}
	if (NULL != gfire->buff_out) memset((void *)gfire->buff_out, 0x00, GFIRE_BUFFOUT_SIZE);
	g_get_current_time(&gtv);
	gfire->last_packet = gtv.tv_sec;
}


int gfire_statistics(guint8 *packet)
{
	int length = 41 + strlen(getenv("LANG") ? getenv("LANG") : "en_GB") + strlen("Gfire") + strlen(GFIRE_VERSION);
	int index = 0;

	gfire_add_header(packet, length, 0x10, 4);/*add header*/
	index += 5;

	index = gfire_add_att_name(packet,index, "lang");
	packet[index] = 0x01;
	*((guint16*)&packet[index+1]) = GUINT16_TO_LE(strlen(getenv("LANG") ? getenv("LANG") : "en_GB"));
	memcpy(packet + index + 3, getenv("LANG") ? getenv("LANG") : "en_GB", strlen(getenv("LANG") ? getenv("LANG") : "en_GB"));
	index += 3 + strlen(getenv("LANG") ? getenv("LANG") : "en_GB");

	index = gfire_add_att_name(packet,index, "skin");
	packet[index] = 0x01;
	*((guint16*)&packet[index+1]) = GUINT16_TO_LE(strlen("Gfire"));
	memcpy(packet + index + 3, "Gfire", strlen("Gfire"));
	index += 3 + strlen("Gfire");

	index = gfire_add_att_name(packet,index, "theme");
	packet[index] = 0x01;
	*((guint16*)&packet[index+1]) = GUINT16_TO_LE(strlen(GFIRE_VERSION));
	memcpy(packet + index + 3, GFIRE_VERSION, strlen(GFIRE_VERSION));
	index += 3 + strlen(GFIRE_VERSION);

	index = gfire_add_att_name(packet,index, "partner");
	packet[index] = 0x01;
	*((guint16*)&packet[index+1]) = 0;
	index += 3;

	return index;
}

int gfire_client_version(guint8 *packet, guint32 version)
{
	gfire_add_header(packet, 18, 0x03, 1);/*add header*/
	int index = 5;

	index = gfire_add_att_name(packet,index, "version");
	packet[index] = 0x02;
	*((guint32*)&packet[index+1]) = GUINT32_TO_LE(version);

	return index + 5;
}

void gfire_input_cb(gpointer p_data, gint p_source, PurpleInputCondition p_condition)
{
	guint16 packet_len = 0;
	static int tmp = 0;
	guint16 pkt_id = 0;
	PurpleConnection *gc = (PurpleConnection*)p_data;
	gfire_data *gfire = (gfire_data*)gc->proto_data;

	if ( (NULL == gfire->buff_out) || (NULL == gfire->buff_in) ) {
		if ( NULL == gfire->buff_out ) gfire->buff_out = g_malloc0(GFIRE_BUFFOUT_SIZE);
		if ( NULL == gfire->buff_in ) gfire->buff_in = g_malloc0(GFIRE_BUFFIN_SIZE);
	}

	if(p_condition != PURPLE_INPUT_READ)
		return;

	if(gfire->bytes_read < 2)
	{
		// Read the first 2 bytes (packet len)
		tmp = recv(p_source, (void*)gfire->buff_in, 2, 0);
		// Check for errors
		if(tmp <= 0)
		{
			// Connection closed when we receive a 0 byte packet
			if(tmp == 0)
			{
				purple_debug(PURPLE_DEBUG_MISC, "gfire", "(input): read 0 bytes, connection closed by peer\n");
				purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Connection closed by peer.");
			}
			// We couldn't read now; not necessarily an error
			else if(errno == EAGAIN)
				return;
			else
			{
				purple_debug(PURPLE_DEBUG_ERROR, "gfire", "Reading from socket failed errno = %d err_str = %s.\n",
						errno, strerror(errno));
				purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Socket read failure.");
			}
			gfire->bytes_read = 0;
			return;
		}

		gfire->bytes_read += tmp;

		if(gfire->bytes_read < 2)
			return;
	}

	// Get packet len
	memcpy(&packet_len, gfire->buff_in, sizeof(packet_len));
	packet_len = GUINT16_FROM_LE(packet_len);

	// Read the rest of the packet
	tmp = recv(p_source, (void*)gfire->buff_in + gfire->bytes_read, packet_len - gfire->bytes_read, 0);
	// Check for errors
	if(tmp <= 0)
	{
		// Connection closed when we receive a 0 byte packet
		if(tmp == 0)
		{
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "(input): read 0 bytes, connection closed by peer\n");
			purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Connection closed by peer.");
		}
		// We couldn't read now; not necessarily an error
		else if(errno == EAGAIN)
			return;
		else
		{
			purple_debug(PURPLE_DEBUG_ERROR, "gfire", "Reading from socket failed errno = %d err_str = %s.\n",
				errno, strerror(errno));
			purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Socket read failure.");
		}
		gfire->bytes_read = 0;
		return;
	}

	gfire->bytes_read += tmp;

	// We still don't have all data, wait until our next call
	if(gfire->bytes_read != packet_len)
		return;

	// We have the whole xfire packet, process it
	//		Get packet id
	memcpy(&pkt_id, gfire->buff_in + 2, sizeof(pkt_id));
	pkt_id = GUINT16_FROM_LE(pkt_id);

	GTimeVal gtv;
	g_get_current_time(&gtv);
	gfire->last_packet = gtv.tv_sec;

	gfire->bytes_read = 0;
	gfire_parse_packet(gc, packet_len, pkt_id);
}

void gfire_parse_packet(PurpleConnection *gc, int packet_len, int packet_id)
{
	int ob_len = 0;
	gfire_data *gfire = (gfire_data *) gc->proto_data;
	guint32 newver = 0;
	char tmp[100] = "";
	PurpleAccount *account = NULL;
	GList *tlist = NULL;
	gfire_buddy *buddy = NULL;
	guint8 *cid = NULL;
	gchar *ctopic = NULL;
	gchar *cmotd = NULL;
	gfire_chat_msg *gcm = NULL;
	PurpleBuddy *pbuddy = NULL;


	switch(packet_id)
	{
		case 128:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received salt packet\n");
			ob_len = gfire_send_auth(gc,packet_len, packet_id);
			gfire_send(gc, gfire->buff_out, ob_len);
			purple_connection_update_progress(gc, "Login sent", 2, XFIRE_CONNECT_STEPS);
		break;

		case 129:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received: wrong passwd/username\n");
			purple_connection_error(gc, "Password or Username Incorrect.");
		break;

		case 130:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "Log in was successful\n");
			purple_connection_set_state(gc, PURPLE_CONNECTED);
			gfire_packet_130(gc, packet_len);

			ob_len = gfire_statistics(gfire->buff_out);
			gfire_send(gc, gfire->buff_out, ob_len);

			if (gfire->alias) purple_connection_set_display_name(gc, g_strdup(gfire->alias));
		break;

		case 131:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "got buddylist: names and nicks\n");
			gfire_packet_131(gc, packet_len); /* buddy list from server */
			gfire_new_buddies(gc);
		break;

		case 132:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "got buddylist: user is online\n");
			tlist = gfire_read_buddy_online(gc, packet_len);
			if (NULL != tlist) gfire_update_buddy_status(gc, tlist, GFIRE_STATUS_ONLINE);
		break;

		case 133:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "got IM (or ack Packet)\n");
			ob_len = gfire_get_im(gc, packet_len);
			if (NULL != gfire->im) {
				/* proccess incomming im */
				gfire_handle_im(gc);
			}
			if(ob_len != 0){
				gfire_send(gc, gfire->buff_out, ob_len);
			}
		break;

		case 134:
			/* out of date version .. */
			/* autoset NEW VERSION :) */
			memcpy(&newver, gfire->buff_in + 17, sizeof(newver));
			newver = GUINT32_FROM_LE(newver);
			g_sprintf(tmp, "Protocol version mismatch, needs to be %d. Auto set to new value.", newver);
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "login ok, but version too old, needs to be = %d\n", newver);
			account = purple_connection_get_account(gc);
			purple_account_set_int(account, "version", newver);
			purple_connection_error(gc, tmp);

		break;

		case 135:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "got buddylist:game that a buddy is playing\n");
			tlist = gfire_game_status(gc, packet_len);
			if (NULL != tlist) gfire_update_buddy_status(gc, tlist, GFIRE_STATUS_GAME);
		break;

		case 136:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "got list of friends of friends\n");
		break;

		case 137:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "invitation result\n");
		break;

		case 138:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "got buddy invitation\n");
			tlist = gfire_read_invitation(gc, packet_len);
			if (NULL != tlist) gfire_process_invitation(gc, tlist);
		break;

		case 139:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "Remove buddy ack received\n");
			if (packet_len < 14) return;
			tlist = gfire_find_buddy_in_list(gfire->buddies,(gpointer *)(gfire->buff_in + 13), GFFB_UIDBIN);
			if (tlist == NULL) {
				purple_debug(PURPLE_DEBUG_MISC, "gfire", "Remove buddy requested, buddy NOT FOUND.\n");
				return;
			}
			buddy = (gfire_buddy *)tlist->data;
			account = purple_connection_get_account(gc);
			pbuddy = purple_find_buddy(account, buddy->name);
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "Removing buddy %s, and freeing memory\n", NN(buddy->name));
			purple_blist_remove_buddy(pbuddy);
			gfire->buddies = g_list_delete_link(gfire->buddies, tlist);
			if (buddy->away_msg) g_free(buddy->away_msg);
			if (buddy->name) g_free(buddy->name);
			if (buddy->alias) g_free(buddy->alias);
			if (buddy->userid) g_free(buddy->userid);
			if (buddy->uid_str) g_free(buddy->uid_str);
			if (buddy->sid) g_free(buddy->sid);
			if (buddy->sid_str) g_free(buddy->sid_str);
			if (buddy->gameip) g_free(buddy->gameip);
			g_free(buddy);
		break;

		case 144:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received keep alive response (PONG)\n");
		break;

		case 145:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "ERROR: You have signed on from another location.\n");
			gc->wants_to_die = TRUE;
			purple_connection_error(gc, "You have signed on from another location.");
		break;

		case 154:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received away status packet.\n");
			tlist = gfire_read_buddy_status(gc, packet_len);
			if (NULL != tlist) gfire_update_buddy_status(gc, tlist, GFIRE_STATUS_AWAY);
		break;

		case 161:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received buddy nick change packet\n");
			gfire_read_alias_change(gc, packet_len);
		break;

		case 174:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received avatar info packet\n");
		break;

		case 351:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received group chat info\n");
		case 353:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received group chat, user join message\n");
			gcm = gfire_read_chat_user_join(gc, packet_len);
			if (NULL != gcm) gfire_chat_user_join(gc, gcm);
		break;

		case 354:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received group chat, user leave message\n");
			gcm = gfire_read_chat_user_leave(gc, packet_len);
			if (NULL != gcm) gfire_chat_user_leave(gc, gcm);
		break;

		case 355:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "got group chat message\n");
			gcm = gfire_read_chat_msg(gc, packet_len);
			if (NULL != gcm) {
				gfire_chat_got_msg(gc, gcm);
			} else {
				purple_debug(PURPLE_DEBUG_ERROR,"gfire", "(group chat): message parsed failed, gcm NULL\n");
			}
		break;

		case 356:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received group chat invite\n");
			gfire_read_chat_invite(gc, packet_len);
		break;

		case 357:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "groupchat buddy permission changed\n");
			read_groupchat_buddy_permission_change(gc, packet_len);
		break;

		case 368:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received group chat channel info, member list\n");
			tlist = gfire_read_chat_info(gc, packet_len, &ctopic, &cmotd, &cid);
			if (NULL != tlist) gfire_chat_joined(gc, tlist, cid, ctopic, cmotd);
		break;

		case 374:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "groupchat motd changed\n");
			gfire_read_chat_motd_change(gc, packet_len);
		break;

		case 387:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received chat reject confirmation\n");
		break;

		default:
			purple_debug(PURPLE_DEBUG_MISC, "gfire", "received: length=%i command=%i\n", packet_len, packet_id);
	}
}


void gfire_send_away(PurpleConnection *gc, const char *msg)
{
	int index = 5;
	gfire_data *gfire = NULL;
	guint16 slen = 0;


	if (msg == NULL) msg = "";
	slen = (guint16) strlen(msg);
	slen = GUINT16_TO_LE(slen);

	if (!gc || !(gfire = (gfire_data *)gc->proto_data) ||
		!gfire->buff_out || !(gfire->fd > 0)) return;

	gfire->buff_out[index++] = 0x2e;
	gfire->buff_out[index++] = 0x01;
	memcpy(gfire->buff_out + index, &slen, sizeof(slen));
	index += sizeof(slen);

	memcpy(gfire->buff_out + index, msg, strlen(msg));
	index += strlen(msg);

	purple_debug(PURPLE_DEBUG_MISC, "gfire", "(away): sending away message \"%s\"\n", NN(msg));
	gfire_add_header(gfire->buff_out, index, 32, 1);
	gfire_send(gc, gfire->buff_out, index);

}



void gfire_process_invitation(PurpleConnection *gc, GList *invites)
{
	GList *i = NULL;
	gfire_buddy *buddy = NULL;
	PurpleAccount *account = purple_connection_get_account(gc);

	if (!gc || !invites) {
		if (invites) {
			invites = g_list_first(invites);
			i = invites;
			while (i != NULL) {
				buddy = (gfire_buddy *)i->data;
				if (!buddy) { i = g_list_next(i); continue; }
				g_free(buddy->name);
				g_free(buddy->uid_str);
				g_free(buddy->alias);
				g_free(buddy);
				i = g_list_next(i);
			}
			g_list_free(i);
		}
		return;
	}

	invites = g_list_first(invites);
	i = invites;

	while (NULL != i){
		buddy = (gfire_buddy *)i->data;
		if (!buddy) { i = g_list_next(i); continue; }
		/* this is a hack, but we need to access gc in the callback */
		buddy->sid = (guint8 *)gc;

		purple_account_request_authorization(account, buddy->name, NULL, buddy->alias, buddy->uid_str,
		TRUE, gfire_buddy_add_authorize_cb,
		gfire_buddy_add_deny_cb, (void *)buddy);
		i = g_list_next(i);
	}
}
