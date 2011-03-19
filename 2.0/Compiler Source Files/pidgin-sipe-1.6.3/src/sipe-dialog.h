/**
 * @file sipe-dialog.h
 *
 * pidgin-sipe
 *
 * Copyright (C) 2009 SIPE Project <http://sipe.sourceforge.net/>
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

/* Helper macros to iterate over dialog list in a SIP session */
#define SIPE_DIALOG_FOREACH {                            \
	GSList *entry = session->dialogs;                \
	while (entry) {                                  \
		struct sip_dialog *dialog = entry->data; \
		entry = entry->next;
#define SIPE_DIALOG_FOREACH_END }}

/* dialog is the new term for call-leg */
struct sip_dialog {
	gchar *with; /* URI */
	gchar *endpoint_GUID;
	/** 
	 *  >0 - pro
	 *  <0 - contra
	 *   0 - didn't participate
	 */ 
	int election_vote;
	gchar *ourtag;
	gchar *theirtag;
	gchar *theirepid;
	gchar *callid;
	GSList *routes;
	gchar *request;
	GSList *supported; /* counterparty capabilities */
	int cseq;
	gboolean is_established;
	struct transaction *outgoing_invite;
};

/* RFC3265 subscription */
struct sip_subscription {
	struct sip_dialog dialog;
	gchar *event;
};

/* Forward declaration */
struct sip_session;

/**
 * Free dialog structure
 *
 * @param dialog (in) Dialog to be freed. May be NULL.
 */
void sipe_dialog_free(struct sip_dialog *dialog);

/**
 * Free subscription structure
 *
 * @param subscription (in) Subscription to be freed. May be NULL.
 */
void sipe_subscription_free(struct sip_subscription *subscription);

/**
 * Add a new, empty dialog to a session
 *
 * @param session (in)
 *
 * @return dialog the new dialog structure
 */
struct sip_dialog *sipe_dialog_add(struct sip_session *session);

/**
 * Find a dialog in a session
 *
 * @param session (in) may be NULL
 * @param who (in) dialog identifier. May be NULL
 *
 * @return dialog the requested dialog or NULL
 */
struct sip_dialog *sipe_dialog_find(struct sip_session *session,
				    const gchar *who);

/**
 * Remove a dialog from a session
 *
 * @param session (in) may be NULL
 * @param who (in) dialog identifier. May be NULL
 */
void sipe_dialog_remove(struct sip_session *session, const gchar *who);

/**
 * Remove all dialogs frome a session
 *
 * @param session (in)
 */
void sipe_dialog_remove_all(struct sip_session *session);

/**
 * Does a session have any dialogs?
 *
 * @param session (in)
 */
#define sipe_dialog_any(session) (session->dialogs != NULL)

/**
 * Return first dialog of a session
 *
 * @param session (in)
 */
#define sipe_dialog_first(session) ((struct sip_dialog *)session->dialogs->data)

/**
 * Fill dialog structure from SIP message
 *
 * @param msg      (in)     mesage
 * @param dialog   (in,out) dialog to fill
 * @param outgoing (in)     outgoing or incoming message
 */
void sipe_dialog_parse(struct sip_dialog *dialog,
		       const struct sipmsg *msg,		       
		       gboolean outgoing);
