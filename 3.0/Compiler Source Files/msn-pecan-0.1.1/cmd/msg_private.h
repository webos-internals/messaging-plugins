/**
 * Copyright (C) 2008-2009 Felipe Contreras
 * Copyright (C) 1998-2006 Pidgin (see pidgin-copyright)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef MSN_MSG_PRIVATE_H
#define MSN_MSG_PRIVATE_H

#include <glib.h>

#include "msg.h"
#include "command.h"
#include "transaction.h"

typedef struct
{
    guint32 session_id;
    guint32 id;
    guint64 offset;
    guint64 total_size;
    guint32 length;
    guint32 flags;
    guint32 ack_id;
    guint32 ack_sub_id;
    guint64 ack_size;

} MsnSlpHeader;

typedef struct
{
    guint32 value;

} MsnSlpFooter;

/**
 * A message.
 */
struct MsnMessage
{
    gsize ref_count; /**< The reference count.       */

    MsnMsgType type;

    gboolean msnslp_message;

    gchar *remote_user;
    gchar flag;

    gchar *content_type;
    gchar *charset;
    gchar *body;
    gsize body_len;
    guint total_chunks;   /**< How many chunks in this multi-part message */
    guint received_chunks; /**< How many chunks we've received so far */

    GHashTable *attr_table;
    GList *attr_list;

    MsnCommand *cmd;
    MsnTransaction *trans;

    MsnMsgCb ack_cb; /**< The callback to call when we receive an ACK of this
                       message. */
    MsnMsgCb nak_cb; /**< The callback to call when we receive a NAK of this
                       message. */
    gpointer ack_data; /**< The data used by callbacks. */

    MsnMsgErrorType error; /**< The error of the message. */

    MsnSlpHeader msnslp_header;
    MsnSlpFooter msnslp_footer;
};

#endif /* MSN_MSG_PRIVATE_H */
