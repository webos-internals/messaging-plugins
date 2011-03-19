/**
 * @file sipe-media.c
 *
 * pidgin-sipe
 *
 * Copyright (C) 2010 Jakub Adam <jakub.adam@tieto.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <glib.h>

#include "sipe-common.h"
#include "sipmsg.h"
#include "sip-transport.h"
#include "sipe-backend.h"
#include "sdpmsg.h"
#include "sipe-core.h"
#include "sipe-core-private.h"
#include "sipe-dialog.h"
#include "sipe-media.h"
#include "sipe-session.h"
#include "sipe-utils.h"
#include "sipe-nls.h"

struct sipe_media_call_private {
	struct sipe_media_call public;

	/* private part starts here */
	struct sipe_core_private	*sipe_private;
	gchar				*with;

	struct sipmsg			*invitation;
	gboolean			 legacy_mode;
	gboolean			 using_nice;
	gboolean			 encryption_compatible;

	struct sdpmsg			*smsg;
};
#define SIPE_MEDIA_CALL         ((struct sipe_media_call *) call_private)
#define SIPE_MEDIA_CALL_PRIVATE ((struct sipe_media_call_private *) call)

static void sipe_media_codec_list_free(GList *codecs)
{
	for (; codecs; codecs = g_list_delete_link(codecs, codecs))
		sipe_backend_codec_free(codecs->data);
}

static void sipe_media_candidate_list_free(GList *candidates)
{
	for (; candidates; candidates = g_list_delete_link(candidates, candidates))
		sipe_backend_candidate_free(candidates->data);
}

static void
sipe_media_call_free(struct sipe_media_call_private *call_private)
{
	if (call_private) {
		struct sip_session *session;
		sipe_backend_media_free(call_private->public.backend_private);

		session = sipe_session_find_call(call_private->sipe_private,
						 call_private->with);
		if (session)
			sipe_session_remove(call_private->sipe_private, session);

		if (call_private->invitation)
			sipmsg_free(call_private->invitation);

		sdpmsg_free(call_private->smsg);
		g_free(call_private->with);
		g_free(call_private);
	}
}

static GSList *
backend_candidates_to_sdpcandidate(GList *candidates)
{
	GSList *result = NULL;
	GList *i;

	for (i = candidates; i; i = i->next) {
		struct sipe_backend_candidate *candidate = i->data;
		struct sdpcandidate *c = g_new(struct sdpcandidate, 1);

		c->foundation = sipe_backend_candidate_get_foundation(candidate);
		c->component = sipe_backend_candidate_get_component_type(candidate);
		c->type = sipe_backend_candidate_get_type(candidate);
		c->protocol = sipe_backend_candidate_get_protocol(candidate);
		c->ip = sipe_backend_candidate_get_ip(candidate);
		c->port = sipe_backend_candidate_get_port(candidate);
		c->base_ip = sipe_backend_candidate_get_base_ip(candidate);
		c->base_port = sipe_backend_candidate_get_base_port(candidate);
		c->priority = sipe_backend_candidate_get_priority(candidate);

		result = g_slist_append(result, c);
	}

	return result;
}

static struct sdpmedia *
backend_stream_to_sdpmedia(struct sipe_media_call_private *call_private,
			   struct sipe_backend_stream *backend_stream)
{
	struct sipe_backend_media *backend_media = call_private->public.backend_private;
	struct sdpmedia *media = g_new0(struct sdpmedia, 1);
	GList *codecs = sipe_backend_get_local_codecs(SIPE_MEDIA_CALL,
						      backend_stream);
	guint rtcp_port;
	SipeMediaType type;
	GSList *attributes = NULL;
	GList *candidates;
	GList *i;
	GSList *j;

	media->name = g_strdup(sipe_backend_stream_get_id(backend_stream));

	if (sipe_strequal(media->name, "audio"))
		type = SIPE_MEDIA_AUDIO;
	else if (sipe_strequal(media->name, "video"))
		type = SIPE_MEDIA_VIDEO;
	else {
		// TODO: incompatible media, should not happen here
	}

	// Process codecs
	for (i = codecs; i; i = i->next) {
		struct sipe_backend_codec *codec = i->data;
		struct sdpcodec *c = g_new0(struct sdpcodec, 1);
		GList *params;

		c->id = sipe_backend_codec_get_id(codec);
		c->name = sipe_backend_codec_get_name(codec);
		c->clock_rate = sipe_backend_codec_get_clock_rate(codec);
		c->type = type;

		params = sipe_backend_codec_get_optional_parameters(codec);
		for (; params; params = params->next) {
			struct sipnameval *param = params->data;
			struct sipnameval *copy = g_new0(struct sipnameval, 1);

			copy->name = g_strdup(param->name);
			copy->value = g_strdup(param->value);

			c->parameters = g_slist_append(c->parameters, copy);
		}

		media->codecs = g_slist_append(media->codecs, c);
	}

	sipe_media_codec_list_free(codecs);

	// Process local candidates
	// If we have established candidate pairs, send them in SDP response.
	// Otherwise send all available local candidates.
	candidates = sipe_backend_media_get_active_local_candidates(backend_media,
								    backend_stream);
	if (!candidates)
		candidates = sipe_backend_get_local_candidates(backend_media,
							       backend_stream);

	media->candidates = backend_candidates_to_sdpcandidate(candidates);

	// Process stream attributes
	if (!call_private->legacy_mode) {
		struct sipe_backend_candidate *candidate = candidates->data;

		gchar *username = sipe_backend_candidate_get_username(candidate);
		gchar *password = sipe_backend_candidate_get_password(candidate);

		attributes = sipe_utils_nameval_add(attributes,
						    "ice-ufrag", username);
		attributes = sipe_utils_nameval_add(attributes,
						    "ice-pwd", password);

		g_free(username);
		g_free(password);
	}

	sipe_media_candidate_list_free(candidates);

	for (j = media->candidates; j; j = j->next) {
		struct sdpcandidate *candidate = j->data;

		if (candidate->type == SIPE_CANDIDATE_TYPE_HOST) {
			if (candidate->component == SIPE_COMPONENT_RTP)
				media->port = candidate->port;
			else if (candidate->component == SIPE_COMPONENT_RTCP)
				rtcp_port = candidate->port;
		}
	}

	if (sipe_backend_stream_is_held(backend_stream))
		attributes = sipe_utils_nameval_add(attributes, "inactive", "");

	if (rtcp_port) {
		gchar *tmp = g_strdup_printf("%u", rtcp_port);
		attributes  = sipe_utils_nameval_add(attributes, "rtcp", tmp);
		g_free(tmp);
	}

	attributes = sipe_utils_nameval_add(attributes, "encryption", "rejected");

	media->attributes = attributes;

	// Process remote candidates
	candidates = sipe_backend_media_get_active_remote_candidates(backend_media,
								     backend_stream);
	media->remote_candidates = backend_candidates_to_sdpcandidate(candidates);
	sipe_media_candidate_list_free(candidates);

	return media;
}

static struct sdpmsg *
sipe_media_to_sdpmsg(struct sipe_media_call_private *call_private)
{
	struct sdpmsg *msg = g_new0(struct sdpmsg, 1);
	GSList *streams = sipe_backend_media_get_streams(call_private->public.backend_private);

	for (; streams; streams = streams->next) {
		struct sdpmedia *media;
		media = backend_stream_to_sdpmedia(call_private, streams->data);
		msg->media = g_slist_append(msg->media, media);
	}

	msg->legacy = call_private->legacy_mode;
	msg->ip = g_strdup(sipe_utils_get_suitable_local_ip(-1));

	return msg;
}

static void
sipe_invite_call(struct sipe_core_private *sipe_private, TransCallback tc)
{
	gchar *hdr;
	gchar *contact;
	gchar *body;
	struct sipe_media_call_private *call_private = sipe_private->media_call;
	struct sip_session *session;
	struct sip_dialog *dialog;
	struct sdpmsg *msg;

	session = sipe_session_find_call(sipe_private, call_private->with);
	dialog = session->dialogs->data;

	contact = get_contact(sipe_private);
	hdr = g_strdup_printf(
		"Supported: ms-early-media\r\n"
		"Supported: 100rel\r\n"
		"ms-keep-alive: UAC;hop-hop=yes\r\n"
		"Contact: %s\r\n"
		"Content-Type: application/sdp\r\n",
		contact);
	g_free(contact);

	msg = sipe_media_to_sdpmsg(call_private);
	body = sdpmsg_to_string(msg);
	sdpmsg_free(msg);

	dialog->outgoing_invite = sip_transport_invite(sipe_private,
						       hdr,
						       body,
						       dialog,
						       tc);

	g_free(body);
	g_free(hdr);
}

static struct sip_dialog *
sipe_media_dialog_init(struct sip_session* session, struct sipmsg *msg)
{
	gchar *newTag = gentag();
	const gchar *oldHeader;
	gchar *newHeader;
	struct sip_dialog *dialog;

	oldHeader = sipmsg_find_header(msg, "To");
	newHeader = g_strdup_printf("%s;tag=%s", oldHeader, newTag);
	sipmsg_remove_header_now(msg, "To");
	sipmsg_add_header_now(msg, "To", newHeader);
	g_free(newHeader);

	dialog = sipe_dialog_add(session);
	dialog->callid = g_strdup(sipmsg_find_header(msg, "Call-ID"));
	dialog->with = parse_from(sipmsg_find_header(msg, "From"));
	sipe_dialog_parse(dialog, msg, FALSE);

	return dialog;
}

static void
send_response_with_session_description(struct sipe_media_call_private *call_private, int code, gchar *text)
{
	struct sdpmsg *msg = sipe_media_to_sdpmsg(call_private);
	gchar *body = sdpmsg_to_string(msg);
	sdpmsg_free(msg);
	sipmsg_add_header(call_private->invitation, "Content-Type", "application/sdp");
	sip_transport_response(call_private->sipe_private, call_private->invitation, code, text, body);
	g_free(body);
}

static gboolean
encryption_levels_compatible(struct sdpmsg *msg)
{
	GSList *i;

	for (i = msg->media; i; i = i->next) {
		const gchar *enc_level;
		struct sdpmedia *m = i->data;

		enc_level = sipe_utils_nameval_find(m->attributes, "encryption");

		// Decline call if peer requires encryption as we don't support it yet.
		if (sipe_strequal(enc_level, "required"))
			return FALSE;
	}

	return TRUE;
}

static void
handle_incompatible_encryption_level(struct sipe_media_call_private *call_private)
{
	sipmsg_add_header(call_private->invitation, "Warning",
			  "308 lcs.microsoft.com \"Encryption Levels not compatible\"");
	sip_transport_response(call_private->sipe_private,
			       call_private->invitation,
			       488, "Encryption Levels not compatible",
			       NULL);
	sipe_backend_media_reject(call_private->public.backend_private, FALSE);
	sipe_backend_notify_error(_("Unable to establish a call"),
		_("Encryption settings of peer are incompatible with ours."));
}

static gboolean
process_invite_call_response(struct sipe_core_private *sipe_private,
								   struct sipmsg *msg,
								   struct transaction *trans);

static gboolean
update_remote_media(struct sipe_media_call_private* call_private,
		    struct sdpmedia *media)
{
	struct sipe_backend_media *backend_media = SIPE_MEDIA_CALL->backend_private;
	struct sipe_backend_stream *backend_stream;
	GList *backend_candidates = NULL;
	GList *backend_codecs = NULL;
	const gchar *username = sipe_utils_nameval_find(media->attributes, "ice-ufrag");
	const gchar *password = sipe_utils_nameval_find(media->attributes, "ice-pwd");
	GSList *i;
	gboolean result = TRUE;

	backend_stream = sipe_backend_media_get_stream_by_id(backend_media,
							     media->name);
	if (media->port == 0) {
		if (backend_stream)
			sipe_backend_media_remove_stream(backend_media, backend_stream);
		return TRUE;
	}

	if (!backend_stream)
		return FALSE;


	for (i = media->candidates; i; i = i->next) {
		struct sdpcandidate *c = i->data;
		struct sipe_backend_candidate *candidate;
		candidate = sipe_backend_candidate_new(c->foundation,
						       c->component,
						       c->type,
						       c->protocol,
						       c->ip,
						       c->port);
		sipe_backend_candidate_set_priority(candidate, c->priority);

		if (username)
			sipe_backend_candidate_set_username_and_pwd(candidate,
								    username,
								    password);

		backend_candidates = g_list_append(backend_candidates, candidate);
	}

	sipe_backend_media_add_remote_candidates(backend_media,
						 backend_stream,
						 backend_candidates);
	sipe_media_candidate_list_free(backend_candidates);

	for (i = media->codecs; i; i = i->next) {
		struct sdpcodec *c = i->data;
		struct sipe_backend_codec *codec;
		GSList *j;

		codec = sipe_backend_codec_new(c->id,
					       c->name,
					       c->type,
					       c->clock_rate);

		for (j = c->parameters; j; j = j->next) {
			struct sipnameval *attr = j->data;

			sipe_backend_codec_add_optional_parameter(codec,
								  attr->name,
								  attr->value);
		}

		backend_codecs = g_list_append(backend_codecs, codec);
	}

	result = sipe_backend_set_remote_codecs(backend_media,
						backend_stream,
						backend_codecs);
	sipe_media_codec_list_free(backend_codecs);

	if (sipe_utils_nameval_find(media->attributes, "inactive")) {
		sipe_backend_stream_hold(backend_media, backend_stream, FALSE);
	} else if (sipe_backend_stream_is_held(backend_stream)) {
		sipe_backend_stream_unhold(backend_media, backend_stream, FALSE);
	}

	return result;
}

static gboolean
apply_remote_message(struct sipe_media_call_private* call_private,
		     struct sdpmsg* msg)
{
	GSList *i;
	for (i = msg->media; i; i = i->next) {
		if (!update_remote_media(call_private, i->data))
			return FALSE;
	}

	call_private->legacy_mode = msg->legacy;
	call_private->encryption_compatible = encryption_levels_compatible(msg);

	return TRUE;
}

void do_apply_remote_message(struct sipe_media_call_private *call_private,
			     struct sdpmsg *smsg)
{
	if (!apply_remote_message(call_private, smsg)) {
		sip_transport_response(call_private->sipe_private,
				       call_private->invitation,
				       487, "Request Terminated", NULL);
		sipe_media_hangup(call_private->sipe_private);
		return;
	}

	sdpmsg_free(call_private->smsg);
	call_private->smsg = NULL;

	if (sipe_backend_media_accepted(call_private->public.backend_private)) {
		send_response_with_session_description(call_private,
						       200, "OK");
		return;
	}

	if (!call_private->legacy_mode && call_private->encryption_compatible)
		send_response_with_session_description(call_private,
						       183, "Session Progress");
}

static void candidates_prepared_cb(struct sipe_media_call *call,
				   struct sipe_backend_stream *stream)
{
	struct sipe_media_call_private *call_private = SIPE_MEDIA_CALL_PRIVATE;

	if (sipe_backend_media_is_initiator(call_private->public.backend_private,
					    stream)) {
		sipe_invite_call(call_private->sipe_private,
				 process_invite_call_response);
		return;
	} else {
		struct sdpmsg *smsg = call_private->smsg;
		call_private->smsg = NULL;

		do_apply_remote_message(call_private, smsg);
		sdpmsg_free(call_private->smsg);
	}
}

static void media_connected_cb(SIPE_UNUSED_PARAMETER struct sipe_media_call_private *call_private)
{
}

static void call_accept_cb(struct sipe_media_call *call, gboolean local)
{
	if (local) {
		struct sipe_media_call_private *call_private = SIPE_MEDIA_CALL_PRIVATE;

		if (!call_private->encryption_compatible) {
			handle_incompatible_encryption_level(call_private);
			return;
		}

		send_response_with_session_description(call_private, 200, "OK");
	}
}

static void call_reject_cb(struct sipe_media_call *call, gboolean local)
{
	struct sipe_media_call_private *call_private = SIPE_MEDIA_CALL_PRIVATE;

	if (local) {
		sip_transport_response(call_private->sipe_private, call_private->invitation, 603, "Decline", NULL);
	}
	call_private->sipe_private->media_call = NULL;
	sipe_media_call_free(call_private);
}

static gboolean
sipe_media_send_ack(struct sipe_core_private *sipe_private, struct sipmsg *msg,
					struct transaction *trans);

static void call_hold_cb(struct sipe_media_call *call,
			 gboolean local,
			 SIPE_UNUSED_PARAMETER gboolean state)
{
	if (local)
		sipe_invite_call(SIPE_MEDIA_CALL_PRIVATE->sipe_private,
				 sipe_media_send_ack);
}

static void call_hangup_cb(struct sipe_media_call *call, gboolean local)
{
	struct sipe_media_call_private *call_private = SIPE_MEDIA_CALL_PRIVATE;

	if (local) {
		struct sip_session *session;
		session = sipe_session_find_call(call_private->sipe_private,
						 call_private->with);

		if (session) {
			sipe_session_close(call_private->sipe_private, session);
		}
	}
	call_private->sipe_private->media_call = NULL;
	sipe_media_call_free(call_private);
}

static struct sipe_media_call_private *
sipe_media_call_new(struct sipe_core_private *sipe_private,
		    const gchar* with, gboolean initiator)
{
	struct sipe_media_call_private *call_private = g_new0(struct sipe_media_call_private, 1);

	call_private->sipe_private = sipe_private;
	call_private->public.backend_private = sipe_backend_media_new(SIPE_CORE_PUBLIC,
								      SIPE_MEDIA_CALL,
								      with,
								      initiator);

	call_private->legacy_mode = FALSE;
	call_private->using_nice = TRUE;
	call_private->encryption_compatible = TRUE;

	call_private->public.candidates_prepared_cb = candidates_prepared_cb;
	call_private->public.media_connected_cb     = media_connected_cb;
	call_private->public.call_accept_cb         = call_accept_cb;
	call_private->public.call_reject_cb         = call_reject_cb;
	call_private->public.call_hold_cb           = call_hold_cb;
	call_private->public.call_hangup_cb         = call_hangup_cb;

	return call_private;
}

void sipe_media_hangup(struct sipe_core_private *sipe_private)
{
	struct sipe_media_call_private *call_private = sipe_private->media_call;
	if (call_private)
		sipe_backend_media_hangup(call_private->public.backend_private,
					  FALSE);
}

void
sipe_core_media_initiate_call(struct sipe_core_public *sipe_public,
			      const char *with,
			      gboolean with_video)
{
	struct sipe_core_private *sipe_private = SIPE_CORE_PRIVATE;
	struct sipe_media_call_private *call_private;
	struct sipe_backend_media *backend_media;
	struct sip_session *session;
	struct sip_dialog *dialog;

	if (sipe_private->media_call)
		return;

	call_private = sipe_media_call_new(sipe_private, with, TRUE);

	session = sipe_session_add_call(sipe_private, with);
	dialog = sipe_dialog_add(session);
	dialog->callid = gencallid();
	dialog->with = g_strdup(session->with);
	dialog->ourtag = gentag();

	call_private->with = g_strdup(session->with);

	backend_media = call_private->public.backend_private;

	if (!sipe_backend_media_add_stream(backend_media,
					   "audio", with, SIPE_MEDIA_AUDIO,
					   call_private->using_nice, TRUE)) {
		sipe_backend_notify_error(_("Error occured"),
					  _("Error creating audio stream"));
		sipe_media_call_free(call_private);
		return;
	}

	if (   with_video
	    && !sipe_backend_media_add_stream(backend_media,
			    	    	      "video", with, SIPE_MEDIA_VIDEO,
			    	    	      call_private->using_nice, TRUE)) {
		sipe_backend_notify_error(_("Error occured"),
					  _("Error creating video stream"));
		sipe_media_call_free(call_private);
		return;
	}

	sipe_private->media_call = call_private;

	// Processing continues in candidates_prepared_cb
}

void
process_incoming_invite_call(struct sipe_core_private *sipe_private,
			     struct sipmsg *msg)
{
	struct sipe_media_call_private *call_private = sipe_private->media_call;
	struct sipe_backend_media *backend_media;
	struct sdpmsg *smsg;
	gboolean has_new_media = FALSE;
	GSList *i;

	if (call_private && !is_media_session_msg(call_private, msg)) {
		sip_transport_response(sipe_private, msg, 486, "Busy Here", NULL);
		return;
	}

	smsg = sdpmsg_parse_msg(msg->body);
	if (!smsg) {
		sip_transport_response(sipe_private, msg,
				       488, "Not Acceptable Here", NULL);
		sipe_media_hangup(sipe_private);
		return;
	}

	if (!call_private) {
		gchar *with = parse_from(sipmsg_find_header(msg, "From"));
		struct sip_session *session;
		struct sip_dialog *dialog;

		call_private = sipe_media_call_new(sipe_private, with, FALSE);
		session = sipe_session_add_call(sipe_private, with);
		dialog = sipe_media_dialog_init(session, msg);

		call_private->with = g_strdup(session->with);
		sipe_private->media_call = call_private;
		g_free(with);
	}

	backend_media = call_private->public.backend_private;

	if (call_private->invitation)
		sipmsg_free(call_private->invitation);
	call_private->invitation = sipmsg_copy(msg);

	// Create any new media streams
	for (i = smsg->media; i; i = i->next) {
		struct sdpmedia *media = i->data;
		gchar *id = media->name;
		SipeMediaType type;

		if (   media->port != 0
		    && !sipe_backend_media_get_stream_by_id(backend_media, id)) {
			gchar *with;

			if (sipe_strequal(id, "audio"))
				type = SIPE_MEDIA_AUDIO;
			else if (sipe_strequal(id, "video"))
				type = SIPE_MEDIA_VIDEO;
			else
				continue;

			with = parse_from(sipmsg_find_header(msg, "From"));
			sipe_backend_media_add_stream(backend_media, id, with,
						      type,
						      !call_private->legacy_mode,
						      FALSE);
			has_new_media = TRUE;
			g_free(with);
		}
	}

	if (has_new_media) {
		call_private->smsg = smsg;
		sip_transport_response(sipe_private, call_private->invitation,
				       180, "Ringing", NULL);
		// Processing continues in candidates_prepared_cb
	} else {
		do_apply_remote_message(call_private, smsg);
		sdpmsg_free(smsg);
	}
}

void process_incoming_cancel_call(struct sipe_core_private *sipe_private,
				  struct sipmsg *msg)
{
	struct sipe_media_call_private *call_private = sipe_private->media_call;

	// We respond to the CANCEL request with 200 OK response and
	// with 487 Request Terminated to the remote INVITE in progress.
	sip_transport_response(sipe_private, msg, 200, "OK", NULL);

	if (call_private->invitation) {
		sip_transport_response(sipe_private, call_private->invitation,
				       487, "Request Terminated", NULL);
	}

	sipe_media_hangup(sipe_private);
}

static gboolean
sipe_media_send_ack(struct sipe_core_private *sipe_private,
					SIPE_UNUSED_PARAMETER struct sipmsg *msg,
					struct transaction *trans)
{
	struct sipe_media_call_private *call_private = sipe_private->media_call;
	struct sip_session *session;
	struct sip_dialog *dialog;
	int trans_cseq;
	int tmp_cseq;

	session = sipe_session_find_call(sipe_private, call_private->with);
	dialog = session->dialogs->data;
	if (!dialog)
		return FALSE;

	tmp_cseq = dialog->cseq;

	sscanf(trans->key, "<%*[a-zA-Z0-9]><%d INVITE>", &trans_cseq);
	dialog->cseq = trans_cseq - 1;
	sip_transport_ack(sipe_private, dialog);
	dialog->cseq = tmp_cseq;

	dialog->outgoing_invite = NULL;

	return TRUE;
}

static gboolean
sipe_media_send_final_ack(struct sipe_core_private *sipe_private,
			  SIPE_UNUSED_PARAMETER struct sipmsg *msg,
			  struct transaction *trans)
{
	sipe_media_send_ack(sipe_private, msg, trans);
	sipe_backend_media_accept(sipe_private->media_call->public.backend_private,
				  FALSE);

	return TRUE;
}

static gboolean
process_invite_call_response(struct sipe_core_private *sipe_private,
			     struct sipmsg *msg,
			     struct transaction *trans)
{
	const gchar *with;
	struct sipe_media_call_private *call_private = sipe_private->media_call;
	struct sipe_backend_media *backend_private;
	struct sip_session *session;
	struct sip_dialog *dialog;
	struct sdpmsg *smsg;

	if (!is_media_session_msg(call_private, msg))
		return FALSE;

	session = sipe_session_find_call(sipe_private, call_private->with);
	dialog = session->dialogs->data;

	backend_private = call_private->public.backend_private;
	with = dialog->with;

	dialog->outgoing_invite = NULL;

	if (msg->response >= 400) {
		// Call rejected by remote peer or an error occurred
		gchar *title;
		GString *desc = g_string_new("");
		gboolean append_responsestr = FALSE;

		switch (msg->response) {
			case 480: {
				const gchar *warn = sipmsg_find_header(msg, "Warning");
				title = _("User unavailable");

				if (warn && g_str_has_prefix(warn, "391 lcs.microsoft.com")) {
					g_string_append_printf(desc, _("%s does not want to be disturbed"), with);
				} else
					g_string_append_printf(desc, _("User %s is not available"), with);
				break;
			}
			case 603:
			case 605:
				title = _("Call rejected");
				g_string_append_printf(desc, _("User %s rejected call"), with);
				break;
			default:
				title = _("Error occured");
				g_string_append(desc, _("Unable to establish a call"));
				append_responsestr = TRUE;
				break;
		}

		if (append_responsestr)
			g_string_append_printf(desc, "\n%d %s",
					       msg->response, msg->responsestr);

		sipe_backend_notify_error(title, desc->str);
		g_string_free(desc, TRUE);

		sipe_media_send_ack(sipe_private, msg, trans);
		sipe_media_hangup(sipe_private);

		return TRUE;
	}

	sipe_dialog_parse(dialog, msg, TRUE);
	smsg = sdpmsg_parse_msg(msg->body);
	if (!smsg) {
		sip_transport_response(sipe_private, msg,
				       488, "Not Acceptable Here", NULL);
		sipe_media_hangup(sipe_private);
		return FALSE;
	}

	if (!apply_remote_message(call_private, smsg)) {
		sip_transport_response(sipe_private, msg,
				       487, "Request Terminated", NULL);
		sipe_media_hangup(sipe_private);
	} else if (msg->response == 183) {
		// Session in progress
		const gchar *rseq = sipmsg_find_header(msg, "RSeq");
		const gchar *cseq = sipmsg_find_header(msg, "CSeq");
		gchar *rack = g_strdup_printf("RAck: %s %s\r\n", rseq, cseq);
		sip_transport_request(sipe_private,
		      "PRACK",
		      with,
		      with,
		      rack,
		      NULL,
		      dialog,
		      NULL);
		g_free(rack);
	} else {
		sipe_media_send_ack(sipe_private, msg, trans);

		if (call_private->legacy_mode && call_private->using_nice) {
			// TODO: legacy
/*			// We created non-legacy stream as we don't know which version of
			// client is on the other side until first SDP response is received.
			// This client requires legacy mode, so we must remove current session
			// (using ICE) and create new using raw UDP transport.
			struct sipe_backend_stream *new_stream;

			call_private->using_nice = FALSE;

			new_stream = sipe_backend_media_add_stream(backend_private,
								   with,
								   SIPE_MEDIA_AUDIO,
								   FALSE,
								   TRUE);

			sipe_backend_media_remove_stream(backend_private,
							 call_private->voice_stream);
			call_private->voice_stream = new_stream;

			apply_remote_message(call_private, smsg);

			// New INVITE will be sent in candidates_prepared_cb  */
		} else {
			sipe_invite_call(sipe_private, sipe_media_send_final_ack);
		}
	}

	sdpmsg_free(smsg);

	return TRUE;
}

gboolean is_media_session_msg(struct sipe_media_call_private *call_private,
			      struct sipmsg *msg)
{
	if (call_private) {
		const gchar *callid = sipmsg_find_header(msg, "Call-ID");
		struct sip_session *session;

		session = sipe_session_find_call(call_private->sipe_private,
						 call_private->with);
		if (session) {
			struct sip_dialog *dialog = session->dialogs->data;
			return sipe_strequal(dialog->callid, callid);
		}
	}
	return FALSE;
}

void sipe_media_handle_going_offline(struct sipe_media_call_private *call_private)
{
	struct sipe_backend_media *backend_private;

	backend_private = call_private->public.backend_private;

	if (   !sipe_backend_media_is_initiator(backend_private, NULL)
	    && !sipe_backend_media_accepted(backend_private)) {
		sip_transport_response(call_private->sipe_private,
				       call_private->invitation,
				       480, "Temporarily Unavailable", NULL);
	} else {
		struct sip_session *session;

		session = sipe_session_find_call(call_private->sipe_private,
						 call_private->with);
		if (session)
			sipe_session_close(call_private->sipe_private, session);
	}

	sipe_media_hangup(call_private->sipe_private);
}

/*
  Local Variables:
  mode: c
  c-file-style: "bsd"
  indent-tabs-mode: t
  tab-width: 8
  End:
*/
