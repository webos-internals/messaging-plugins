/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

/* libxmpp is the XMPP protocol plugin. It is linked against libjabbercommon,
 * which may be used to support other protocols (Bonjour) which may need to
 * share code.
 */

#include "libfacebook.h"
#include "fb_util.h"
#include "fb_connection.h"
#include "fb_messages.h"
#include "jabber.h"

#define FACEBOOK_APP_ID "107899262577150"
#define FACEBOOK_API_KEY "ff26b9e09239e33babaeaf747c778926"
#define FACEBOOK_SECRET "57a7da14aa16458ab1490fd305b6f545"

#define FACEBOOK_XMPP_PLUGIN_ID "prpl-bigbrownchunx-facebookxmpp"

/*
http://www.facebook.com/connect/prompt_permissions.php?api_key=ff26b9e09239e33babaeaf747c778926&v=1.0&next=http%3A%2F%2Fwww.facebook.com%2Fconnect%2Flogin_success.html&cancel=http%3A%2F%2Fwww.facebook.com%2Fconnect%2Flogin_failure.html&ext_perm=xmpp_login%2Cread_stream%2Cpublish_stream%2Cread_mailbox%2Coffline_access&channel_url=&display=page&canvas=0&return_session=1&source=login&fbconnect=1&offline_access=0&required=1
charset_test	Ä,¥,Ä,¥,?,?,?
fb_dtsg	1Oi8s
post_form_id	7c5917b2dfc9c709d03c10e0d45b631b
ext_perm	64,16384,32768,524288,32
perms_granted	{"32":["800753867"],"64":["800753867"],"16384":["800753867"],"32768":["800753867"],"524288":["800753867"]}
session_version	
data_perms_user	0
data_perms_friends	0

Location	http://www.facebook.com/connect/login_success.html?session=%7B%22session_key%22%3A%22f665d3b753064a328f8c0372-800753867%22%2C%22uid%22%3A800753867%2C%22expires%22%3A0%2C%22secret%22%3A%225b07b330e4d9754b8476927830b462e8%22%2C%22sig%22%3A%22573dd1e21d1f8f22450cba4ea060f3fb%22%7D
*/



static void fb_cookie_foreach_cb(gchar *cookie_name,
								 gchar *cookie_value, GString *str)
{
	/* TODO: Need to escape name and value? */
	g_string_append_printf(str, "%s=%s;", cookie_name, cookie_value);
}

/**
 * Serialize the fba->cookie_table hash table to a string.
 */
static gchar *fb_cookies_to_string(FacebookAccount *fba)
{
	GString *str;
	
	str = g_string_new(NULL);
	
	g_hash_table_foreach(fba->cookie_table,
						 (GHFunc)fb_cookie_foreach_cb, str);
	
	return g_string_free(str, FALSE);
}

void fb_authorise_app_cb(FacebookAccount *fba, const gchar *response, gsize len,
						 gpointer userdata)
{
	// session_key is stored in cookies somehow
	if (len && g_str_equal(response, "Success"))
	{
		purple_debug_info("facebookxmpp", "Autorised app cookies: %s", fb_cookies_to_string(fba));
	}
}

void fb_authorise_app(FacebookAccount *fba)
{
	gchar *postdata;
	gchar *url;
	gchar *encoded_charset_test;
	
	url = g_strdup_printf("/connect/prompt_permissions.php?api_key=" FACEBOOK_API_KEY "&v=1.0&"
						  "next=http%%3A%%2F%%2Fwww.facebook.com%%2Fconnect%%2Flogin_success.html&"
						  "cancel=http%%3A%%2F%%2Fwww.facebook.com%%2Fconnect%%2Flogin_failure.html&"
						  "ext_perm=xmpp_login%%2Cread_stream%%2Cpublish_stream%%2Cread_mailbox%%2Coffline_access&"
						  "channel_url=&display=page&canvas=0&return_session=1&source=login&"
						  "fbconnect=1&offline_access=0&required=1");
	
	
	encoded_charset_test = g_strdup(purple_url_encode("€,´,€,´,水,Д,Є"));
	postdata = g_strdup_printf("charset_test=%s&fb_dtsg=%s&post_form_id=%s&"
							   "ext_perm=64,16384,32768,524288,32&"
							   "perms_granted={\"32\":[\"%" G_GINT64_FORMAT "\"],\"64\":[\"%" G_GINT64_FORMAT "\"],\"16384\":[\"%" G_GINT64_FORMAT "\"],\"32768\":[\"%" G_GINT64_FORMAT "\"],\"524288\":[\"%" G_GINT64_FORMAT "\"]}&"
							   "session_version=&data_perms_user=0&data_perms_friends=0",
							   encoded_charset_test, (fba->dtsg?fba->dtsg:"(null)"),
							   (fba->post_form_id?fba->post_form_id:"(null)"),
							   fba->uid, fba->uid, fba->uid, fba->uid, fba->uid);
	fb_post_or_get(fba, FB_METHOD_POST, NULL, url, postdata, fb_authorise_app_cb, NULL, FALSE);
	
	g_free(url);
	g_free(postdata);
	g_free(encoded_charset_test);
}

gboolean api_request_traverse_func(gchar *key, gchar *value, GString *ret)
{
	ret = g_string_append(ret, purple_url_encode(key));
	ret = g_string_append_c(ret, '=');
	ret = g_string_append(ret, purple_url_encode(value));
	ret = g_string_append_c(ret, '&');
	return FALSE;
}

static gchar *prepare_api_request(GTree *request_vars, const gchar *method, const gchar *secret)
{
	gchar *ret, *sig, *temp;
	GString *retstring = g_string_new("");
	
	g_tree_replace(request_vars, "api_key", g_strdup(FACEBOOK_API_KEY));
	g_tree_replace(request_vars, "call_id", g_strdup_printf("%d", (int) time(NULL)));
	if (method)
	{
		g_tree_replace(request_vars, "method", g_strdup(method));
	}
	g_tree_replace(request_vars, "v", g_strdup("1.0"));
	g_tree_foreach(request_vars, (GTraverseFunc)api_request_traverse_func, ret);
	ret = g_string_free(retstring, FALSE);
	
	// Compute the 'sig' value
	sig = g_strdup(ret);
	purple_str_strip_char(sig, '&');
	temp = g_strconcat(purple_url_decode(sig), secret, NULL);
	g_free(sig);
	sig = fb_md5_encode(temp);
	g_free(temp);
	
	temp = g_strconcat(ret, "sig=", sig, NULL);
	g_free(ret);
	g_free(sig);
	
	return temp;
}

void fb_fbconnect_request(FacebookAccount *fba, char *fb_method,
		GTree *request_vars,
		FacebookProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive)
{
	gchar *postdata;
	
	postdata = prepare_api_request(request_vars, fb_method, 
								   purple_account_get_string(fba->account, "session_key", ""));
	
	fb_post_or_get(fba, FB_METHOD_POST,
		"api.facebook.com", "/restserver.php", postdata,
		callback_func, user_data, keepalive);
	
	g_free(postdata);
}

static void handle_jabber_receiving_xmlnode_signal(PurpleConnection *gc, xmlnode **packet, gpointer userdata) 
{
	JabberStream *js = userdata;
	xmlnode *mechnode;
	
	if(xmlnode_get_child(*packet, "mechanisms"))
	{
		//hack the JabberStream to only use our mech
		mechnode = xmlnode_get_child(*packet, "mechanisms");
		xmlnode_insert_data(mechnode, "X-FACEBOOK-PLATFORM", -1);
		js->auth_mech = facebook_mech;
	}
}
				   
static void fb_jabber_http_login_cb(FacebookAccount *fba, const gchar *response, gsize len,
										   gpointer userdata)
{
	gchar *user_cookie;
	
	purple_connection_update_progress(fba->pc, _("Authenticating"), 2, 3);
	
	/* Look for our uid */
	user_cookie = g_hash_table_lookup(fba->cookie_table, "c_user");
	if (user_cookie == NULL) 
	{
		/*
		 * Server didn't set the c_user cookie, so we must have given
		 * them a bad username or password
		 */
		purple_connection_error_reason(fba->pc,
									   PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
									   _("Incorrect username or password."));
		return;
	}
	fba->uid = atoll(user_cookie);
	purple_debug_info("facebook", "uid %" G_GINT64_FORMAT "\n", fba->uid);

	fb_get_post_form_id(fba, fb_authorise_app);
}
				   
static void fb_jabber_login(PurpleAccount *account)
{
	FacebookAccount *fba;
	PurpleConnection *pc = purple_account_get_connection(account);
	JabberStream *js;
	PurpleStoredImage *image;
	const gchar *old_username;
	gchar *new_username;

	purple_account_set_bool(account, "require_tls", FALSE);
	purple_account_set_bool(account, "old_ssl", FALSE);
	purple_account_set_bool(account, "auth_plain_in_clear", TRUE);
	purple_account_set_int(account, "port", 5222);
	purple_account_set_string(account, "connect_server", "chat.facebook.com");
	purple_account_set_string(account, "ft_proxies", "");
	purple_account_set_bool(account, "custom_smileys", FALSE);

	/* Hack the username so that it's a valid XMPP username */
	old_username = account->username;
	new_username = g_strconcat(purple_url_encode(old_username), "@", "chat.facebook.com", "/", "Pidgin", NULL);
	account->username = new_username;
	js = jabber_stream_new(account);
	account->username = old_username;
	g_free(new_username);
	
	if (js == NULL)
		return;
	
	fba = g_new0(FacebookAccount, 1);
	fba->account = account;
	fba->pc = pc;
	fba->uid = -1;
	
	purple_connection_set_state(pc, PURPLE_CONNECTING);
	purple_connection_update_progress(pc, _("Connecting"), 1, 3);
	
	//we want to hook into the "jabber-receiving-xmlnode" signal to be able to listen to our connect event
	purple_signal_connect(pc, "jabber-receiving-xmlnode", js,
						  PURPLE_CALLBACK(handle_jabber_receiving_xmlnode_signal), js);
	
	
	/* Have we authorised access to Facebook on this account? */
	if (purple_account_get_string(account, "session_key", NULL) == NULL)
	{
		fb_do_http_login(fba, fb_jabber_http_login_cb, js);
		/* Need to wait for the logins to happen before connecting to jabber */
		return;
	}
	
	jabber_stream_connect(js);
}

static void fb_jabber_close(PurpleConnection *pc)
{
	purple_signals_unregister_by_instance(pc);
	fb_close(pc);
	jabber_close(pc);
}

static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME | OPT_PROTO_MAIL_CHECK |
	OPT_PROTO_PASSWORD_OPTIONAL |
	OPT_PROTO_SLASH_COMMANDS_NATIVE,
	NULL,							/* user_splits */
	NULL,							/* protocol_options */
	{"png", 32, 32, 96, 96, 0, PURPLE_ICON_SCALE_SEND | PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
	fb_list_icon,				/* list_icon */
	NULL,			/* list_emblems */
	fb_status_text,				/* status_text */
	fb_tooltip_text,			/* tooltip_text */
	jabber_status_types,			/* status_types */
	jabber_blist_node_menu,			/* blist_node_menu */
	NULL,				/* chat_info */
	NULL,		/* chat_info_defaults */
	fb_jabber_login,					/* login */
	fb_jabber_close,					/* close */
	jabber_message_send_im,			/* send_im */
	NULL,				/* set_info */
	jabber_send_typing,				/* send_typing */
	jabber_buddy_get_info,			/* get_info */
	fb_set_status_p,				/* set_status */
	jabber_idle_set,				/* set_idle */
	NULL,							/* change_passwd */
	fb_add_buddy,		/* add_buddy */
	NULL,							/* add_buddies */
	fb_remove_buddy,		/* remove_buddy */
	NULL,							/* remove_buddies */
	NULL,							/* add_permit */
	NULL,				/* add_deny */
	NULL,							/* rem_permit */
	NULL,				/* rem_deny */
	NULL,							/* set_permit_deny */
	NULL,				/* join_chat */
	NULL,							/* reject_chat */
	NULL,			/* get_chat_name */
	NULL,				/* chat_invite */
	NULL,				/* chat_leave */
	NULL,							/* chat_whisper */
	NULL,		/* chat_send */
	jabber_keepalive,				/* keepalive */
	NULL,		/* register_user */
	NULL,							/* get_cb_info */
	NULL,							/* get_cb_away */
	jabber_roster_alias_change,		/* alias_buddy */
	fb_group_buddy_move,		/* group_buddy */
	fb_group_rename,		/* rename_group */
	fb_buddy_free,							/* buddy_free */
	jabber_convo_closed,			/* convo_closed */
	jabber_normalize,				/* normalize */
	NULL,			/* set_buddy_icon */
	fb_group_remove,							/* remove_group */
	NULL,	/* get_cb_real_name */
	NULL,			/* set_chat_topic */
	NULL,			/* find_blist_chat */
	NULL,		/* roomlist_get_list */
	NULL,			/* roomlist_cancel */
	NULL,							/* roomlist_expand_category */
	NULL,		/* can_receive_file */
	NULL,			/* send_file */
	NULL,				/* new_xfer */
	NULL,			/* offline_message */
	NULL,							/* whiteboard_prpl_ops */
	jabber_prpl_send_raw,			/* send_raw */
	NULL, /* roomlist_room_serialize */
	NULL,		/* unregister_user */
	NULL,			/* send_attention */
	NULL,			/* attention_types */
#if PURPLE_MAJOR_VERSION > 2 || PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >= 5
	sizeof(PurplePluginProtocolInfo),       /* struct_size */
	fb_get_account_text_table, /* get_account_text_table */
	NULL,          /* initiate_media */
	NULL,                  /* get_media_caps */
#else
	(gpointer) sizeof(PurplePluginProtocolInfo)
#endif
};

static gboolean load_plugin(PurplePlugin *plugin)
{
	purple_signal_register(plugin, "jabber-receiving-xmlnode",
			purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new_outgoing(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_XMLNODE));

	purple_signal_register(plugin, "jabber-sending-xmlnode",
			purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new_outgoing(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_XMLNODE));

	/*
	 * Do not remove this or the plugin will fail. Completely. You have been
	 * warned!
	 */
	purple_signal_connect_priority(plugin, "jabber-sending-xmlnode",
			plugin, PURPLE_CALLBACK(jabber_send_signal_cb),
			NULL, PURPLE_SIGNAL_PRIORITY_HIGHEST);

	purple_signal_register(plugin, "jabber-sending-text",
			     purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			     purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			     purple_value_new_outgoing(PURPLE_TYPE_STRING));

	purple_signal_register(plugin, "jabber-receiving-message",
			purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER_POINTER,
			purple_value_new(PURPLE_TYPE_BOOLEAN), 6,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new(PURPLE_TYPE_STRING), /* type */
			purple_value_new(PURPLE_TYPE_STRING), /* id */
			purple_value_new(PURPLE_TYPE_STRING), /* from */
			purple_value_new(PURPLE_TYPE_STRING), /* to */
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_XMLNODE));

	purple_signal_register(plugin, "jabber-receiving-iq",
			purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
			purple_value_new(PURPLE_TYPE_BOOLEAN), 5,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new(PURPLE_TYPE_STRING), /* type */
			purple_value_new(PURPLE_TYPE_STRING), /* id */
			purple_value_new(PURPLE_TYPE_STRING), /* from */
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_XMLNODE));

	purple_signal_register(plugin, "jabber-watched-iq",
			purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
			purple_value_new(PURPLE_TYPE_BOOLEAN), 5,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new(PURPLE_TYPE_STRING), /* type */
			purple_value_new(PURPLE_TYPE_STRING), /* id */
			purple_value_new(PURPLE_TYPE_STRING), /* from */
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_XMLNODE)); /* child */

	/* Modifying these? Look at jabber_init_plugin for the ipc versions */
	purple_signal_register(plugin, "jabber-register-namespace-watcher",
			purple_marshal_VOID__POINTER_POINTER,
			NULL, 2,
			purple_value_new(PURPLE_TYPE_STRING),  /* node */
			purple_value_new(PURPLE_TYPE_STRING)); /* namespace */

	purple_signal_register(plugin, "jabber-unregister-namespace-watcher",
			purple_marshal_VOID__POINTER_POINTER,
			NULL, 2,
			purple_value_new(PURPLE_TYPE_STRING),  /* node */
			purple_value_new(PURPLE_TYPE_STRING)); /* namespace */

	purple_signal_connect(plugin, "jabber-register-namespace-watcher",
			plugin, PURPLE_CALLBACK(jabber_iq_signal_register), NULL);
	purple_signal_connect(plugin, "jabber-unregister-namespace-watcher",
			plugin, PURPLE_CALLBACK(jabber_iq_signal_unregister), NULL);

	purple_signal_register(plugin, "jabber-receiving-presence",
			purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER,
			purple_value_new(PURPLE_TYPE_BOOLEAN), 4,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new(PURPLE_TYPE_STRING), /* type */
			purple_value_new(PURPLE_TYPE_STRING), /* from */
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_XMLNODE));

	return TRUE;
}

static gboolean unload_plugin(PurplePlugin *plugin)
{
	purple_signals_unregister_by_instance(plugin);

	/* reverse order of init_plugin */
	jabber_data_uninit();
	jabber_si_uninit();
	jabber_ibb_uninit();
	/* PEP things should be uninit via jabber_pep_uninit, not here */
	jabber_pep_uninit();
	jabber_caps_uninit();
	jabber_iq_uninit();

	jabber_unregister_commands();

	/* Stay on target...stay on target... Almost there... */
	jabber_uninit_plugin(plugin);

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	2,
	3,
	PURPLE_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	FACEBOOK_XMPP_PLUGIN_ID,                                    /**< id             */
	"Facebook (XMPP)", 					/* name */
	FACEBOOK_PLUGIN_VERSION, 			/* version */
	N_("Facebook Protocol Plugin"), 		/* summary */
	N_("Facebook Protocol Plugin"), 		/* description */
	"Eion Robb <eionrobb@gmail.com>", 		/* author */
	"http://pidgin-facebookchat.googlecode.com/",	/* homepage */

	load_plugin,                                      /**< load           */
	unload_plugin,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	fb_actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};


static JabberSaslState
fb_facebook_mech_start(JabberStream *js, xmlnode *packet, xmlnode **response,
                 char **error)
{
	xmlnode *auth = xmlnode_new("auth");
	xmlnode_set_namespace(auth, NS_XMPP_SASL);
	xmlnode_set_attrib(auth, "mechanism", "X-FACEBOOK-PLATFORM");

	*response = auth;
	return JABBER_SASL_STATE_CONTINUE;
}

/* Parts of this algorithm are inspired by stuff in libgsasl */
static GHashTable* parse_challenge(const char *challenge)
{
	GHashTable *ret = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	
	return ret;
}

static JabberSaslState
fb_facebook_handle_challenge(JabberStream *js, xmlnode *packet,
                            xmlnode **response, char **msg)
{
	xmlnode *reply = NULL;
	char *enc_in = xmlnode_get_data(packet);
	char *dec_in;
	char *enc_out;
	GHashTable *parts;
	JabberSaslState state = JABBER_SASL_STATE_CONTINUE;
	GTree *response_tree;
	FacebookAccount *fba;

	if (!enc_in) {
		*msg = g_strdup(_("Invalid response from server"));
		return JABBER_SASL_STATE_FAIL;
	}

	dec_in = (char *)purple_base64_decode(enc_in, NULL);
	purple_debug_misc("jabber", "decoded challenge (%"
			G_GSIZE_FORMAT "): %s\n", strlen(dec_in), dec_in);
			
	parts = parse_challenge(dec_in);
	
	response_tree = g_tree_new_full((GCompareDataFunc)strcmp, NULL, NULL, g_free);
	g_tree_insert(response_tree, "nonce", g_strdup(g_hash_table_lookup(parts, "nonce")));
	g_tree_insert(response_tree, "session_key", g_strdup(purple_account_get_string(js->account, "session_key", "")));
	
	//method, api_key, call_id, sig, v already sent
	prepare_api_request(response_tree, g_hash_table_lookup(parts, "method"), FACEBOOK_SECRET);
	
	g_hash_table_destroy(parts);
}

static JabberSaslMech facebook_mech = {
	20, /* priority - needs to be more important than the DIGEST-MD5 priority */
	"X-FACEBOOK-PLATFORM", /* name */
	fb_facebook_mech_start,
	fb_facebook_handle_challenge, /* handle_challenge */
	NULL, /* handle_success */
	NULL, /* handle_failure */
	NULL  /* dispose */
};

JabberSaslMech *fb_auth_get_facebook_mech(void)
{
	return &facebook_mech;
}

static void
init_plugin(PurplePlugin *plugin)
{
	
	auth_mechs = g_slist_insert_sorted(auth_mechs, fb_auth_get_facebook_mech(), compare_mech);


	my_protocol = plugin;
	jabber_init_plugin(plugin);

	purple_prefs_remove("/plugins/prpl/facebook");
	jabber_register_commands();

	/* reverse order of unload_plugin */
	jabber_iq_init();
	jabber_caps_init();
	/* PEP things should be init via jabber_pep_init, not here */
	jabber_pep_init();
	jabber_data_init();

	jabber_si_init();
}


PURPLE_INIT_PLUGIN(facebookxmpp, init_plugin, info);
