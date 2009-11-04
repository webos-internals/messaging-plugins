/**
 * @file sipe-conf.c
 *
 * pidgin-sipe
 *
 * Copyright (C) 2009 pier11 <pier11@operamail.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <glib.h>

#include "debug.h"

#include "sipe.h"
#include "sipe-conf.h"
#include "sipe-dialog.h"
#include "sipe-nls.h"
#include "sipe-session.h"
#include "sipe-utils.h"

/**
 * Add Conference request to FocusFactory.
 * @param focus_factory_uri (%s) Ex.: sip:bob7@boston.local;gruu;opaque=app:conf:focusfactory
 * @param from		    (%s) Ex.: sip:bob7@boston.local
 * @param request_id	    (%d) Ex.: 1094520
 * @param conference_id	    (%s) Ex.: 8386E6AEAAA41E4AA6627BA76D43B6D1
 * @param expiry_time	    (%s) Ex.: 2009-07-13T17:57:09Z , Default duration: 7 hours
 */
#define SIPE_SEND_CONF_ADD \
"<?xml version=\"1.0\"?>"\
"<request xmlns=\"urn:ietf:params:xml:ns:cccp\" "\
	"xmlns:mscp=\"http://schemas.microsoft.com/rtc/2005/08/cccpextensions\" "\
	"C3PVersion=\"1\" "\
	"to=\"%s\" "\
	"from=\"%s\" "\
	"requestId=\"%d\">"\
	"<addConference>"\
		"<ci:conference-info xmlns:ci=\"urn:ietf:params:xml:ns:conference-info\" entity=\"\" xmlns:msci=\"http://schemas.microsoft.com/rtc/2005/08/confinfoextensions\">"\
			"<ci:conference-description>"\
				"<ci:subject/>"\
				"<msci:conference-id>%s</msci:conference-id>"\
				"<msci:expiry-time>%s</msci:expiry-time>"\
				"<msci:admission-policy>openAuthenticated</msci:admission-policy>"\
			"</ci:conference-description>"\
			"<msci:conference-view>"\
				"<msci:entity-view entity=\"chat\"/>"\
			"</msci:conference-view>"\
		"</ci:conference-info>"\
	"</addConference>"\
"</request>"

/**
 * AddUser request to Focus.
 * Params:
 * focus_URI, from, request_id, focus_URI, from, endpoint_GUID
 */
#define SIPE_SEND_CONF_ADD_USER \
"<?xml version=\"1.0\"?>"\
"<request xmlns=\"urn:ietf:params:xml:ns:cccp\" xmlns:mscp=\"http://schemas.microsoft.com/rtc/2005/08/cccpextensions\" "\
	"C3PVersion=\"1\" "\
	"to=\"%s\" "\
	"from=\"%s\" "\
	"requestId=\"%d\">"\
	"<addUser>"\
		"<conferenceKeys confEntity=\"%s\"/>"\
		"<ci:user xmlns:ci=\"urn:ietf:params:xml:ns:conference-info\" entity=\"%s\">"\
			"<ci:roles>"\
				"<ci:entry>attendee</ci:entry>"\
			"</ci:roles>"\
			"<ci:endpoint entity=\"{%s}\" xmlns:msci=\"http://schemas.microsoft.com/rtc/2005/08/confinfoextensions\"/>"\
		"</ci:user>"\
	"</addUser>"\
"</request>"

/**
 * ModifyUserRoles request to Focus. Makes user a leader.
 * @param focus_uri (%s)
 * @param from (%s)
 * @param request_id (%d)
 * @param focus_uri (%s)
 * @param who (%s)
 */
#define SIPE_SEND_CONF_MODIFY_USER_ROLES \
"<?xml version=\"1.0\"?>"\
"<request xmlns=\"urn:ietf:params:xml:ns:cccp\" xmlns:mscp=\"http://schemas.microsoft.com/rtc/2005/08/cccpextensions\" "\
	"C3PVersion=\"1\" "\
	"to=\"%s\" "\
	"from=\"%s\" "\
	"requestId=\"%d\">"\
	"<modifyUserRoles>"\
		"<userKeys confEntity=\"%s\" userEntity=\"%s\"/>"\
		"<user-roles xmlns=\"urn:ietf:params:xml:ns:conference-info\">"\
			"<entry>presenter</entry>"\
		"</user-roles>"\
	"</modifyUserRoles>"\
"</request>"

/**
 * ModifyConferenceLock request to Focus. Locks/unlocks conference.
 * @param focus_uri (%s)
 * @param from (%s)
 * @param request_id (%d)
 * @param focus_uri (%s)
 * @param locked (%s) "true" or "false" values applicable
 */
#define SIPE_SEND_CONF_MODIFY_CONF_LOCK \
"<?xml version=\"1.0\"?>"\
"<request xmlns=\"urn:ietf:params:xml:ns:cccp\" xmlns:mscp=\"http://schemas.microsoft.com/rtc/2005/08/cccpextensions\" "\
	"C3PVersion=\"1\" "\
	"to=\"%s\" "\
	"from=\"%s\" "\
	"requestId=\"%d\">"\
	"<modifyConferenceLock>"\
		"<conferenceKeys confEntity=\"%s\"/>"\
		"<locked>%s</locked>"\
	"</modifyConferenceLock>"\
"</request>"

/**
 * ModifyConferenceLock request to Focus. Locks/unlocks conference.
 * @param focus_uri (%s)
 * @param from (%s)
 * @param request_id (%d)
 * @param focus_uri (%s)
 * @param who (%s)
 */
#define SIPE_SEND_CONF_DELETE_USER \
"<?xml version=\"1.0\"?>"\
"<request xmlns=\"urn:ietf:params:xml:ns:cccp\" xmlns:mscp=\"http://schemas.microsoft.com/rtc/2005/08/cccpextensions\" "\
	"C3PVersion=\"1\" "\
	"to=\"%s\" "\
	"from=\"%s\" "\
	"requestId=\"%d\">"\
	"<deleteUser>"\
		"<userKeys confEntity=\"%s\" userEntity=\"%s\"/>"\
	"</deleteUser>"\
"</request>"

/**
 * Invite counterparty to join conference.
 * @param focus_uri (%s)
 * @param subject (%s) of conference
 */
#define SIPE_SEND_CONF_INVITE \
"<Conferencing version=\"2.0\">"\
	"<focus-uri>%s</focus-uri>"\
	"<subject>%s</subject>"\
	"<im available=\"true\">"\
		"<first-im/>"\
	"</im>"\
"</Conferencing>"

/**
 * Generates random GUID.
 * This method is borrowed from pidgin's msnutils.c
 */
static char *
rand_guid()
{
	return g_strdup_printf("%4X%4X-%4X-%4X-%4X-%4X%4X%4X",
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111);
}

/**
 * @param expires not respected if set to negative value (E.g. -1)
 */
static void
sipe_subscribe_conference(struct sipe_account_data *sip,
			  struct sip_session *session,
			  const int expires)
{
	gchar *expires_hdr = (expires >= 0) ? g_strdup_printf("Expires: %d\r\n", expires) : g_strdup("");
	gchar *contact = get_contact(sip);
	gchar *hdr = g_strdup_printf(
		"Event: conference\r\n"
		"%s"
		"Accept: application/conference-info+xml\r\n"
		"Supported: com.microsoft.autoextend\r\n"
		"Supported: ms-benotify\r\n"
		"Proxy-Require: ms-benotify\r\n"
		"Supported: ms-piggyback-first-notify\r\n"
		"Contact: %s\r\n",
		expires_hdr,
		contact);
	g_free(expires_hdr);
	g_free(contact);

	send_sip_request(sip->gc,
			 "SUBSCRIBE",
			 session->focus_uri,
			 session->focus_uri,
			 hdr,
			 "",
			 NULL,
			 process_subscribe_response);
	g_free(hdr);
}

/** Invite us to the focus callback */
static gboolean
process_invite_conf_focus_response(struct sipe_account_data *sip,
				   struct sipmsg *msg,
				   SIPE_UNUSED_PARAMETER struct transaction *trans)
{
	struct sip_session *session = NULL;
	char *focus_uri = parse_from(sipmsg_find_header(msg, "To"));

	session = sipe_session_find_conference(sip, focus_uri);

	if (!session) {
		purple_debug_info("sipe", "process_invite_conf_focus_response: unable to find conf session with focus=%s\n", focus_uri);
		g_free(focus_uri);
		return FALSE;
	}

	if (!session->focus_dialog) {
		purple_debug_info("sipe", "process_invite_conf_focus_response: session's focus_dialog is NULL\n");
		g_free(focus_uri);
		return FALSE;
	}

	sipe_dialog_parse(session->focus_dialog, msg, TRUE);

	if (msg->response >= 200) {
		/* send ACK to focus */
		session->focus_dialog->cseq = 0;
		send_sip_request(sip->gc, "ACK", session->focus_dialog->with, session->focus_dialog->with, NULL, NULL, session->focus_dialog, NULL);
		session->focus_dialog->outgoing_invite = NULL;
		session->focus_dialog->is_established = TRUE;
	}

	if (msg->response >= 400) {
		purple_debug_info("sipe", "process_invite_conf_focus_response: INVITE response is not 200. Failed to join focus.\n");
		/* @TODO notify user of failure to join focus */
		sipe_session_remove(sip, session);
		g_free(focus_uri);
		return FALSE;
	} else if (msg->response == 200) {
		xmlnode *xn_response = xmlnode_from_str(msg->body, msg->bodylen);
		const gchar *code = xmlnode_get_attrib(xn_response, "code");
		if (!strcmp(code, "success")) {
			/* subscribe to focus */
			sipe_subscribe_conference(sip, session, -1);
		}
		xmlnode_free(xn_response);
	}

	g_free(focus_uri);
	return TRUE;
}

/** Invite us to the focus */
static void
sipe_invite_conf_focus(struct sipe_account_data *sip,
		       struct sip_session *session)
{
	gchar *hdr;
	gchar *contact;
	gchar *body;
	gchar *self;

	if (session->focus_dialog && session->focus_dialog->is_established) {
		purple_debug_info("sipe", "session with %s already has a dialog open\n", session->focus_uri);
		return;
	}

	if(!session->focus_dialog) {
		session->focus_dialog = g_new0(struct sip_dialog, 1);
		session->focus_dialog->callid = gencallid();
		session->focus_dialog->with = g_strdup(session->focus_uri);
		session->focus_dialog->endpoint_GUID = rand_guid();
	}
	if (!(session->focus_dialog->ourtag)) {
		session->focus_dialog->ourtag = gentag();
	}

	contact = get_contact(sip);
	hdr = g_strdup_printf(
		"Supported: ms-sender\r\n"
		"Contact: %s\r\n"
		"Content-Type: application/cccp+xml\r\n",
		contact);
	g_free(contact);

	/* @TODO put request_id to queue to further compare with incoming one */
	/* focus_URI, from, request_id, focus_URI, from, endpoint_GUID */
	self = sip_uri_self(sip);
	body = g_strdup_printf(
		SIPE_SEND_CONF_ADD_USER,
		session->focus_dialog->with,
		self,
		session->request_id++,
		session->focus_dialog->with,
		self,
		session->focus_dialog->endpoint_GUID);
	g_free(self);

	session->focus_dialog->outgoing_invite = send_sip_request(sip->gc,
								  "INVITE",
								  session->focus_dialog->with,
								  session->focus_dialog->with,
								  hdr,
								  body,
								  session->focus_dialog,
								  process_invite_conf_focus_response);
	g_free(body);
	g_free(hdr);
}

/** Modify User Role */
void
sipe_conf_modify_user_role(struct sipe_account_data *sip,
			   struct sip_session *session,
			   const gchar* who)
{
	gchar *hdr;
	gchar *body;
	gchar *self;

	if (!session->focus_dialog || !session->focus_dialog->is_established) {
		purple_debug_info("sipe", "sipe_conf_modify_user_role: no dialog with focus, exiting.\n");
		return;
	}

	hdr = g_strdup(
		"Content-Type: application/cccp+xml\r\n");

	/* @TODO put request_id to queue to further compare with incoming one */
	self = sip_uri_self(sip);
	body = g_strdup_printf(
		SIPE_SEND_CONF_MODIFY_USER_ROLES,
		session->focus_dialog->with,
		self,
		session->request_id++,
		session->focus_dialog->with,
		who);
	g_free(self);

	send_sip_request(sip->gc,
			 "INFO",
			 session->focus_dialog->with,
			 session->focus_dialog->with,
			 hdr,
			 body,
			 session->focus_dialog,
			 NULL);
	g_free(body);
	g_free(hdr);
}

/** Modify Conference Lock */
void
sipe_conf_modify_conference_lock(struct sipe_account_data *sip,
				 struct sip_session *session,
				 const gboolean locked)
{
	gchar *hdr;
	gchar *body;
	gchar *self;

	if (!session->focus_dialog || !session->focus_dialog->is_established) {
		purple_debug_info("sipe", "sipe_conf_modify_conference_lock: no dialog with focus, exiting.\n");
		return;
	}

	hdr = g_strdup(
		"Content-Type: application/cccp+xml\r\n");

	/* @TODO put request_id to queue to further compare with incoming one */
	self = sip_uri_self(sip);
	body = g_strdup_printf(
		SIPE_SEND_CONF_MODIFY_CONF_LOCK,
		session->focus_dialog->with,
		self,
		session->request_id++,
		session->focus_dialog->with,
		locked ? "true" : "false");
	g_free(self);

	send_sip_request(sip->gc,
			 "INFO",
			 session->focus_dialog->with,
			 session->focus_dialog->with,
			 hdr,
			 body,
			 session->focus_dialog,
			 NULL);
	g_free(body);
	g_free(hdr);
}

/** Modify Delete User */
void
sipe_conf_delete_user(struct sipe_account_data *sip,
		      struct sip_session *session,
		      const gchar* who)
{
	gchar *hdr;
	gchar *body;
	gchar *self;

	if (!session->focus_dialog || !session->focus_dialog->is_established) {
		purple_debug_info("sipe", "sipe_conf_delete_user: no dialog with focus, exiting.\n");
		return;
	}

	hdr = g_strdup(
		"Content-Type: application/cccp+xml\r\n");

	/* @TODO put request_id to queue to further compare with incoming one */
	self = sip_uri_self(sip);
	body = g_strdup_printf(
		SIPE_SEND_CONF_DELETE_USER,
		session->focus_dialog->with,
		self,
		session->request_id++,
		session->focus_dialog->with,
		who);
	g_free(self);

	send_sip_request(sip->gc,
			 "INFO",
			 session->focus_dialog->with,
			 session->focus_dialog->with,
			 hdr,
			 body,
			 session->focus_dialog,
			 NULL);
	g_free(body);
	g_free(hdr);
}

/** Invite counterparty to join conference callback */
static gboolean
process_invite_conf_response(struct sipe_account_data *sip,
			     struct sipmsg *msg,
			     SIPE_UNUSED_PARAMETER struct transaction *trans)
{
	struct sip_dialog *dialog = g_new0(struct sip_dialog, 1);

	dialog->callid = g_strdup(sipmsg_find_header(msg, "Call-ID"));
	dialog->cseq = parse_cseq(sipmsg_find_header(msg, "CSeq"));
	dialog->with = parse_from(sipmsg_find_header(msg, "To"));
	sipe_dialog_parse(dialog, msg, TRUE);

	if (msg->response >= 200) {
		/* send ACK to counterparty */
		dialog->cseq--;
		send_sip_request(sip->gc, "ACK", dialog->with, dialog->with, NULL, NULL, dialog, NULL);
		dialog->outgoing_invite = NULL;
		dialog->is_established = TRUE;
	}

	if (msg->response >= 400) {
		purple_debug_info("sipe", "process_invite_conf_response: INVITE response is not 200. Failed to invite %s.\n", dialog->with);
		/* @TODO notify user of failure to invite counterparty */
		sipe_dialog_free(dialog);
		return FALSE;
	}
	if (msg->response >= 200) {
		/* send BYE to counterparty */
		send_sip_request(sip->gc, "BYE", dialog->with, dialog->with, NULL, NULL, dialog, NULL);
	}

	sipe_dialog_free(dialog);
	return TRUE;
}

/**
 * Invites counterparty to join conference.
 */
void
sipe_invite_conf(struct sipe_account_data *sip,
		 struct sip_session *session,
		 const gchar* who)
{
	gchar *hdr;
	gchar *contact;
	gchar *body;
	struct sip_dialog *dialog = NULL;

	/* It will be short lived special dialog.
	 * Will not be stored in session.
	 */
	dialog = g_new0(struct sip_dialog, 1);
	dialog->callid = gencallid();
	dialog->with = g_strdup(who);
	dialog->ourtag = gentag();

	contact = get_contact(sip);
	hdr = g_strdup_printf(
		"Supported: ms-sender\r\n"
		"Contact: %s\r\n"
		"Content-Type: application/ms-conf-invite+xml\r\n",
		contact);
	g_free(contact);

	body = g_strdup_printf(
		SIPE_SEND_CONF_INVITE,
		session->focus_uri,
		session->subject ? session->subject : ""
		);

	send_sip_request( sip->gc,
			  "INVITE",
			  dialog->with,
			  dialog->with,
			  hdr,
			  body,
			  dialog,
			  process_invite_conf_response);

	sipe_dialog_free(dialog);
	g_free(body);
	g_free(hdr);
}

/** Create conference callback */
static gboolean
process_conf_add_response(struct sipe_account_data *sip,
			  struct sipmsg *msg,
			  struct transaction *trans)
{
	if (msg->response >= 400) {
		purple_debug_info("sipe", "process_conf_add_response: SERVICE response is not 200. Failed to create conference.\n");
		/* @TODO notify user of failure to create conference */
		return FALSE;
	}
	if (msg->response == 200) {
		xmlnode *xn_response = xmlnode_from_str(msg->body, msg->bodylen);
		if (!strcmp("success", xmlnode_get_attrib(xn_response, "code")))
		{
			gchar *who = (gchar *)trans->payload;
			struct sip_session *session;
			xmlnode *xn_conference_info = xmlnode_get_descendant(xn_response, "addConference", "conference-info", NULL);

			session = sipe_session_add_chat(sip);
			session->is_multiparty = FALSE;
			session->focus_uri = g_strdup(xmlnode_get_attrib(xn_conference_info, "entity"));
			purple_debug_info("sipe", "process_conf_add_response: session->focus_uri=%s\n",
						   session->focus_uri ? session->focus_uri : "");

			session->pending_invite_queue = slist_insert_unique_sorted(
				session->pending_invite_queue, g_strdup(who), (GCompareFunc)strcmp);

			/* add self to conf */
			sipe_invite_conf_focus(sip, session);
		}
		xmlnode_free(xn_response);
	}

	return TRUE;
}

/**
 * Creates conference.
 */
void
sipe_conf_add(struct sipe_account_data *sip,
	      const gchar* who)
{
	gchar *hdr;
	gchar *conference_id;
	gchar *contact;
	gchar *body;
	gchar *self;
	struct transaction * tr;
	struct sip_dialog *dialog = NULL;
	time_t expiry = time(NULL) + 7*60*60; /* 7 hours */
	const char *expiry_time;

	contact = get_contact(sip);
	hdr = g_strdup_printf(
		"Supported: ms-sender\r\n"
		"Contact: %s\r\n"
		"Content-Type: application/cccp+xml\r\n",
		contact);
	g_free(contact);

	expiry_time = purple_utf8_strftime("%Y-%m-%dT%H:%M:%SZ", gmtime(&expiry));
	self = sip_uri_self(sip);
	conference_id = genconfid();
	body = g_strdup_printf(
		SIPE_SEND_CONF_ADD,
		sip->focus_factory_uri,
		self,
		rand(),
		conference_id,
		expiry_time);
	g_free(conference_id);
	g_free(self);

	tr = send_sip_request( sip->gc,
			  "SERVICE",
			  sip->focus_factory_uri,
			  sip->focus_factory_uri,
			  hdr,
			  body,
			  NULL,
			  process_conf_add_response);
	tr->payload = g_strdup(who);

	sipe_dialog_free(dialog);
	g_free(body);
	g_free(hdr);
}

void
process_incoming_invite_conf(struct sipe_account_data *sip,
			     struct sipmsg *msg)
{
	struct sip_session *session = NULL;
	struct sip_dialog *dialog = NULL;
	xmlnode *xn_conferencing = xmlnode_from_str(msg->body, msg->bodylen);
	xmlnode *xn_focus_uri = xmlnode_get_child(xn_conferencing, "focus-uri");
	char *focus_uri = xmlnode_get_data(xn_focus_uri);

	xmlnode_free(xn_conferencing);

	/* send OK */
	purple_debug_info("sipe", "We have received invitation to Conference. Focus URI=%s\n", focus_uri);
	send_sip_response(sip->gc, msg, 200, "OK", NULL);

	session = sipe_session_add_chat(sip);
	session->focus_uri = focus_uri;
	session->is_multiparty = FALSE;

	/* temporary dialog with invitor */
	dialog = g_new0(struct sip_dialog, 1);
	dialog->callid = g_strdup(sipmsg_find_header(msg, "Call-ID"));
	dialog->cseq = parse_cseq(sipmsg_find_header(msg, "CSeq"));
	dialog->with = parse_from(sipmsg_find_header(msg, "From"));
	sipe_dialog_parse(dialog, msg, FALSE);

	/* send BYE to invitor */
	send_sip_request(sip->gc, "BYE", dialog->with, dialog->with, NULL, NULL, dialog, NULL);
	sipe_dialog_free(dialog);

	/* add self to conf */
	sipe_invite_conf_focus(sip, session);
}

void
sipe_process_conference(struct sipe_account_data *sip,
			struct sipmsg *msg)
{
	xmlnode *xn_conference_info;
	xmlnode *node;
	xmlnode *xn_subject;
	const gchar *focus_uri;
	struct sip_session *session;
	gboolean just_joined = FALSE;

	if (msg->response != 0 && msg->response != 200) return;

	if (msg->bodylen == 0 || msg->body == NULL || strcmp(sipmsg_find_header(msg, "Event"), "conference")) return;

	xn_conference_info = xmlnode_from_str(msg->body, msg->bodylen);
	if (!xn_conference_info) return;

	focus_uri = xmlnode_get_attrib(xn_conference_info, "entity");
	session = sipe_session_find_conference(sip, focus_uri);

	if (!session) {
		purple_debug_info("sipe", "sipe_process_conference: unable to find conf session with focus=%s\n", focus_uri);
		return;
	}

	if (session->focus_uri && !session->conv) {
		gchar *chat_name = g_strdup_printf(_("Chat #%d"), ++sip->chat_seq);
		gchar *self = sip_uri_self(sip);
		/* create prpl chat */
		session->conv = serv_got_joined_chat(sip->gc, session->chat_id, chat_name);
		session->chat_name = chat_name;
		purple_conv_chat_set_nick(PURPLE_CONV_CHAT(session->conv), self);
		just_joined = TRUE;
		/* @TODO ask for full state (re-subscribe) if it was a partial one -
		 * this is to obtain full list of conference participants.
		 */
		 g_free(self);
	}
	
	/* subject */
	if ((xn_subject = xmlnode_get_descendant(xn_conference_info, "conference-description", "subject", NULL))) {
		g_free(session->subject);
		session->subject = xmlnode_get_data(xn_subject);
		purple_conv_chat_set_topic(PURPLE_CONV_CHAT(session->conv), NULL, session->subject);
		purple_debug_info("sipe", "sipe_process_conference: subject=%s\n", session->subject ? session->subject : "");
	}

	/* IM MCU URI */
	if (!session->im_mcu_uri) {
		for (node = xmlnode_get_descendant(xn_conference_info, "conference-description", "conf-uris", "entry", NULL);
		     node;
		     node = xmlnode_get_next_twin(node))
		{
			gchar *purpose = xmlnode_get_data(xmlnode_get_child(node, "purpose"));

			if (purpose && !strcmp("chat", purpose)) {
				g_free(purpose);
				session->im_mcu_uri = xmlnode_get_data(xmlnode_get_child(node, "uri"));
				purple_debug_info("sipe", "sipe_process_conference: im_mcu_uri=%s\n", session->im_mcu_uri);
				break;
			}
			g_free(purpose);
		}
	}

	/* users */
	for (node = xmlnode_get_descendant(xn_conference_info, "users", "user", NULL); node; node = xmlnode_get_next_twin(node)) {
		xmlnode *endpoint = NULL;
		const gchar *user_uri = xmlnode_get_attrib(node, "entity");
		const gchar *state = xmlnode_get_attrib(node, "state");
		gchar *role  = xmlnode_get_data(xmlnode_get_descendant(node, "roles", "entry", NULL));
		PurpleConvChatBuddyFlags flags = PURPLE_CBFLAGS_NONE;
		PurpleConvChat *chat = PURPLE_CONV_CHAT(session->conv);
		gboolean is_in_im_mcu = FALSE;
		gchar *self = sip_uri_self(sip);
		
		if (role && !strcmp(role, "presenter")) {
			flags |= PURPLE_CBFLAGS_OP;
		}

		if (!strcmp("deleted", state)) {
			if (purple_conv_chat_find_user(chat, user_uri)) {
				purple_conv_chat_remove_user(chat, user_uri, NULL /* reason */);
			}
		} else {
			/* endpoints */
			for (endpoint = xmlnode_get_child(node, "endpoint"); endpoint; endpoint = xmlnode_get_next_twin(endpoint)) {
				if (!strcmp("chat", xmlnode_get_attrib(endpoint, "session-type"))) {
					gchar *status = xmlnode_get_data(xmlnode_get_child(endpoint, "status"));
					if (!strcmp("connected", status)) {
						is_in_im_mcu = TRUE;
						if (!purple_conv_chat_find_user(chat, user_uri)) {
							purple_conv_chat_add_user(chat, user_uri, NULL, flags,
										  !just_joined && g_strcasecmp(user_uri, self));
						} else {
							purple_conv_chat_user_set_flags(chat, user_uri, flags);
						}
					}
					g_free(status);
					break;
				}
			}
			if (!is_in_im_mcu) {
				if (purple_conv_chat_find_user(chat, user_uri)) {
					purple_conv_chat_remove_user(chat, user_uri, NULL /* reason */);
				}
			}
		}
		g_free(role);
		g_free(self);
	}
	
	/* entity-view, locked */
	for (node = xmlnode_get_descendant(xn_conference_info, "conference-view", "entity-view", NULL);
	     node;
	     node = xmlnode_get_next_twin(node)) {
	
		xmlnode *xn_type = xmlnode_get_descendant(node, "entity-state", "media", "entry", "type", NULL);
		gchar *tmp;
		if (xn_type && !strcmp("chat", (tmp = xmlnode_get_data(xn_type)))) {
			xmlnode *xn_locked = xmlnode_get_descendant(node, "entity-state", "locked", NULL);
			if (xn_locked) {
				gchar *locked = xmlnode_get_data(xn_locked);
				gboolean prev_locked = session->locked;
				session->locked = (locked && !strcmp(locked, "true")) ? TRUE : FALSE;
				if (prev_locked && !session->locked) {
					sipe_present_info(sip, session, 
						_("This conference is no longer locked. Additional participants can now join."));
				}
				if (!prev_locked && session->locked) {
					sipe_present_info(sip, session, 
						_("This conference is locked. Nobody else can join the conference while it is locked."));
				}
				
				purple_debug_info("sipe", "sipe_process_conference: session->locked=%s\n",
						          session->locked ? "TRUE" : "FALSE");
				g_free(locked);
			}
			g_free(tmp);
		}
	}	
	xmlnode_free(xn_conference_info);

	if (session->im_mcu_uri) {
		struct sip_dialog *dialog = sipe_dialog_find(session, session->im_mcu_uri);
		if (!dialog) {
			dialog = sipe_dialog_add(session);

			dialog->callid = g_strdup(session->callid);
			dialog->with = g_strdup(session->im_mcu_uri);

			/* send INVITE to IM MCU */
			sipe_invite(sip, session, dialog->with, NULL, NULL, FALSE);
		}
	}

	sipe_process_pending_invite_queue(sip, session);
}

void
sipe_conf_immcu_closed(struct sipe_account_data *sip,
		       struct sip_session *session)
{
	sipe_present_info(sip, session, 
			  _("You have been disconnected from this conference."));
	purple_conv_chat_clear_users(PURPLE_CONV_CHAT(session->conv));
}

void
conf_session_close(struct sipe_account_data *sip,
		   struct sip_session *session)
{
	if (session) {
		/* unsubscribe from focus */
		sipe_subscribe_conference(sip, session, 0);

		if (session->focus_dialog) {
			/* send BYE to focus */
			send_sip_request(sip->gc,
					 "BYE",
					 session->focus_dialog->with,
					 session->focus_dialog->with,
					 NULL,
					 NULL,
					 session->focus_dialog,
					 NULL);
		}
	}
}

void
sipe_process_imdn(struct sipe_account_data *sip,
		  struct sipmsg *msg)
{
	gchar *with = parse_from(sipmsg_find_header(msg, "From"));
	gchar *call_id = sipmsg_find_header(msg, "Call-ID");
	static struct sip_session *session;
	xmlnode *xn_imdn;
	xmlnode *node;
	gchar *message_id;
	gchar *message;

	session = sipe_session_find_chat_by_callid(sip, call_id);
	if (!session) {
		session = sipe_session_find_im(sip, with);
	}
	if (!session) {
		purple_debug_info("sipe", "sipe_process_imdn: unable to find conf session with call_id=%s\n", call_id);
		g_free(with);
		return;
	}

	xn_imdn = xmlnode_from_str(msg->body, msg->bodylen);
	message_id = xmlnode_get_data(xmlnode_get_child(xn_imdn, "message-id"));

	message = g_hash_table_lookup(session->conf_unconfirmed_messages, message_id);

	/* recipient */
	for (node = xmlnode_get_child(xn_imdn, "recipient"); node; node = xmlnode_get_next_twin(node)) {
		gchar *tmp = parse_from(xmlnode_get_attrib(node, "uri"));
		gchar *uri = parse_from(tmp);
		sipe_present_message_undelivered_err(sip, session, uri, message);
		g_free(tmp);
		g_free(uri);
	}

	xmlnode_free(xn_imdn);

	g_hash_table_remove(session->conf_unconfirmed_messages, message_id);
	purple_debug_info("sipe", "sipe_process_imdn: removed message %s from conf_unconfirmed_messages(count=%d)\n",
			  message_id, g_hash_table_size(session->conf_unconfirmed_messages));
	g_free(message_id);
	g_free(with);
}


/*
  Local Variables:
  mode: c
  c-file-style: "bsd"
  indent-tabs-mode: t
  tab-width: 8
  End:
*/
