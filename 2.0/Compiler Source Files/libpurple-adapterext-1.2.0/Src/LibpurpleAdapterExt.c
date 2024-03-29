/*
 * <LibpurpleAdapterExt.c: wrapper around libpurple.so>
 *
 * Copyright 2009 Palm, Inc. All rights reserved.
 *
 * This program is free software and licensed under the terms of the GNU 
 * Lesser General Public License Version 2.1 as published by the Free 
 * Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License,   
 * Version 2.1 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-   
 * 1301, USA  

 * The LibpurpleAdapterExt is a wrapper around libpurple.so and provides a simple API over D-Bus to be used by
 * the messaging service and potentially other interested services/apps. 
 * The same D-Bus API will be implemented by other transport providers (e.g. Oz)
 * 
 */

#include "purple.h"

#include <glib.h>

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "defines.h"

#include <cjson/json.h>
#include <lunaservice.h>
#include <json_utils.h>

#include <pthread.h>

#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)
#define CONNECT_TIMEOUT_SECONDS 180

/**
 * The number of seconds we wait before disabling the server queue after the screen turns on
 */
#define DISABLE_QUEUE_TIMEOUT_SECONDS 10

/**
 * The number of seconds we wait after login before we enable the server queue (if display is off)
 */
#define POST_LOGIN_WAIT_SECONDS 10

static const char *dbusAddress = "im.libpurpleext.greg";

static LSHandle *serviceHandle = NULL;
/**
 * List of accounts that are online
 */
static GHashTable *onlineAccountData = NULL;
/**
 * List of accounts that are in the process of logging in
 */
static GHashTable *pendingAccountData = NULL;
static GHashTable *offlineAccountData = NULL;
static GHashTable *accountLoginTimers = NULL;
static GHashTable *loginMessages = NULL;
static GHashTable *logoutMessages = NULL;
static GHashTable *connectionTypeData = NULL;

static bool libpurpleInitialized = FALSE;
static bool registeredForAccountSignals = FALSE;
static bool registeredForPresenceUpdateSignals = FALSE;
static bool registeredForDisplayEvents = FALSE;
static bool currentDisplayState = TRUE; // TRUE: display on

char *JabberServer = "";
char *JabberServerPort = "";
bool JabberServerTLS = FALSE;
char *JabberResource = "";
char *SIPEServer = "";
char *SIPEServerPort = "";
char *SIPEServerLogin = "";
bool SIPEServerTLS = FALSE;
bool SIPEServerProxy = FALSE;
char *SIPEUserAgent = "";
char *SametimeServer = "";
char *SametimeServerPort = "";
bool SametimeServerTLS = FALSE;
bool SametimehideID = FALSE;
char *gwimServer = "";
char *gwimServerPort = "";
bool gwimServerTLS = FALSE;
char *MySpaceServer = "";
char *MySpaceServerPort = "";
char *GaduServer = "";
char *XFireServer = "";
char *XFireServerPort = "";
char *XFireversion = "122";

bool GTalkAvatar = FALSE;
bool AIMAvatar = FALSE;
bool YahooAvatar = FALSE;
bool JabberAvatar = FALSE;
bool gwimAvatar = FALSE;
bool ICQAvatar = FALSE;
bool LiveAvatar = FALSE;
bool SIPEAvatar = FALSE;
bool QQAvatar = FALSE;
bool SametimeAvatar = FALSE;
bool XFireAvatar = FALSE;
bool FacebookAvatar = FALSE;
bool MySpaceAvatar = FALSE;
bool GaduAvatar = FALSE;

bool LiveAlias = FALSE;
bool XFireAlias = FALSE;
bool ICQAlias = FALSE;
/** 
 * Keeps track of the local IP address that we bound to when logging in to individual accounts 
 * key: accountKey, value: IP address 
 */
static GHashTable *ipAddressesBoundTo = NULL;

static void adapterUIInit(void)
{
	purple_conversations_set_ui_ops(&adapterConversationUIOps);
}

static void destroyNotify(gpointer dataToFree)
{
	g_free(dataToFree);
}

static gboolean adapterInvokeIO(GIOChannel *ioChannel, GIOCondition ioCondition, gpointer data)
{
	IOClosure *ioClosure = data;
	PurpleInputCondition purpleCondition = 0;
	
	if (PURPLE_GLIB_READ_COND & ioCondition)
	{
		purpleCondition = purpleCondition | PURPLE_INPUT_READ;
	}
	
	if (PURPLE_GLIB_WRITE_COND & ioCondition)
	{
		purpleCondition = purpleCondition | PURPLE_INPUT_WRITE;
	}

	ioClosure->function(ioClosure->data, g_io_channel_unix_get_fd(ioChannel), purpleCondition);

	return TRUE;
}

static guint adapterIOAdd(gint fd, PurpleInputCondition purpleCondition, PurpleInputFunction inputFunction, gpointer data)
{
	GIOChannel *ioChannel;
	GIOCondition ioCondition = 0;
	IOClosure *ioClosure = g_new0(IOClosure, 1);

	ioClosure->data = data;
	ioClosure->function = inputFunction;

	if (PURPLE_INPUT_READ & purpleCondition)
	{
		ioCondition = ioCondition | PURPLE_GLIB_READ_COND;
	}
	
	if (PURPLE_INPUT_WRITE & purpleCondition)
	{
		ioCondition = ioCondition | PURPLE_GLIB_WRITE_COND;
	}

	ioChannel = g_io_channel_unix_new(fd);
	ioClosure->result = g_io_add_watch_full(ioChannel, G_PRIORITY_DEFAULT, ioCondition, adapterInvokeIO, ioClosure,
			destroyNotify);

	g_io_channel_unref(ioChannel);
	return ioClosure->result;
}
/*
 * Helper methods 
 * TODO: move them to the right spot
 */

/*
 * TODO: make constants for the java-side availability values
 */

static int getPalmAvailabilityFromPrplAvailability(int prplAvailability)
{
	switch (prplAvailability)
	{
	case PURPLE_STATUS_UNSET:
		return 6;
	case PURPLE_STATUS_OFFLINE:
		return 4;
	case PURPLE_STATUS_AVAILABLE:
		return 0;
	case PURPLE_STATUS_UNAVAILABLE:
		return 2;
	case PURPLE_STATUS_INVISIBLE:
		return 3;
	case PURPLE_STATUS_AWAY:
		return 2;
	case PURPLE_STATUS_EXTENDED_AWAY:
		return 2;
	case PURPLE_STATUS_MOBILE:
		return 1;
	case PURPLE_STATUS_TUNE:
		return 0;
	default:
		return 4;
	}
}

static int getPrplAvailabilityFromPalmAvailability(int palmAvailability)
{
	switch (palmAvailability)
	{
	case 0:
		return PURPLE_STATUS_AVAILABLE;
	case 1:
		return PURPLE_STATUS_MOBILE;
	case 2:
		return PURPLE_STATUS_AWAY;
	case 3:
		return PURPLE_STATUS_INVISIBLE;
	case 4:
		return PURPLE_STATUS_OFFLINE;
	default:
		return PURPLE_STATUS_OFFLINE;
	}
}

static bool getpluginpreference(const char *serviceName, const char *preference)
{
	if (!preference || !serviceName)
	{
		return FALSE;
	}

	if (strcmp(serviceName, "live") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			if (LiveAvatar)
			{
				return TRUE;
			}
		}
		if (strcmp(preference, "Alias") == 0)
		{
			if (LiveAlias)
			{
				return TRUE;
			}
		}
	}
	if (strcmp(serviceName, "xfire") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			if (XFireAvatar)
			{
				return TRUE;
			}
		}
		if (strcmp(preference, "Alias") == 0)
		{
			if (XFireAlias)
			{
				return TRUE;
			}
		}
	}
	if (strcmp(serviceName, "sipe") == 0)
	{
		if (SIPEAvatar)
		{
			return TRUE;
		}
	}
	if (strcmp(serviceName, "yahoo") == 0)
	{
		if (YahooAvatar)
		{
			return TRUE;
		}
	}
	if (strcmp(serviceName, "jabber") == 0)
	{
		if (JabberAvatar)
		{
			return TRUE;
		}
	}
	if (strcmp(serviceName, "lcs") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			if (SametimeAvatar)
			{
				return TRUE;
			}
		}
		if (strcmp(preference, "Alias") == 0)
		{
			return TRUE;
		}
	}
	if (strcmp(serviceName, "gwim") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			if (gwimAvatar)
			{
				return TRUE;
			}
		}
	}
	if (strcmp(serviceName, "gadu") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			if (GaduAvatar)
			{
				return TRUE;
			}
		}
	}
	if (strcmp(serviceName, "myspace") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			if (MySpaceAvatar)
			{
				return TRUE;
			}
		}
	}
	if (strcmp(serviceName, "icq") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			if (ICQAvatar)
			{
				return TRUE;
			}
		}
		if (strcmp(preference, "Alias") == 0)
		{
			if (ICQAlias)
			{
				return TRUE;
			}
		}
	}
	if (strcmp(serviceName, "qqim") == 0)
	{
		if (QQAvatar)
		{
			return TRUE;
		}
	}
	if (strcmp(serviceName, "facebook") == 0)
	{
		if (FacebookAvatar)
		{
			return TRUE;
		}
	}
	if (strcmp(serviceName, "aol") == 0)
	{
		if (AIMAvatar)
		{
			return TRUE;
		}
	}
	if (strcmp(serviceName, "gmail") == 0)
	{
		if (GTalkAvatar)
		{
			return TRUE;
		}
	}
	
	return FALSE;
}

/*
 * This method handles special cases where the username passed by the java side does not satisfy a particular prpl's requirement
 * (e.g. for logging into AIM, the java service uses "amiruci@aol.com", yet the aim prpl expects "amiruci"; same scenario with yahoo)
 * Free the returned string when you're done with it 
 */
static char* getPrplFriendlyUsername(const char *serviceName, const char *username)
{
	if (!username || !serviceName)
	{
		return "";
	}

	char *transportFriendlyUsername;
	if (strcmp(serviceName, "aol") == 0)
	{
		// Need to strip off @aol.com, but not @aol.com.mx
		const char *extension = strstr(username, "@aol.com");
		if (extension != NULL && strstr(extension, "aol.com.") == NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@aol.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "yahoo") == 0)
	{
		if (strstr(username, "@yahoo.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@yahoo.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "icq") == 0)
	{
		if (strstr(username, "@icq.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@icq.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "gwim") == 0)
	{
		if (strstr(username, "@gwim.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@gwim.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "myspace") == 0)
	{
		if (strstr(username, "@myspace.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@myspace.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "gadu") == 0)
	{
		if (strstr(username, "@gadu.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@gadu.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "lcs") == 0)
	{
		if (strstr(username, "@lcs.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@lcs.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "sipe") == 0)
	{
		if (strstr(username, "@sipe.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@sipe.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "qqim") == 0)
	{
		if (strstr(username, "@qqim.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@qqim.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}
	else if (strcmp(serviceName, "xfire") == 0)
	{
		if (strstr(username, "@xfire.com") != NULL)
		{
			transportFriendlyUsername = malloc(strlen(username) - strlen("@xfire.com") + 1);
			char *usernameCopy= alloca(strlen(username) + 1);
			strcpy(usernameCopy, username);
			strtok(usernameCopy, "@");
			strcpy(transportFriendlyUsername, usernameCopy);
			return transportFriendlyUsername;
		}
	}

	//Special Case for Office Communicator when DOMAIN\USER is set. Account name is USERNAME,DOMAIN\USER
	if (strcmp(serviceName, "sipe") == 0 && strcmp(SIPEServerLogin, "") != 0)
	{
		if (strstr(username, ",") != NULL)
		{
			//A "," exists in the sign in name already
			transportFriendlyUsername = malloc(strlen(username) + 1);
			transportFriendlyUsername = strcpy(transportFriendlyUsername, username);
			return transportFriendlyUsername;
		}
		else
		{
			transportFriendlyUsername = malloc(strlen(username) + 1);
			transportFriendlyUsername = strcpy(transportFriendlyUsername, username);

			//SIPE Account
			char *SIPEFullLoginName = NULL;
			SIPEFullLoginName = (char *)calloc(strlen(transportFriendlyUsername) + strlen(SIPEServerLogin) + 2, sizeof(char));
			strcat(SIPEFullLoginName, transportFriendlyUsername);
			strcat(SIPEFullLoginName, ",");
			strcat(SIPEFullLoginName, SIPEServerLogin);

			return SIPEFullLoginName;
		}
	}

	//Special Case for jabber when resource is set. Account name is USERNAME/RESOURCE
	if (strcmp(serviceName, "jabber") == 0 && strcmp(JabberResource, "") != 0)
	{
		if (strstr(username, "/") != NULL)
		{
			//A "/" exists in the sign in name already
			transportFriendlyUsername = malloc(strlen(username) + 1);
			transportFriendlyUsername = strcpy(transportFriendlyUsername, username);

			return transportFriendlyUsername;
		}
		else
		{
			transportFriendlyUsername = malloc(strlen(username) + 1);
			transportFriendlyUsername = strcpy(transportFriendlyUsername, username);

			//Jabber Account
			char *JabberFullLoginName = NULL;
			JabberFullLoginName = (char *)calloc(strlen(transportFriendlyUsername) + strlen(JabberResource) + 2, sizeof(char));
			strcat(JabberFullLoginName, transportFriendlyUsername);
			strcat(JabberFullLoginName, "/");
			strcat(JabberFullLoginName, JabberResource);

			return JabberFullLoginName;
		}
	}

	//Everything else
	transportFriendlyUsername = malloc(strlen(username) + 1);
	transportFriendlyUsername = strcpy(transportFriendlyUsername, username);
	return transportFriendlyUsername;
}

/*
 * The messaging service expects the username to be in the username@domain.com format, whereas the AIM prpl uses the username only
 * Free the returned string when you're done with it 
 */
static char* getJavaFriendlyUsername(const char *username, const char *serviceName)
{
	if (!username || !serviceName)
	{
		return strdup("");
	}
	GString *javaFriendlyUsername = g_string_new(username);
	if (strcmp(serviceName, "aol") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@aol.com");
	}
	else if (strcmp(serviceName, "yahoo") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@yahoo.com");
	}
	else if (strcmp(serviceName, "icq") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@icq.com");
	}
	else if (strcmp(serviceName, "gwim") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@gwim.com");
	}
	else if (strcmp(serviceName, "myspace") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@myspace.com");
	}
	else if (strcmp(serviceName, "gadu") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@gadu.com");
	}
	else if (strcmp(serviceName, "xfire") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@xfire.com");
	}
	else if (strcmp(serviceName, "gmail") == 0)
	{
		char *resource = memchr(username, '/', strlen(username));
		if (resource != NULL)
		{
			int charsToKeep = resource - username;
			g_string_erase(javaFriendlyUsername, charsToKeep, -1);
		}
	}
	else if (strcmp(serviceName, "lcs") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@lcs.com");
	}
	else if (strcmp(serviceName, "jabber") == 0)
	{
		if (strcmp(JabberResource, "") != 0)
		{
			//If jabber resource is blank remove /
			char *resource = memchr(username, '/', strlen(username));
			if (resource != NULL)
			{
				int charsToKeep = resource - username;
				g_string_erase(javaFriendlyUsername, charsToKeep, -1);
			}
		}
	}
	else if (strcmp(serviceName, "sipe") == 0)
	{
		char *resource = memchr(username, ',', strlen(username));
		if (resource != NULL)
		{
			int charsToKeep = resource - username;
			g_string_erase(javaFriendlyUsername, charsToKeep, -1);
		}
	}
	else if (strcmp(serviceName, "qqim") == 0 && strchr(username, '@') == NULL)
	{
		g_string_append(javaFriendlyUsername, "@qqim.com");
	}
	char *javaFriendlyUsernameToReturn = strdup(javaFriendlyUsername->str);
	g_string_free(javaFriendlyUsername, TRUE);
	return javaFriendlyUsernameToReturn;
}

static char* stripResourceFromGtalkUsername(const char *username)
{
	if (!username)
	{
		return "";
	}
	GString *javaFriendlyUsername = g_string_new(username);
	char *resource = memchr(username, '/', strlen(username));
	if (resource != NULL)
	{
		int charsToKeep = resource - username;
		g_string_erase(javaFriendlyUsername, charsToKeep, -1);
	}
	char *javaFriendlyUsernameToReturn = strdup(javaFriendlyUsername->str);
	g_string_free(javaFriendlyUsername, TRUE);
	return javaFriendlyUsernameToReturn;
}

static char* getJavaFriendlyErrorCode(PurpleConnectionError type)
{
	char *javaFriendlyErrorCode;
	if (type == PURPLE_CONNECTION_ERROR_INVALID_USERNAME)
	{
		javaFriendlyErrorCode = "AcctMgr_Bad_Username";
	}
	else if (type == PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED)
	{
		javaFriendlyErrorCode = "AcctMgr_Bad_Authentication";
	}
	else if (type == PURPLE_CONNECTION_ERROR_NETWORK_ERROR)
	{
		javaFriendlyErrorCode = "AcctMgr_Network_Error";
	}
	else if (type == PURPLE_CONNECTION_ERROR_NAME_IN_USE)
	{
		javaFriendlyErrorCode = "AcctMgr_Name_In_Use";
	}
	else if (type == PURPLE_CONNECTION_ERROR_CERT_NOT_PROVIDED)
	{
		javaFriendlyErrorCode = "AcctMgr_Cert_Error";
	}
	else if (type == PURPLE_CONNECTION_ERROR_CERT_UNTRUSTED)
	{
		javaFriendlyErrorCode = "AcctMgr_Cert_Error";
	}
	else if (type == PURPLE_CONNECTION_ERROR_CERT_EXPIRED)
	{
		javaFriendlyErrorCode = "AcctMgr_Cert_Error";
	}
	else if (type == PURPLE_CONNECTION_ERROR_CERT_NOT_ACTIVATED)
	{
		javaFriendlyErrorCode = "AcctMgr_Cert_Error";
	}
	else if (type == PURPLE_CONNECTION_ERROR_CERT_HOSTNAME_MISMATCH)
	{
		javaFriendlyErrorCode = "AcctMgr_Cert_Error";
	}
	else if (type == PURPLE_CONNECTION_ERROR_CERT_FINGERPRINT_MISMATCH)
	{
		javaFriendlyErrorCode = "AcctMgr_Cert_Error";
	}
	else if (type == PURPLE_CONNECTION_ERROR_CERT_SELF_SIGNED)
	{
		javaFriendlyErrorCode = "AcctMgr_Cert_Error";
	}
	else if (type == PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR)
	{
		javaFriendlyErrorCode = "AcctMgr_Cert_Error";
	}
	else
	{
		syslog(LOG_INFO, "PurpleConnectionError was %i", type);
		javaFriendlyErrorCode = "AcctMgr_Generic_Error";
	}
	return javaFriendlyErrorCode;
}

/*
 * Given java-friendly serviceName, it will return prpl-specific protocol_id (e.g. given "aol", it will return "prpl-aim")
 * Free the returned string when you're done with it 
 */
static char* getPrplProtocolIdFromServiceName(const char *serviceName)
{
	if (!serviceName)
	{
		return "";
	}
	GString *prplProtocolId = g_string_new("prpl-");
	if (strcmp(serviceName, "aol") == 0)
	{
		// Special case for aol where the java serviceName is "aol" and the prpl protocol_id is "prpl-aim"
		// I can imagine that we'll have more of these cases coming up...
		g_string_append(prplProtocolId, "aim");
	}
	else if (strcmp(serviceName, "gmail") == 0)
	{
		// Special case for gtalk where the java serviceName is "gmail" and the prpl protocol_id is "prpl-jabber"
		g_string_append(prplProtocolId, "jabber");
	}
	else if (strcmp(serviceName, "live") == 0)
	{
		// Special case for messenger where the java serviceName is "live" and the prpl protocol_id is "prpl-msn"
		g_string_append(prplProtocolId, "msn");
	}
	else if (strcmp(serviceName, "yahoo") == 0)
	{
		// Special case for messenger where the java serviceName is "yahoo" and the prpl protocol_id is "prpl-yahoo"
		g_string_append(prplProtocolId, "yahoo");
	}
	else if (strcmp(serviceName, "icq") == 0)
	{
		// Special case for icq where the java serviceName is "icq" and the prpl protocol_id is "prpl-icq"
		g_string_append(prplProtocolId, "icq");
	}
	else if (strcmp(serviceName, "xfire") == 0)
	{
		// Special case for xfire where the java serviceName is "xfire" and the prpl protocol_id is "prpl-xfire"
		g_string_append(prplProtocolId, "xfire");
	}
	else if (strcmp(serviceName, "facebook") == 0)
	{
		// Special case for facebook where the java serviceName is "facebook" and the prpl protocol_id is "prpl-bigbrownchunx-facebookim"
		g_string_append(prplProtocolId, "bigbrownchunx-facebookim");
	}
	else if (strcmp(serviceName, "jabber") == 0)
	{
		// Special case for jabber where the java serviceName is "jabber" and the prpl protocol_id is "prpl-jabber"
		g_string_append(prplProtocolId, "jabber");
	}
	else if (strcmp(serviceName, "sipe") == 0)
	{
		// Special case for sipe where the java serviceName is "sipe" and the prpl protocol_id is "prpl-sipe"
		g_string_append(prplProtocolId, "sipe");
	}
	else if (strcmp(serviceName, "lcs") == 0)
	{
		// Special case for sametime where the java serviceName is "lcs" and the prpl protocol_id is "prpl-meanwhile"
		g_string_append(prplProtocolId, "meanwhile");
	}
	else if (strcmp(serviceName, "gwim") == 0)
	{
		// Special case for Groupwise where the java serviceName is "gwim" and the prpl protocol_id is "prpl-novell"
		g_string_append(prplProtocolId, "novell");
	}
	else if (strcmp(serviceName, "myspace") == 0)
	{
		// Special case for MySpace where the java serviceName is "myspace" and the prpl protocol_id is "prpl-myspace"
		g_string_append(prplProtocolId, "myspace");
	}
	else if (strcmp(serviceName, "gadu") == 0)
	{
		// Special case for Gadu Gadu where the java serviceName is "gadu" and the prpl protocol_id is "prpl-gg"
		g_string_append(prplProtocolId, "gg");
	}
	else if (strcmp(serviceName, "qqim") == 0)
	{
		// Special case for QQ where the java serviceName is "qqim" and the prpl protocol_id is "prpl-qq"
		g_string_append(prplProtocolId, "qq");
	}
	else
	{
		g_string_append(prplProtocolId, serviceName);
	}
	char *prplProtocolIdToReturn = strdup(prplProtocolId->str);
	g_string_free(prplProtocolId, TRUE);
	return prplProtocolIdToReturn;
}

/*
 * Given the prpl-specific protocol_id, it will return java-friendly serviceName (e.g. given "prpl-aim", it will return "aol")
 * Free the returned string when you're done with it 
 */
static char* getServiceNameFromPrplProtocolId(PurpleAccount *prplaccount)
{
	char *prplProtocolId = prplaccount->protocol_id;
	if (!prplProtocolId)
	{
		return strdup("");
	}

	char *stringChopper = prplProtocolId;
	stringChopper += strlen("prpl-");
	GString *serviceName = g_string_new(stringChopper);

	if (strcmp(serviceName->str, "aim") == 0)
	{
		// Special case for aol where the java serviceName is "aol" and the prpl protocol_id is "prpl-aim"
		// I can imagine that we'll have more of these cases coming up...
		serviceName = g_string_new("aol");
	}
	else if (strcmp(serviceName->str, "jabber") == 0)
	{
		const char *Alias = purple_account_get_alias (prplaccount);

		if (Alias != NULL)
		{
			//Check account alias. gtalk for Gtalk, jabber for Jabber
			if (strcmp(Alias, "gtalk") == 0)
			{
				// Special case for gtalk where the java serviceName is "gmail" and the prpl protocol_id is "prpl-jabber"
				serviceName = g_string_new("gmail");
				printf("**************************** getServiceNameFromPrplProtocolId\n");
				printf("*************************************************************\n");
				printf("GTalk protocol returned.\n");
				printf("*************************************************************\n");
				printf("*************************************************************\n");
			}
			else
			{
				// Special case for jabber where the java serviceName is "jabber" and the prpl protocol_id is "prpl-jabber"
				serviceName = g_string_new("jabber");
				printf("**************************** getServiceNameFromPrplProtocolId\n");
				printf("*************************************************************\n");
				printf("Jabber protocol returned.\n");
				printf("*************************************************************\n");
				printf("*************************************************************\n");
			}
		}
		else
		{
			//Account alias is blank for some reason. Guess gtalk
			serviceName = g_string_new("gmail");
			printf("**************************** getServiceNameFromPrplProtocolId\n");
			printf("*************************************************************\n");
			printf("Alias Blank. GTalk protocol returned.\n");
			printf("*************************************************************\n");
			printf("*************************************************************\n");
		}
	}
	else if (strcmp(serviceName->str, "msn") == 0)
	{
		// Special case for messenger where the java serviceName is "live" and the prpl protocol_id is "prpl-msn"
		serviceName = g_string_new("live");
	}
	else if (strcmp(serviceName->str, "yahoo") == 0)
	{
		// Special case for yahoo where the java serviceName is "yahoo" and the prpl protocol_id is "prpl-yahoo"
		serviceName = g_string_new("yahoo");
	}
	else if (strcmp(serviceName->str, "icq") == 0)
	{
		// Special case for icq where the java serviceName is "icq" and the prpl protocol_id is "prpl-icq"
		serviceName = g_string_new("icq");
	}
	else if (strcmp(serviceName->str, "xfire") == 0)
	{
		// Special case for xfire where the java serviceName is "xfire" and the prpl protocol_id is "prpl-xfire"
		serviceName = g_string_new("xfire");
	}
	else if (strcmp(serviceName->str, "bigbrownchunx-facebookim") == 0)
	{
		// Special case for facebook where the java serviceName is "facebook" and the prpl protocol_id is "prpl-bigbrownchunx-facebookim"
		serviceName = g_string_new("facebook");
	}
	else if (strcmp(serviceName->str, "sipe") == 0)
	{
		// Special case for sipe where the java serviceName is "sipe" and the prpl protocol_id is "prpl-sipe"
		serviceName = g_string_new("sipe");
	}
	else if (strcmp(serviceName->str, "jabber") == 0)
	{
		// Special case for jabber where the java serviceName is "jabber" and the prpl protocol_id is "prpl-jabber"
		serviceName = g_string_new("jabber");
	}
	else if (strcmp(serviceName->str, "novell") == 0)
	{
		// Special case for Group Wise where the java serviceName is "gwim" and the prpl protocol_id is "prpl-novell"
		serviceName = g_string_new("gwim");
	}
	else if (strcmp(serviceName->str, "myspace") == 0)
	{
		// Special case for MySpace where the java serviceName is "myspace" and the prpl protocol_id is "prpl-myspace"
		serviceName = g_string_new("myspace");
	}
	else if (strcmp(serviceName->str, "gg") == 0)
	{
		// Special case for Gadu Gadu where the java serviceName is "gadu" and the prpl protocol_id is "prpl-gg"
		serviceName = g_string_new("gadu");
	}
	else if (strcmp(serviceName->str, "meanwhile") == 0)
	{
		// Special case for sametime where the java serviceName is "lcs" and the prpl protocol_id is "prpl-meanwhile"
		serviceName = g_string_new("lcs");
	}
	else if (strcmp(serviceName->str, "qq") == 0)
	{
		// Special case for QQ where the java serviceName is "qqim" and the prpl protocol_id is "prpl-qq"
		serviceName = g_string_new("qqim");
	}
	char *serviceNameToReturn = strdup(serviceName->str);
	g_string_free(serviceName, TRUE);
	return serviceNameToReturn;
}

static char* getAccountKey(const char *username, const char *serviceName)
{
	if (!username || !serviceName)
	{
		return strdup("");
	}
	char *accountKey = malloc(strlen(username) + strlen(serviceName) + 2);
	strcpy(accountKey, username);
	strcat(accountKey, "_");
	strcat(accountKey, serviceName);
	return accountKey;
}

static char* getAccountKeyFromPurpleAccount(PurpleAccount *account)
{
	if (!account)
	{
		return "";
	}
	char *serviceName = getServiceNameFromPrplProtocolId(account);
	char *username = getJavaFriendlyUsername(account->username, serviceName);
	char *accountKey = getAccountKey(username, serviceName);

	free(serviceName);
	free(username);

	return accountKey;
}

static const char* getField(struct json_object* message, const char* name)
{
	struct json_object* val = json_object_object_get(message, name);
	if (val)
	{
		return json_object_get_string(val);
	}
	return NULL;
}

/**
 * Returns a GString containing the special stanza to enable server-side presence update queue
 * Clean up after yourself using g_string_free when you're done with the return value
 */
static GString* getEnableQueueStanza(PurpleAccount *account)
{
	GString *stanza = NULL;
	if (account != NULL)
	{
		if (strcmp(account->protocol_id, "prpl-jabber") == 0)
		{
			stanza = g_string_new("");
			PurpleConnection *pc = purple_account_get_connection(account);
			if (pc == NULL)
			{
				return NULL;
			}
			const char *displayName = purple_connection_get_display_name(pc);
			if (displayName == NULL)
			{
				return NULL;
			}
			g_string_append(stanza, "<iq from='");
			g_string_append(stanza, displayName);
			g_string_append(stanza, "' type='set'><query xmlns='google:queue'><enable/></query></iq>");
		}
		else if (strcmp(account->protocol_id, "prpl-aim") == 0)
		{
			syslog(LOG_INFO, "getEnableQueueStanza for AIM");
			stanza = g_string_new("true");
		}
	}
	return stanza;
}

/**
 * Returns a GString containing the special stanza to disable and flush the server-side presence update queue
 * Clean up after yourself using g_string_free when you're done with the return value
 */
static GString* getDisableQueueStanza(PurpleAccount *account)
{
	GString *stanza = NULL;
	if (account != NULL)
	{
		if (strcmp(account->protocol_id, "prpl-jabber") == 0)
		{
			stanza = g_string_new("");
			PurpleConnection *pc = purple_account_get_connection(account);
			if (pc == NULL)
			{
				return NULL;
			}
			const char *displayName = purple_connection_get_display_name(pc);
			if (displayName == NULL)
			{
				return NULL;
			}
			g_string_append(stanza, "<iq from='");
			g_string_append(stanza, displayName);
			g_string_append(stanza, "' type='set'><query xmlns='google:queue'><disable/><flush/></query></iq>");
		}
		else if (strcmp(account->protocol_id, "prpl-aim") == 0)
		{
			syslog(LOG_INFO, "getDisableQueueStanza for AIM");
			stanza = g_string_new("false");
		}
	}
	return stanza;
}

static void enableServerQueueForAccount(PurpleAccount *account)
{
	if (!account)
	{
		return;
	}

	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePlugin *prpl = NULL;
	
	if (gc != NULL)
	{
		prpl = purple_connection_get_prpl(gc);
	}

	if (prpl != NULL)
	{
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	}

	if (prpl_info && prpl_info->send_raw)
	{
		GString *enableQueueStanza = getEnableQueueStanza(account);
		if (enableQueueStanza != NULL) 
		{
			syslog(LOG_INFO, "Enabling server queue for %s", account->protocol_id);
			prpl_info->send_raw(gc, enableQueueStanza->str, enableQueueStanza->len);
			g_string_free(enableQueueStanza, TRUE);
		}
	}
}

static void disableServerQueueForAccount(PurpleAccount *account)
{
	if (!account)
	{
		return;
	}
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePlugin *prpl = NULL;
	
	if (gc != NULL)
	{
		prpl = purple_connection_get_prpl(gc);
	}

	if (prpl != NULL)
	{
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	}

	if (prpl_info && prpl_info->send_raw)
	{
		GString *disableQueueStanza = getDisableQueueStanza(account);
		if (disableQueueStanza != NULL) 
		{
			syslog(LOG_INFO, "Disabling server queue");
			prpl_info->send_raw(gc, disableQueueStanza->str, disableQueueStanza->len);
			g_string_free(disableQueueStanza, TRUE);
		}
	}
}

/**
 * Asking the gtalk server to enable/disable queueing of presence updates 
 * This is called when the screen is turned off (enable:true) or turned on (enable:false)
 */
static bool queuePresenceUpdates(bool enable)
{
	PurpleAccount *account;
	GList *onlineAccountKeys = NULL;
	GList *iterator = NULL;
	char *accountKey = "";
	
	onlineAccountKeys = g_hash_table_get_keys(onlineAccountData);
	for (iterator = onlineAccountKeys; iterator != NULL; iterator = g_list_next(iterator))
	{
		accountKey = iterator->data;
		account = g_hash_table_lookup(onlineAccountData, accountKey);
		if (account)
		{
			/*
			 * enabling/disabling server queue is supported by gtalk (jabber) or aim
			 */
 			if ((strcmp(account->protocol_id, "prpl-jabber") == 0) ||
			    (strcmp(account->protocol_id, "prpl-aim") == 0))
 			{
				if (enable)
				{
					enableServerQueueForAccount(account);
				}
				else
				{
					disableServerQueueForAccount(account);
				}
 			}
		}
	}
	return TRUE;
}

static gboolean queuePresenceUpdatesTimer(gpointer data)
{
	if (currentDisplayState)
	{
		queuePresenceUpdates(FALSE);
	}
	return FALSE;
}

static gboolean queuePresenceUpdatesForAccountTimerCallback(gpointer data)
{
	/*
	 * if the display is still off, then enable the server queue for this account
	 */
	if (!currentDisplayState)
	{
		char *accountKey = data;
		PurpleAccount *account = g_hash_table_lookup(onlineAccountData, accountKey);
		if (account != NULL)
		{
			enableServerQueueForAccount(account);
		}
	}
	return FALSE;
}

static void ReadWriteContacts(const char *PluginName)
{
	//Set Filename
	char *filename = "/media/cryptofs/apps/usr/palm/applications/org.webosinternals.messaging/org.webosinternals.messaging.jar";
	char *DestDIR = "";
	FILE *hFile;

	printf("Checking for file %s\n",filename);
	hFile = fopen(filename, "r");
	if (hFile == NULL)
	{
		printf("File not found. Defaulting to /var/usr/palm/applications/org.webosinternals.messaging/org.webosinternals.messaging.jar");
		DestDIR = "/var/usr/palm/applications/org.webosinternals.messaging/";
	}
	else
	{
		printf("File found. Defaulting to /media/cryptofs/apps/usr/palm/applications/org.webosinternals.messaging/org.webosinternals.messaging.jar");
		fclose(hFile);
		DestDIR = "/media/cryptofs/apps/usr/palm/applications/org.webosinternals.messaging/";
	}

	//Set Command Line
	char *CommandLine = alloca(strlen("javahy -bcp /usr/lib/luna/java/Utils.jar:/usr/lib/luna/java/accounts.jar:/usr/lib/luna/java/accountservices.jar:/usr/lib/luna/java/activerecord.jar:/usr/lib/luna/java/json.jar:/usr/lib/luna/java/sqlitejdbc-v053.jar:") + strlen(DestDIR) + strlen("org.webosinternals.messaging.jar org.webosinternals.messaging.Main ") + strlen(" EnableContactsReadWrite ") + strlen("\"") + strlen(PluginName) + strlen("\""));
	strcpy(CommandLine, "javahy -bcp /usr/lib/luna/java/Utils.jar:/usr/lib/luna/java/accounts.jar:/usr/lib/luna/java/accountservices.jar:/usr/lib/luna/java/activerecord.jar:/usr/lib/luna/java/json.jar:/usr/lib/luna/java/sqlitejdbc-v053.jar:");
	strcat(CommandLine, DestDIR);
	strcat(CommandLine, "org.webosinternals.messaging.jar org.webosinternals.messaging.Main ");
	strcat(CommandLine, " EnableContactsReadWrite ");
	strcat(CommandLine, "\'");
	strcat(CommandLine, PluginName);
	strcat(CommandLine, "\'");

	//Run Java to enable r/w on contacts
	printf("\n");
	printf("=================================\n");
	printf("Running command: %s",CommandLine);
	printf("=================================\n");
	printf("\n");
	system(CommandLine);
}

static void respondWithFullBuddyList(PurpleAccount *account, char *serviceName, char *myJavaFriendlyUsername)
{
	if (!account || !myJavaFriendlyUsername || !serviceName)
	{
		syslog(LOG_INFO, "ERROR: respondWithFullBuddyList was passed NULL");
		return;
	}
	GSList *buddyList = purple_find_buddies(account, NULL);
	if (!buddyList)
	{
		syslog(LOG_INFO, "ERROR: the buddy list was NULL");
	}

	GString *jsonResponse = g_string_new("{\"serviceName\":\"");
	g_string_append(jsonResponse, serviceName);
	g_string_append(jsonResponse, "\", \"username\":\"");
	g_string_append(jsonResponse, myJavaFriendlyUsername);
	g_string_append(jsonResponse, "\", \"fullBuddyList\":true, \"buddies\":[");

	GSList *buddyIterator = NULL;
	PurpleBuddy *buddyToBeAdded = NULL;
	PurpleGroup *group = NULL;

	printf("\n\n\n\n\n\n ---------------- BUDDY LIST SIZE: %d----------------- \n\n\n\n\n\n", g_slist_length(buddyList));

	bool firstItem = TRUE;

	for (buddyIterator = buddyList; buddyIterator != NULL; buddyIterator = buddyIterator->next)
	{
		buddyToBeAdded = (PurpleBuddy *) buddyIterator->data;
		group = purple_buddy_get_group(buddyToBeAdded);
		const char *groupName = purple_group_get_name(group);

		if (getpluginpreference(serviceName,"Alias"))
		{
			if (buddyToBeAdded->server_alias == NULL)
			{
				buddyToBeAdded->server_alias = "";
			}
		}
		else
		{
			if (buddyToBeAdded->alias == NULL)
			{
				buddyToBeAdded->alias = "";
			}
		}

		/*
		 * Getting the availability
		 */
		PurpleStatus *status = purple_presence_get_active_status(purple_buddy_get_presence(buddyToBeAdded));
		int newStatusPrimitive = purple_status_type_get_primitive(purple_status_get_type(status));
		int availabilityInt = getPalmAvailabilityFromPrplAvailability(newStatusPrimitive);
		char availabilityString[2];
		sprintf(availabilityString, "%i", availabilityInt);

		/*
		 * Getting the custom message
		 */
		const char *customMessage = purple_status_get_attr_string(status, "message");
		if (customMessage == NULL)
		{
			customMessage = "";
		}

		/*
		 * Getting the avatar location
		 */
		PurpleBuddyIcon *icon = purple_buddy_get_icon(buddyToBeAdded);
		char* buddyAvatarLocation = NULL;
		if (getpluginpreference(serviceName,"Avatar"))
		{
			if (icon != NULL)
			{
				buddyAvatarLocation = purple_buddy_icon_get_full_path(icon);
			}
		}
		else
		{
			buddyAvatarLocation = "";
		}
			
		if (buddyAvatarLocation == NULL)
		{
			buddyAvatarLocation = "";
		}

		if (!firstItem)
		{
			g_string_append(jsonResponse, ", ");
		}
		else
		{
			firstItem = FALSE;
		}

		struct json_object *payload = json_object_new_object();

		//Special for Office Communicator. (Remove 'sip:' prefix)
		if (strcmp(serviceName, "sipe") == 0)
		{
			if (strstr(buddyToBeAdded->name, "sip:") != NULL || strstr(buddyToBeAdded->alias, "sip:") != NULL)
			{
				if (strstr(buddyToBeAdded->alias, "sip:") != NULL)
				{
					GString *SIPBuddyAlias = g_string_new(buddyToBeAdded->alias);
					g_string_erase(SIPBuddyAlias, 0, 4);
					json_object_object_add(payload, "displayName", json_object_new_string(SIPBuddyAlias->str));
				}
				else
				{
					json_object_object_add(payload, "displayName", json_object_new_string(buddyToBeAdded->alias));
				}
				if (strstr(buddyToBeAdded->name, "sip:") != NULL)
				{
					GString *SIPBuddyName = g_string_new(buddyToBeAdded->name);
					g_string_erase(SIPBuddyName, 0, 4);
					json_object_object_add(payload, "buddyUsername", json_object_new_string(SIPBuddyName->str));
				}
				else
				{
					json_object_object_add(payload, "buddyUsername", json_object_new_string(buddyToBeAdded->name));
				}
			}
			else
			{
				json_object_object_add(payload, "buddyUsername", json_object_new_string(buddyToBeAdded->name));
				json_object_object_add(payload, "displayName", json_object_new_string(buddyToBeAdded->alias));
			}
		}
		else
		{
				json_object_object_add(payload, "buddyUsername", json_object_new_string(buddyToBeAdded->name));

				//Set display alias?
				if (getpluginpreference(serviceName,"Alias"))
				{
					json_object_object_add(payload, "displayName", json_object_new_string(buddyToBeAdded->server_alias));
				}
				else
				{
					//Set Palm Default Alias (Must use this for Gtalk otherwise it will crash)
					json_object_object_add(payload, "displayName", json_object_new_string(buddyToBeAdded->alias));
				}
		}

		json_object_object_add(payload, "avatarLocation", json_object_new_string((buddyAvatarLocation) ? buddyAvatarLocation : ""));
		json_object_object_add(payload, "customMessage", json_object_new_string((char*)customMessage));
		json_object_object_add(payload, "availability", json_object_new_string(availabilityString));
		json_object_object_add(payload, "groupName", json_object_new_string((char*)groupName));
		g_string_append(jsonResponse, json_object_to_json_string(payload));
		g_message(
				"%s says: %s's presence: availability: '%s', custom message: '%s', avatar location: '%s', display name: '%s', group name:'%s'",
				__FUNCTION__, buddyToBeAdded->name, availabilityString, customMessage, buddyAvatarLocation,
				buddyToBeAdded->alias, groupName);
		if (!is_error(payload)) 
		{
			json_object_put(payload);
		}
	}
	g_string_append(jsonResponse, "]}");
	LSError lserror;
	LSErrorInit(&lserror);
	bool retVal = LSSubscriptionReply(serviceHandle, "/getBuddyList", jsonResponse->str, &lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}

	LSErrorFree(&lserror);
	g_string_free(jsonResponse, TRUE);
}

/*
 * End of helper methods 
 */

/*
 * Callbacks
 */

static void buddy_signed_on_off_cb(PurpleBuddy *buddy, gpointer data)
{
	LSError lserror;
	LSErrorInit(&lserror);

	PurpleAccount *account = purple_buddy_get_account(buddy);
	char *serviceName = getServiceNameFromPrplProtocolId(account);
	const char *myUsername = purple_account_get_username(account);
	char *myJavaFriendlyUsername = getJavaFriendlyUsername(myUsername, serviceName);
	PurpleStatus *activeStatus = purple_presence_get_active_status(purple_buddy_get_presence(buddy));
	/*
	 * Getting the new availability
	 */
	int newStatusPrimitive = purple_status_type_get_primitive(purple_status_get_type(activeStatus));
	int newAvailabilityValue = getPalmAvailabilityFromPrplAvailability(newStatusPrimitive);
	char availabilityString[2];
	PurpleBuddyIcon *icon = purple_buddy_get_icon(buddy);
	const char *customMessage = "";
	char* buddyAvatarLocation = NULL;

	sprintf(availabilityString, "%i", newAvailabilityValue);


	if (getpluginpreference(serviceName,"Avatar"))
	{
		if (icon != NULL)
		{
			buddyAvatarLocation = purple_buddy_icon_get_full_path(icon);
		}
	}
	else
	{
		buddyAvatarLocation = "";
	}
		
	if (buddyAvatarLocation == NULL)
	{
		buddyAvatarLocation = "";
	}

	if (buddy->name == NULL)
	{
		buddy->name = "";
	}
	
	if (getpluginpreference(serviceName,"Alias"))
	{
		if (buddy->server_alias == NULL)
		{
			buddy->server_alias = "";
		}
	}
	else
	{
		if (buddy->alias == NULL)
		{
			buddy->alias = "";
		}
	}

	customMessage = purple_status_get_attr_string(activeStatus, "message");
	if (customMessage == NULL)
	{
		customMessage = "";
	}

	PurpleGroup *group = purple_buddy_get_group(buddy);
	const char *groupName = purple_group_get_name(group);
	if (groupName == NULL)
	{
		groupName = "";
	}
	
	struct json_object *payload = json_object_new_object();
	json_object_object_add(payload, "serviceName", json_object_new_string(serviceName));
	json_object_object_add(payload, "username", json_object_new_string(myJavaFriendlyUsername));

	//Special for Office Communicator. (Remove 'sip:' prefix)
	if (strcmp(serviceName, "sipe") == 0)
	{
		if (strstr(buddy->name, "sip:") != NULL || strstr(buddy->alias, "sip:") != NULL)
		{
			if (strstr(buddy->alias, "sip:") != NULL)
			{
				GString *SIPBuddyAlias = g_string_new(buddy->alias);
				g_string_erase(SIPBuddyAlias, 0, 4);
				json_object_object_add(payload, "displayName", json_object_new_string(SIPBuddyAlias->str));
			}
			else
			{
				json_object_object_add(payload, "displayName", json_object_new_string(buddy->alias));
			}
			if (strstr(buddy->name, "sip:") != NULL)
			{
				GString *SIPBuddyName = g_string_new(buddy->name);
				g_string_erase(SIPBuddyName, 0, 4);
				json_object_object_add(payload, "buddyUsername", json_object_new_string(SIPBuddyName->str));
			}
			else
			{
				json_object_object_add(payload, "buddyUsername", json_object_new_string(buddy->name));
			}
		}
		else
		{
			json_object_object_add(payload, "buddyUsername", json_object_new_string(buddy->name));
			json_object_object_add(payload, "displayName", json_object_new_string(buddy->alias));
		}
	}
	else
	{
			json_object_object_add(payload, "buddyUsername", json_object_new_string(buddy->name));

			//Set Alias?
			if (getpluginpreference(serviceName,"Alias"))
			{
				json_object_object_add(payload, "displayName", json_object_new_string(buddy->server_alias));
			}
			else
			{
				json_object_object_add(payload, "displayName", json_object_new_string(buddy->alias));
			}
	}

	json_object_object_add(payload, "avatarLocation", json_object_new_string((buddyAvatarLocation) ? buddyAvatarLocation : ""));
	json_object_object_add(payload, "customMessage", json_object_new_string((char*)customMessage));
	json_object_object_add(payload, "availability", json_object_new_string(availabilityString));
	json_object_object_add(payload, "groupName", json_object_new_string((char*)groupName));

	bool retVal = LSSubscriptionReply(serviceHandle, "/getBuddyList", json_object_to_json_string(payload), &lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	
	g_message(
			"%s says: %s's presence: availability: '%s', custom message: '%s', avatar location: '%s', display name: '%s', group name: '%s'",
			__FUNCTION__, buddy->name, availabilityString, customMessage, buddyAvatarLocation, buddy->alias, groupName);
	
	free(serviceName);
	if (myJavaFriendlyUsername)
	{
		free(myJavaFriendlyUsername);
	}
	if (!is_error(payload)) 
	{
		json_object_put(payload);
	}	
}

static void buddy_status_changed_cb(PurpleBuddy *buddy, PurpleStatus *old_status, PurpleStatus *new_status,
		gpointer unused)
{
	/*
	 * Getting the new availability
	 */
	int newStatusPrimitive = purple_status_type_get_primitive(purple_status_get_type(new_status));
	int newAvailabilityValue = getPalmAvailabilityFromPrplAvailability(newStatusPrimitive);
	char availabilityString[2];
	sprintf(availabilityString, "%i", newAvailabilityValue);

	/*
	 * Getting the new custom message
	 */
	const char *customMessage = purple_status_get_attr_string(new_status, "message");
	if (customMessage == NULL)
	{
		customMessage = "";
	}

	LSError lserror;
	LSErrorInit(&lserror);

	PurpleAccount *account = purple_buddy_get_account(buddy);
	char *serviceName = getServiceNameFromPrplProtocolId(account);
	char *username = getJavaFriendlyUsername(account->username, serviceName);

	PurpleBuddyIcon *icon = purple_buddy_get_icon(buddy);
	char* buddyAvatarLocation = NULL;	
	if (getpluginpreference(serviceName,"Avatar"))
	{
		if (icon != NULL)
		{
			buddyAvatarLocation = purple_buddy_icon_get_full_path(icon);
		}
	}
	else
	{
		buddyAvatarLocation = "";
	}
		
	if (buddyAvatarLocation == NULL)
	{
		buddyAvatarLocation = "";
	}

	PurpleGroup *group = purple_buddy_get_group(buddy);
	const char *groupName = purple_group_get_name(group);
	if (groupName == NULL)
	{
		groupName = "";
	}

	char *buddyName = buddy->name;
	if (buddyName == NULL)
	{
		buddyName = "";
	}

	struct json_object *payload = json_object_new_object();
	json_object_object_add(payload, "serviceName", json_object_new_string(serviceName));
	json_object_object_add(payload, "username", json_object_new_string(username));
	
	//Special for Office Communicator. (Remove 'sip:' prefix)
	if (strcmp(serviceName, "sipe") == 0)
	{
		if (strstr(buddyName, "sip:") != NULL)
		{
			GString *SIPBuddyName = g_string_new(buddyName);
			g_string_erase(SIPBuddyName, 0, 4);
			json_object_object_add(payload, "buddyUsername", json_object_new_string(SIPBuddyName->str));
		}
		else
		{
			json_object_object_add(payload, "buddyUsername", json_object_new_string(buddyName));
		}
	}
	else
	{
		json_object_object_add(payload, "buddyUsername", json_object_new_string(buddyName));
	}

	json_object_object_add(payload, "avatarLocation", json_object_new_string((buddyAvatarLocation) ? buddyAvatarLocation : ""));
	json_object_object_add(payload, "customMessage", json_object_new_string((char*)customMessage));
	json_object_object_add(payload, "availability", json_object_new_string(availabilityString));
	json_object_object_add(payload, "groupName", json_object_new_string((char*)groupName));

	bool retVal = LSSubscriptionReply(serviceHandle, "/getBuddyList", json_object_to_json_string(payload), &lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	
	g_message(
			"%s says: %s's presence: availability: '%s', custom message: '%s', avatar location: '%s', display name: '%s', group name: '%s'",
			__FUNCTION__, buddy->name, availabilityString, customMessage, buddyAvatarLocation, buddy->alias, groupName);
	
	if (serviceName)
	{
		free(serviceName);
	}
	if (username)
	{
		free(username);
	}
	if (!is_error(payload)) 
	{
		json_object_put(payload);
	}
}

static void buddy_avatar_changed_cb(PurpleBuddy *buddy)
{
	PurpleStatus *activeStatus = purple_presence_get_active_status(purple_buddy_get_presence(buddy));
	buddy_status_changed_cb(buddy, activeStatus, activeStatus, NULL);
}

static bool displayEventHandler(LSHandle *sh , LSMessage *message, void *ctx)
{
    const char *payload = LSMessageGetPayload(message);
    struct json_object *params = json_tokener_parse(payload);
    if (is_error(params)) goto end;
    
    const char *displayState = NULL;
    bool newDisplayState = currentDisplayState;
    bool returnValue = json_object_get_boolean(json_object_object_get(params, "returnValue"));
    
    if (returnValue)
    {
    	displayState = getField(params, "state");
		if (displayState && strcmp(displayState, "on") == 0)
		{
			newDisplayState = TRUE;
		}
		else if (displayState && strcmp(displayState, "off") == 0)
		{
			newDisplayState = FALSE;
		}
		else
		{
			displayState = getField(params, "event");
			if (displayState && strcmp(displayState, "displayOn") == 0)
			{
				newDisplayState = TRUE;
			}
			else if (displayState && strcmp(displayState, "displayOff") == 0)
			{
				newDisplayState = FALSE;
			}
			else
			{
				goto end;
			}
		}
		if (newDisplayState != currentDisplayState)
		{
			currentDisplayState = newDisplayState;
			if (currentDisplayState)
			{
				/*
				 * display has turned on, therefore we disable and flush the queue (after DISABLE_QUEUE_TIMEOUT_SECONDS seconds for perf reasons)
				 */
				purple_timeout_add_seconds(DISABLE_QUEUE_TIMEOUT_SECONDS, queuePresenceUpdatesTimer, NULL);
			}
			else
			{
				/*
				 * display has turned off, therefore we enable the queue
				 */
				queuePresenceUpdates(TRUE);
			}
		}
    }
    else 
    {
    	currentDisplayState = TRUE;
    	registeredForDisplayEvents = FALSE;
    	queuePresenceUpdates(FALSE);
    }

end:
    if (!is_error(params)) json_object_put(params);
    return TRUE;
}

static void account_logged_in(PurpleConnection *gc, gpointer unused)
{
	void *blist_handle = purple_blist_get_handle();
	static int handle;

	PurpleAccount *loggedInAccount = purple_connection_get_account(gc);
	g_return_if_fail(loggedInAccount != NULL);

	char *accountKey = getAccountKeyFromPurpleAccount(loggedInAccount);

	if (g_hash_table_lookup(onlineAccountData, accountKey) != NULL)
	{
		//TODO: we were online. why are we getting notified that we're connected again? we were never disconnected.
		return;
	}

	/*
	 * cancel the connect timeout for this account
	 */
	guint timerHandle = (guint)g_hash_table_lookup(accountLoginTimers, accountKey);

	purple_timeout_remove(timerHandle);
	g_hash_table_remove(accountLoginTimers, accountKey);

	g_hash_table_insert(onlineAccountData, accountKey, loggedInAccount);
	g_hash_table_remove(pendingAccountData, accountKey);

	syslog(LOG_INFO, "Account connected...");

	char *serviceName = getServiceNameFromPrplProtocolId(loggedInAccount);
	char *myJavaFriendlyUsername = getJavaFriendlyUsername(loggedInAccount->username, serviceName);

	GString *jsonResponse = g_string_new("{\"serviceName\":\"");
	g_string_append(jsonResponse, serviceName);
	g_string_append(jsonResponse, "\",  \"username\":\"");
	g_string_append(jsonResponse, myJavaFriendlyUsername);
	g_string_append(jsonResponse, "\", \"returnValue\":true}");

	LSError lserror;
	LSErrorInit(&lserror);

	LSMessage *message = g_hash_table_lookup(loginMessages, getAccountKeyFromPurpleAccount(loggedInAccount));
	bool retVal = LSMessageReply(serviceHandle, message, jsonResponse->str, &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}

	if (registeredForPresenceUpdateSignals == FALSE)
	{
		purple_signal_connect(blist_handle, "buddy-status-changed", &handle, PURPLE_CALLBACK(buddy_status_changed_cb),
				NULL);
		purple_signal_connect(blist_handle, "buddy-signed-on", &handle, PURPLE_CALLBACK(buddy_signed_on_off_cb),
				GINT_TO_POINTER(TRUE));
		purple_signal_connect(blist_handle, "buddy-signed-off", &handle, PURPLE_CALLBACK(buddy_signed_on_off_cb),
				GINT_TO_POINTER(FALSE));
		purple_signal_connect(blist_handle, "buddy-icon-changed", &handle, PURPLE_CALLBACK(buddy_avatar_changed_cb),
				GINT_TO_POINTER(FALSE));
		registeredForPresenceUpdateSignals = TRUE;
	}
	
	if (registeredForDisplayEvents == FALSE)
	{
		retVal = LSCall(serviceHandle, "luna://com.palm.display/control/status",
		        "{\"subscribe\":true}", displayEventHandler, NULL, NULL, &lserror);
		if (!retVal) goto error;
		registeredForDisplayEvents = TRUE;
	}
	else
	{
		/*
		 * This account has just been logged in. We should enable queuing of presence updates 
		 * if the screen is off, but not until we get the initial presence updates
		 */
		if (currentDisplayState == FALSE)
		{
			guint handle = purple_timeout_add_seconds(POST_LOGIN_WAIT_SECONDS, queuePresenceUpdatesForAccountTimerCallback, accountKey);
			if (!handle)
			{
				syslog(LOG_INFO, "purple_timeout_add_seconds failed in account_logged_in");
			}
		}
	}
	
error:
	LSErrorFree(&lserror);
	g_string_free(jsonResponse, TRUE);
}

static void account_signed_off_cb(PurpleConnection *gc, void *data)
{
	syslog(LOG_INFO, "account_signed_off_cb");

	PurpleAccount *account = purple_connection_get_account(gc);
	g_return_if_fail(account != NULL);

	char *accountKey = getAccountKeyFromPurpleAccount(account);
	if (g_hash_table_lookup(onlineAccountData, accountKey) != NULL)
	{
		g_hash_table_remove(onlineAccountData, accountKey);
	}
	else if (g_hash_table_lookup(pendingAccountData, accountKey) != NULL)
	{
		g_hash_table_remove(pendingAccountData, accountKey);
	}
	else
	{
		return;
	}
	g_hash_table_remove(ipAddressesBoundTo, accountKey);
	//g_hash_table_remove(connectionTypeData, accountKey);

	syslog(LOG_INFO, "Account disconnected...");

	if (g_hash_table_lookup(offlineAccountData, accountKey) == NULL)
	{
		/*
		 * Keep the PurpleAccount struct to reuse in future logins
		 */
		g_hash_table_insert(offlineAccountData, accountKey, account);
	}
	
	LSMessage *message = g_hash_table_lookup(logoutMessages, accountKey);
	if (message != NULL)
	{
		char *serviceName = getServiceNameFromPrplProtocolId(account);
		char *myJavaFriendlyUsername = getJavaFriendlyUsername(account->username, serviceName);

		GString *jsonResponse = g_string_new("{\"serviceName\":\"");
		g_string_append(jsonResponse, serviceName);
		g_string_append(jsonResponse, "\",  \"username\":\"");
		g_string_append(jsonResponse, myJavaFriendlyUsername);
		g_string_append(jsonResponse, "\", \"returnValue\":true}");

		LSError lserror;
		LSErrorInit(&lserror);

		bool retVal = LSMessageReply(serviceHandle, message, jsonResponse->str, &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		g_hash_table_remove(logoutMessages, accountKey);
		LSMessageUnref(message);
		LSErrorFree(&lserror);
		g_string_free(jsonResponse, TRUE);
	}
}

/*
 * This callback is called if a) the login attempt failed, or b) login was successful but the session was closed 
 * (e.g. connection problems, etc).
 */
static void account_login_failed(PurpleConnection *gc, PurpleConnectionError type, const gchar *description,
		gpointer unused)
{
	syslog(LOG_INFO, "account_login_failed is called with description %s", description);

	PurpleAccount *account = purple_connection_get_account(gc);
	g_return_if_fail(account != NULL);

	gboolean loggedOut = FALSE;
	char *accountKey = getAccountKeyFromPurpleAccount(account);
	if (g_hash_table_lookup(onlineAccountData, accountKey) != NULL)
	{
		/* 
		 * We were online on this account and are now disconnected because either a) the data connection is dropped, 
		 * b) the server is down, or c) the user has logged in from a different location and forced this session to
		 * get closed.
		 */
		loggedOut = TRUE;
	}
	else
	{
		/*
		 * cancel the connect timeout for this account
		 */
		guint timerHandle = (guint)g_hash_table_lookup(accountLoginTimers, accountKey);
		purple_timeout_remove(timerHandle);
		g_hash_table_remove(accountLoginTimers, accountKey);

		if (g_hash_table_lookup(pendingAccountData, accountKey) == NULL)
		{
			/*
			 * This account was in neither of the account data lists (online or pending). We must have logged it out 
			 * and not cared about letting java know about it (probably because java went down and came back up and 
			 * thought that the account was logged out anyways)
			 */
			return;
		}
		else
		{
			g_hash_table_remove(pendingAccountData, accountKey);
		}
	}

	char *serviceName = getServiceNameFromPrplProtocolId(account);
	char *myJavaFriendlyUsername = getJavaFriendlyUsername(account->username, serviceName);
	char *javaFriendlyErrorCode = getJavaFriendlyErrorCode(type);
	char *accountBoundToIpAddress = g_hash_table_lookup(ipAddressesBoundTo, accountKey);
	char *connectionType = g_hash_table_lookup(connectionTypeData, accountKey);

	if (accountBoundToIpAddress == NULL)
	{
		accountBoundToIpAddress = "";
	}

	if (connectionType == NULL)
	{
		connectionType = "";
	}

	char *escapedDescription = g_strescape(description, NULL);
	
	GString *jsonResponse = g_string_new("{\"serviceName\":\"");
	g_string_append(jsonResponse, serviceName);
	g_string_append(jsonResponse, "\",  \"username\":\"");
	g_string_append(jsonResponse, myJavaFriendlyUsername);
	g_string_append(jsonResponse, "\", \"returnValue\":false, \"errorCode\":\"");
	g_string_append(jsonResponse, javaFriendlyErrorCode);
	g_string_append(jsonResponse, "\",  \"localIpAddress\":\"");
	g_string_append(jsonResponse, accountBoundToIpAddress);
	g_string_append(jsonResponse, "\", \"errorText\":\"");
	g_string_append(jsonResponse, escapedDescription);
	if (loggedOut)
	{
		g_string_append(jsonResponse, "\", \"connectionStatus\":\"loggedOut\", \"connectionType\":\"");
		g_string_append(jsonResponse, connectionType);
		g_string_append(jsonResponse, "\"}");
		syslog(LOG_INFO, "We were logged out. Reason: %s, prpl error code: %i", description, type);
	}
	else
	{
		g_string_append(jsonResponse, "\", \"connectionType\":\"");
		g_string_append(jsonResponse, connectionType);
		g_string_append(jsonResponse, "\"}");
		syslog(LOG_INFO, "Login failed. Reason: \"%s\", prpl error code: %i", description, type);
	}
	g_hash_table_remove(onlineAccountData, accountKey);
	g_hash_table_remove(ipAddressesBoundTo, accountKey);
	g_hash_table_remove(connectionTypeData, accountKey);
 
	if (g_hash_table_lookup(offlineAccountData, accountKey) == NULL)
	{
		/*
		 * Keep the PurpleAccount struct to reuse in future logins
		 */
		g_hash_table_insert(offlineAccountData, accountKey, account);
	}
	
	LSError lserror;
	LSErrorInit(&lserror);

	LSMessage *message = g_hash_table_lookup(loginMessages, accountKey);
	if (message != NULL)
	{
		bool retVal = LSMessageReply(serviceHandle, message, jsonResponse->str, &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		g_hash_table_remove(loginMessages, accountKey);
		LSMessageUnref(message);
	}
	LSErrorFree(&lserror);
	g_string_free(jsonResponse, TRUE);
	if (escapedDescription)
	{
		g_free(escapedDescription);
	}
}

static void account_status_changed(PurpleAccount *account, PurpleStatus *old, PurpleStatus *new, gpointer data)
{
	printf("\n\n ACCOUNT STATUS CHANGED \n\n");
}

static void incoming_message_cb(PurpleConversation *conv, const char *who, const char *alias, const char *message,
		PurpleMessageFlags flags, time_t mtime)
{
	/*
	 * snippet taken from nullclient
	 */
	const char *usernameFrom;
	if (who && *who)
		usernameFrom = who;
	else if (alias && *alias)
		usernameFrom = alias;
	else
		usernameFrom = "";

	if ((flags & PURPLE_MESSAGE_RECV) != PURPLE_MESSAGE_RECV)
	{
		/* this is a sent message. ignore it. */
		return;
	}

	PurpleAccount *account = purple_conversation_get_account(conv);

	char *serviceName = getServiceNameFromPrplProtocolId(account);
	char *username = getJavaFriendlyUsername(account->username, serviceName);

	if (strcmp(username, usernameFrom) == 0)
	{
		/* We get notified even though we sent the message. Just ignore it */
		free(serviceName);
		free(username);
		return;
	}

	if (strcmp(serviceName, "aol") == 0 && (strcmp(usernameFrom, "aolsystemmsg") == 0 || strcmp(usernameFrom,
			"AOL System Msg") == 0))
	{
		/*
		 * ignore messages from the annoying aolsystemmsg telling us that we're logged in somewhere else
		 */
		free(serviceName);
		free(username);
		return;
	}

	//If IRC get the title of the chat window becuase group chat messages appear from the user
	if (strcmp(serviceName, "irc") == 0)
	{
		char *usernameFromStripped = (char *)usernameFrom;

		//Get Window Title
		char *IRCWindow = (char *)calloc(strlen(purple_conversation_get_title(conv)), sizeof(char));
		strcat(IRCWindow, purple_conversation_get_title(conv));

		//If this is a group chat e.g. #webos-internals append users name to message
		char *IRCMessage = NULL;
		if (strstr(IRCWindow, "#") != NULL)
		{
			//Append the users name to the message
			IRCMessage = (char *)calloc(strlen(usernameFromStripped) + strlen(" - ") + strlen(message), sizeof(char));
			strcat(IRCMessage, usernameFromStripped);
			strcat(IRCMessage, " - ");
			strcat(IRCMessage, message);
		}

		LSError lserror;
		LSErrorInit(&lserror);

		struct json_object *payload = json_object_new_object();
		json_object_object_add(payload, "serviceName", json_object_new_string(serviceName));
		json_object_object_add(payload, "username", json_object_new_string(username));
		json_object_object_add(payload, "usernameFrom", json_object_new_string(IRCWindow));

		if (strstr(IRCWindow, "#") != NULL)
		{
			json_object_object_add(payload, "messageText", json_object_new_string((char *)IRCMessage));
		}
		else
		{
			json_object_object_add(payload, "messageText", json_object_new_string((char *)message));
		}

		bool retVal = LSSubscriptionReply(serviceHandle, "/registerForIncomingMessages",
				json_object_to_json_string(payload), &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		LSErrorFree(&lserror);

		if (usernameFromStripped)
		{
			free(usernameFromStripped);
		}
		if (IRCMessage)
		{
			free(IRCMessage);
		}
		if (IRCWindow)
		{
			free(IRCWindow);
		}
		if (!is_error(payload)) 
		{
			json_object_put(payload);
		}
	}
	else
	{
		char *usernameFromStripped = NULL;
		if (strcmp(serviceName, "sipe") == 0)
		{
			//If sipe remove sip: from the start of the username
			GString *SIPEusernameFrom = g_string_new(usernameFrom);
			g_string_erase(SIPEusernameFrom, 0, 4);

			usernameFromStripped = (char *)SIPEusernameFrom->str;
		}
		else
		{
			usernameFromStripped = stripResourceFromGtalkUsername(usernameFrom);
		}

		LSError lserror;
		LSErrorInit(&lserror);

		struct json_object *payload = json_object_new_object();
		json_object_object_add(payload, "serviceName", json_object_new_string(serviceName));
		json_object_object_add(payload, "username", json_object_new_string(username));
		json_object_object_add(payload, "usernameFrom", json_object_new_string(usernameFromStripped));
		json_object_object_add(payload, "messageText", json_object_new_string((char*)message));

		bool retVal = LSSubscriptionReply(serviceHandle, "/registerForIncomingMessages",
				json_object_to_json_string(payload), &lserror);

		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}

		LSErrorFree(&lserror);

		if (usernameFromStripped)
		{
			free(usernameFromStripped);
		}
		if (!is_error(payload)) 
		{
			json_object_put(payload);
		}
	}

	if (serviceName)
	{
		free(serviceName);
	}
	if (username)
	{
		free(username);
	}
}

static gboolean connectTimeoutCallback(gpointer data)
{
	char *accountKey = data;
	PurpleAccount *account = g_hash_table_lookup(pendingAccountData, accountKey);
	if (account == NULL)
	{
		/*
		 * If the account is not pending anymore (which means login either already failed or succeeded) 
		 * then we shouldn't have gotten to this point since we should have cancelled the timer
		 */
		syslog(LOG_INFO,
				"WARNING: we shouldn't have gotten to connectTimeoutCallback since login had already failed/succeeded");
		return FALSE;
	}

	/*
	 * abort logging in since our connect timeout has hit before login either failed or succeeded
	 */
	g_hash_table_remove(accountLoginTimers, accountKey);
	g_hash_table_remove(pendingAccountData, accountKey);
	g_hash_table_remove(ipAddressesBoundTo, accountKey);

	purple_account_disconnect(account);

	char *serviceName = getServiceNameFromPrplProtocolId(account);
	char *username = getJavaFriendlyUsername(account->username, serviceName);
	char *connectionType = g_hash_table_lookup(connectionTypeData, accountKey);
	if (connectionType == NULL)
	{
		connectionType = "";
	}

	GString *jsonResponse = g_string_new("{\"serviceName\":\"");
	g_string_append(jsonResponse, serviceName);
	g_string_append(jsonResponse, "\",  \"username\":\"");
	g_string_append(jsonResponse, username);
	g_string_append(
			jsonResponse,
			"\", \"returnValue\":false, \"errorCode\":\"AcctMgr_Network_Error\", \"errorText\":\"Connection timed out\", \"connectionType\":\"");
	g_string_append(jsonResponse, connectionType);
	g_string_append(jsonResponse, "\"}");

	LSError lserror;
	LSErrorInit(&lserror);

	LSMessage *message = g_hash_table_lookup(loginMessages, accountKey);
	if (message != NULL)
	{
		bool retVal = LSMessageReply(serviceHandle, message, jsonResponse->str, &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		g_hash_table_remove(loginMessages, accountKey);
		LSMessageUnref(message);
	}
	LSErrorFree(&lserror);
	free(serviceName);
	free(username);
	free(accountKey);
	g_string_free(jsonResponse, TRUE);
	return FALSE;
}

/*
 * End of callbacks
 */

/*
 * libpurple initialization methods
 */

static GHashTable* getClientInfo(void)
{
	GHashTable *clientInfo = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	g_hash_table_insert(clientInfo, "name", "Palm Messaging");
	g_hash_table_insert(clientInfo, "version", "");

	return clientInfo;
}

static void initializeLibpurple()
{
	signal(SIGCHLD, SIG_IGN);

	/* Set a custom user directory (optional) */
	purple_util_set_user_dir(CUSTOM_USER_DIRECTORY);

	/* We do not want any debugging for now to keep the noise to a minimum. */
	purple_debug_set_enabled(TRUE);

	/* Set the core-uiops, which is used to
	 * 	- initialize the ui specific preferences.
	 * 	- initialize the debug ui.
	 * 	- initialize the ui components for all the modules.
	 * 	- uninitialize the ui components for all the modules when the core terminates.
	 */
	purple_core_set_ui_ops(&adapterCoreUIOps);

	purple_eventloop_set_ui_ops(&adapterEventLoopUIOps);

	/* purple_core_init () calls purple_dbus_init ().  We don't want libpurple's
	 * own dbus server, so let's kill it here.  Ideally, it would never be
	 * initialized in the first place, but hey.
	 */
	// building without dbus... no need to call this method
	// purple_dbus_uninit();

	if (!purple_core_init(UI_ID))
	{
		syslog(LOG_INFO, "libpurple initialization failed.");
		abort();
	}

	/* Create and load the buddylist. */
	purple_set_blist(purple_blist_new());
	purple_blist_load();

	purple_buddy_icons_set_cache_dir("/var/luna/data/im-avatars");

	libpurpleInitialized = TRUE;
	syslog(LOG_INFO, "libpurple initialized.\n");
}
/*
 * End of libpurple initialization methods
 */

static void loadserver(const char *serviceName)
{
	char configline[255];
	char *Server = "";
	char *ServerPort = "";
	char *ServerTLS = "";
	char *ServerProxy = "";
	char *ServerLogin = "";
	char *UserAgent = "";
	char *Resource = "";

	//Set Filename
	char *filename = NULL;
	filename = (char *)calloc(strlen("/var/preferences/org.webosinternals.messaging/") + strlen(serviceName) + strlen(".cfg") + 1, sizeof(char));
	strcat(filename, "/var/preferences/org.webosinternals.messaging/");
	strcat(filename, serviceName);
	strcat(filename, ".cfg");

	FILE *hFile;

	printf("Opening %s\n",filename);
	hFile = fopen(filename, "r");
	if (hFile == NULL)
	{
		printf("Error! %s not found.\n",filename);
		return;
	}
	else
	{
		printf("Opened file %s\n",filename);
		printf("Getting %s server address...\n",serviceName);

		//Read Config File
		fgets(configline, sizeof configline, hFile);
		char* token;
		const char dlm[] = ":";
		token = strtok(configline,dlm);
		Server = token;
		ServerPort = strtok(NULL,":");
		ServerTLS = strtok(NULL,":");

		if(Server == NULL)
		{
			Server = "";
		}
		if(ServerPort == NULL)
		{
			ServerPort = "";
		}
		if(ServerTLS == NULL)
		{
			ServerTLS = "";
		}

		if (strcmp(serviceName, "jabber") == 0)
		{
			Resource = strtok(NULL,":");

			if(Resource == NULL)
			{
				Resource = "";
			}

			printf("%s server resource: %s\n",serviceName, Resource);
		}

		if (strcmp(serviceName, "xfire") == 0)
		{
			Resource = strtok(NULL,":");

			if(Resource == NULL)
			{
				Resource = "";
			}

			printf("%s server version: %s\n",serviceName, Resource);
		}

		if (strcmp(serviceName, "lcs") == 0)
		{
			Resource = strtok(NULL,":");

			if(Resource == NULL)
			{
				Resource = "";
			}

			printf("%s server hide ID: %s\n",serviceName, Resource);
		}

		if (strcmp(serviceName, "sipe") == 0)
		{
			ServerLogin = strtok(NULL,":");

			if(ServerLogin == NULL)
			{
				ServerLogin = "";
			}

			printf("%s server login: %s\n",serviceName, ServerLogin);

			ServerProxy = strtok(NULL,":");

			if(ServerProxy == NULL)
			{
				ServerProxy = "";
			}

			printf("%s server proxy: %s\n",serviceName, ServerProxy);

			UserAgent = strtok(NULL,":");

			if(UserAgent == NULL)
			{
				UserAgent = "";
			}

			printf("%s user agent: %s\n",serviceName, UserAgent);
		}

		printf("%s server address: %s\n",serviceName, Server);
		printf("%s server port: %s\n",serviceName, ServerPort);
		printf("%s server TLS: %s\n",serviceName, ServerTLS);

		if (strcmp(serviceName, "jabber") == 0)
		{
			//Set Jabber Server
			JabberServer = (char *)calloc(strlen(Server) + 1,sizeof(char));
			strcat(JabberServer, Server);
			
			//Set Jabber Server Port
			JabberServerPort = (char *)calloc(strlen(ServerPort) + 1,sizeof(char));
			strcat(JabberServerPort, ServerPort);

			//Set Jabber Server TLS?
			if (strcmp(ServerTLS, "true") == 0)
			{
				JabberServerTLS = TRUE;
			}

			//Set Jabber Resource?
			if (strcmp(Resource, "$$BLANK$$") != 0)
			{
				JabberResource = (char *)calloc(strlen(Resource) + 1,sizeof(char));
				strcat(JabberResource, Resource);
			}
			else
			{
				JabberResource = "";
			}
		}
		if (strcmp(serviceName, "sipe") == 0)
		{
			//Set SIPE Server
			if (strcmp(Server, "$$BLANK$$") != 0)
			{
				SIPEServer = (char *)calloc(strlen(Server) + 1,sizeof(char));
				strcat(SIPEServer, Server);
			}
			else
			{
				SIPEServer = "";
			}
			
			//Set SIPE Server Port
			if (strcmp(ServerPort, "$$BLANK$$") != 0)
			{
				SIPEServerPort = (char *)calloc(strlen(ServerPort) + 1,sizeof(char));
				strcat(SIPEServerPort, ServerPort);
			}
			else
			{
				SIPEServerPort = "";
			}

			//Set SIPE Server TLS?
			if (strcmp(ServerTLS, "true") == 0)
			{
				SIPEServerTLS = TRUE;
			}

			//Set SIPE Server Proxy?
			if (strcmp(ServerProxy, "true") == 0)
			{
				SIPEServerProxy = TRUE;
			}

			//Set SIPE Server Login
			if (strcmp(ServerLogin, "$$BLANK$$") != 0)
			{
				SIPEServerLogin = (char *)calloc(strlen(ServerLogin) + 1,sizeof(char));
				strcat(SIPEServerLogin, ServerLogin);
			}
			else
			{
				SIPEServerLogin = "";
			}

			//Set SIPE User Agent
			if (strcmp(UserAgent, "$$BLANK$$") != 0)
			{
				SIPEUserAgent = (char *)calloc(strlen(UserAgent) + 1,sizeof(char));
				strcat(SIPEUserAgent, UserAgent);
			}
			else
			{
				SIPEUserAgent = "";
			}
		}
		if (strcmp(serviceName, "lcs") == 0)
		{
			//Set Sametime Server
			SametimeServer = (char *)calloc(strlen(Server) + 1,sizeof(char));
			strcat(SametimeServer, Server);
			
			//Set Sametime Server Port
			SametimeServerPort = (char *)calloc(strlen(ServerPort) + 1,sizeof(char));
			strcat(SametimeServerPort, ServerPort);

			//Set Sametime Server TLS?
			if (strcmp(ServerTLS, "true") == 0)
			{
				SametimeServerTLS = TRUE;
			}

			//Set Sametime Hide Client ID?
			if (strcmp(Resource, "true") == 0)
			{
				SametimehideID = TRUE;
			}
		}
		if (strcmp(serviceName, "gwim") == 0)
		{
			//Set Group Wise Server
			gwimServer = (char *)calloc(strlen(Server) + 1,sizeof(char));
			strcat(gwimServer, Server);
			
			//Set Group Wise Server Port
			gwimServerPort = (char *)calloc(strlen(ServerPort) + 1,sizeof(char));
			strcat(gwimServerPort, ServerPort);

			//Set Group Wise Server TLS?
			if (strcmp(ServerTLS, "true") == 0)
			{
				gwimServerTLS = TRUE;
			}
		}
		if (strcmp(serviceName, "myspace") == 0)
		{
			//Set MySapce Server
			MySpaceServer = (char *)calloc(strlen(Server) + 1,sizeof(char));
			strcat(MySpaceServer, Server);
			
			//Set MySpace Server Port
			MySpaceServerPort = (char *)calloc(strlen(ServerPort) + 1,sizeof(char));
			strcat(MySpaceServerPort, ServerPort);
		}
		if (strcmp(serviceName, "gadu") == 0)
		{
			//Set Gadu Gadu Server
			GaduServer = (char *)calloc(strlen(Server) + 1,sizeof(char));
			strcat(GaduServer, Server);
		}
		if (strcmp(serviceName, "xfire") == 0)
		{
			//Set XFire Server
			XFireServer = (char *)calloc(strlen(Server) + 1,sizeof(char));
			strcat(XFireServer, Server);
			
			//Set XFire Server Port
			XFireServerPort = (char *)calloc(strlen(ServerPort) + 1,sizeof(char));
			strcat(XFireServerPort, ServerPort);

			//Set XFire Version
			XFireversion = (char *)calloc(strlen(Resource) + 1,sizeof(char));
			strcat(XFireversion, Resource);
		}
	}

	// Close file
	fclose(hFile);
}

static void loadpreference(const char *serviceName, const char *preference)
{
	char configline[255];
	char *Server = "";
	char *ServerPort = "";
	char *ServerTLS = "";
	char *ServerProxy = "";
	char *ServerLogin = "";

	//Set Filename
	char *filename = NULL;
	filename = (char *)calloc(strlen("/var/preferences/org.webosinternals.messaging/") + strlen(serviceName) + strlen(".") + strlen(preference) + 1, sizeof(char));
	strcat(filename, "/var/preferences/org.webosinternals.messaging/");
	strcat(filename, serviceName);
	strcat(filename, ".");
	strcat(filename, preference);

	FILE *hFile;

	printf("Opening %s\n",filename);
	hFile = fopen(filename, "r");
	if (hFile == NULL)
	{
		if (strcmp(serviceName, "live") == 0)
		{
			if (strcmp(preference, "Avatar") == 0)
			{
				LiveAvatar = FALSE;
			}
			if (strcmp(preference, "Alias") == 0)
			{
				LiveAlias = FALSE;
			}
		}
		if (strcmp(serviceName, "xfire") == 0)
		{
			if (strcmp(preference, "Avatar") == 0)
			{
				XFireAvatar = FALSE;
			}
			if (strcmp(preference, "Alias") == 0)
			{
				XFireAlias = FALSE;
			}
		}
		if (strcmp(serviceName, "sipe") == 0)
		{
			SIPEAvatar = FALSE;
		}
		if (strcmp(serviceName, "yahoo") == 0)
		{
			YahooAvatar = FALSE;
		}
		if (strcmp(serviceName, "jabber") == 0)
		{
			JabberAvatar = FALSE;
		}
		if (strcmp(serviceName, "lcs") == 0)
		{
			SametimeAvatar = FALSE;
		}
		if (strcmp(serviceName, "gwim") == 0)
		{
			gwimAvatar = FALSE;
		}
		if (strcmp(serviceName, "myspace") == 0)
		{
			MySpaceAvatar = FALSE;
		}
		if (strcmp(serviceName, "gadu") == 0)
		{
			GaduAvatar = FALSE;
		}
		if (strcmp(serviceName, "icq") == 0)
		{
			if (strcmp(preference, "Avatar") == 0)
			{
				ICQAvatar = FALSE;
			}
			if (strcmp(preference, "Alias") == 0)
			{
				ICQAlias = FALSE;
			}
		}
		if (strcmp(serviceName, "qqim") == 0)
		{
			QQAvatar = FALSE;
		}
		if (strcmp(serviceName, "facebook") == 0)
		{
			FacebookAvatar = FALSE;
		}
		if (strcmp(serviceName, "aol") == 0)
		{
			AIMAvatar = FALSE;
		}
		if (strcmp(serviceName, "gmail") == 0)
		{
			GTalkAvatar = FALSE;
		}
		return;
	}
	else
	{
		if (strcmp(serviceName, "live") == 0)
		{
			if (strcmp(preference, "Avatar") == 0)
			{
				LiveAvatar = TRUE;
			}
			if (strcmp(preference, "Alias") == 0)
			{
				LiveAlias = TRUE;
			}
		}
		if (strcmp(serviceName, "xfire") == 0)
		{
			if (strcmp(preference, "Avatar") == 0)
			{
				XFireAvatar = TRUE;
			}
			if (strcmp(preference, "Alias") == 0)
			{
				XFireAlias = TRUE;
			}
		}
		if (strcmp(serviceName, "sipe") == 0)
		{
			SIPEAvatar = TRUE;
		}
		if (strcmp(serviceName, "yahoo") == 0)
		{
			YahooAvatar = TRUE;
		}
		if (strcmp(serviceName, "jabber") == 0)
		{
			JabberAvatar = TRUE;
		}
		if (strcmp(serviceName, "lcs") == 0)
		{
			SametimeAvatar = TRUE;
		}
		if (strcmp(serviceName, "gwim") == 0)
		{
			gwimAvatar = TRUE;
		}
		if (strcmp(serviceName, "myspace") == 0)
		{
			MySpaceAvatar = TRUE;
		}
		if (strcmp(serviceName, "gadu") == 0)
		{
			GaduAvatar = TRUE;
		}
		if (strcmp(serviceName, "icq") == 0)
		{
			if (strcmp(preference, "Avatar") == 0)
			{
				ICQAvatar = TRUE;
			}
			if (strcmp(preference, "Alias") == 0)
			{
				ICQAlias = TRUE;
			}
		}
		if (strcmp(serviceName, "qqim") == 0)
		{
			QQAvatar = TRUE;
		}
		if (strcmp(serviceName, "facebook") == 0)
		{
			FacebookAvatar = TRUE;
		}
		if (strcmp(serviceName, "aol") == 0)
		{
			AIMAvatar = TRUE;
		}
		if (strcmp(serviceName, "gmail") == 0)
		{
			GTalkAvatar = TRUE;
		}
	}

	// Close file
	fclose(hFile);
}

/*
 * Service methods
 */
static bool login(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal;
	bool success = TRUE;
	LSError lserror;
	LSErrorInit(&lserror);

	/* Passed parameters */
	const char *serviceName = NULL;
	const char *username = NULL;
	const char *password = NULL;
	int availability = 0;
	const char *customMessage = NULL;
	const char *localIpAddress = NULL;
	const char *connectionType = NULL;

	bool subscribe = FALSE;

	PurpleAccount *account;
	char *myJavaFriendlyUsername = NULL;
	char *prplProtocolId = NULL;
	char *transportFriendlyUserName = NULL;
	char *accountKey = NULL;
	bool accountIsAlreadyOnline = FALSE;
	bool accountIsAlreadyPending = FALSE;

	bool invalidParameters = TRUE;

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	char *payload = strdup(LSMessageGetPayload(message));
	struct json_object *responsePayload = json_object_new_object();
	
	if (!payload)
	{
		success = FALSE;
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		success = FALSE;
		goto error;
	}
	subscribe = json_object_get_boolean(json_object_object_get(params, "subscribe"));

	serviceName = getField(params, "serviceName");
	if (!serviceName)
	{
		success = FALSE;
		goto error;
	}

	username = getField(params, "username");
	if (!username)
	{
		success = FALSE;
		goto error;
	}

	password = getField(params, "password");

	availability = json_object_get_int(json_object_object_get(params, "availability"));

	customMessage = getField(params, "customMessage");
	if (!customMessage)
	{
		customMessage = "";
	}

	localIpAddress = getField(params, "localIpAddress");
	if (!localIpAddress)
	{
		localIpAddress = "";
	}
	
	connectionType = getField(params, "connectionType");
	if (!connectionType)
	{
		connectionType = "";
	}

	invalidParameters = FALSE;

	syslog(LOG_INFO, "Parameters: servicename %s, connectionType %s", serviceName, connectionType);

	if (libpurpleInitialized == FALSE)
	{
		initializeLibpurple();
	}

	//Load custom server details
	if (strcmp(serviceName, "jabber") == 0)
	{
		loadserver ("jabber");
		loadpreference ("jabber", "Avatar");
	}
	if (strcmp(serviceName, "sipe") == 0)
	{
		loadserver ("sipe");
		loadpreference ("sipe", "Avatar");
	}
	if (strcmp(serviceName, "lcs") == 0)
	{
		loadserver ("lcs");
		loadpreference ("lcs", "Avatar");
	}
	if (strcmp(serviceName, "gwim") == 0)
	{
		loadserver ("gwim");
		loadpreference ("gwim", "Avatar");
	}
	if (strcmp(serviceName, "myspace") == 0)
	{
		loadserver ("myspace");
		loadpreference ("myspace", "Avatar");
	}
	if (strcmp(serviceName, "gadu") == 0)
	{
		loadserver ("gadu");
		loadpreference ("gadu", "Avatar");
	}
	if (strcmp(serviceName, "icq") == 0)
	{
		loadpreference ("icq", "Avatar");
		loadpreference ("icq", "Alias");
	}
	if (strcmp(serviceName, "yahoo") == 0)
	{
		loadpreference ("yahoo", "Avatar");
	}
	if (strcmp(serviceName, "live") == 0)
	{
		loadpreference ("live", "Avatar");
		loadpreference ("live", "Alias");
	}
	if (strcmp(serviceName, "xfire") == 0)
	{
		loadpreference ("xfire", "Avatar");
		loadpreference ("xfire", "Alias");
		loadserver ("xfire");
	}
	if (strcmp(serviceName, "qqim") == 0)
	{
		loadpreference ("qqim", "Avatar");
	}

	/* libpurple variables */
	transportFriendlyUserName = getPrplFriendlyUsername(serviceName, username);
	accountKey = getAccountKey(username, serviceName);

	myJavaFriendlyUsername = getJavaFriendlyUsername(username, serviceName);

	json_object_object_add(responsePayload, "serviceName", json_object_new_string((char*)serviceName));
	json_object_object_add(responsePayload, "username", json_object_new_string((char*)myJavaFriendlyUsername));

	/*
	 * Let's check to see if we're already logged in to this account or that we're already in the process of logging in 
	 * to this account. This can happen when java goes down and comes back up.
	 */
	PurpleAccount *alreadyActiveAccount = g_hash_table_lookup(onlineAccountData, accountKey);
	if (alreadyActiveAccount != NULL)
	{
		accountIsAlreadyOnline = TRUE;
	}
	else
	{
		alreadyActiveAccount = g_hash_table_lookup(pendingAccountData, accountKey);
		if (alreadyActiveAccount != NULL)
		{
			accountIsAlreadyPending = TRUE;
		}
	}

	if (alreadyActiveAccount != NULL)
	{
		/*
		 * We're either already logged in to this account or we're already in the process of logging in to this account 
		 * (i.e. it's pending; waiting for server response)
		 */
		char *accountBoundToIpAddress = g_hash_table_lookup(ipAddressesBoundTo, accountKey);
		if (accountBoundToIpAddress != NULL && strcmp(localIpAddress, accountBoundToIpAddress) == 0)
		{
			/*
			 * We're using the right interface for this account
			 */
			if (accountIsAlreadyPending)
			{
				syslog(LOG_INFO, "We were already in the process of logging in");
				/* 
				 * keep the message in order to respond to it in either account_logged_in or account_login_failed 
				 */
				LSMessageRef(message);
				/*
				 * remove the old login message
				 */
				g_hash_table_remove(loginMessages, accountKey);
				/*
				 * add the new login message to respond to once account_logged_in is called
				 */
				g_hash_table_insert(loginMessages, accountKey, message);
				if (transportFriendlyUserName)
				{
					free(transportFriendlyUserName);
				}
				return TRUE;
			}
			else if (accountIsAlreadyOnline)
			{
				syslog(LOG_INFO, "We were already logged in to the requested account");
				json_object_object_add(responsePayload, "accountWasAlreadyLoggedIn", json_object_new_boolean(TRUE));
				json_object_object_add(responsePayload, "returnValue", json_object_new_boolean(TRUE));

				LSError lserror;
				LSErrorInit(&lserror);

				bool retVal = LSMessageReply(serviceHandle, message, json_object_to_json_string(responsePayload),
						&lserror);
				if (!retVal)
				{
					LSErrorPrint(&lserror, stderr);
				}
				if (transportFriendlyUserName)
				{
					free(transportFriendlyUserName);
				}
				return TRUE;
			}
		}
		else
		{
			/*
			 * We're not using the right interface. Close the current connection for this account and create a new one
			 */
			syslog(LOG_INFO,
					"We have to logout and login again since the local IP address has changed. Logging out from account");
			/*
			 * Once the current connection is closed we don't want to let java know that the account was disconnected. 
			 * Since java went down and came back up it didn't know that the account was connected anyways. 
			 * So let's take the account out of the account data hash and then disconnect it.
			 */
			if (g_hash_table_lookup(onlineAccountData, accountKey) != NULL)
			{
				g_hash_table_remove(onlineAccountData, accountKey);
			}
			if (g_hash_table_lookup(pendingAccountData, accountKey) != NULL)
			{
				g_hash_table_remove(pendingAccountData, accountKey);
			}
			purple_account_disconnect(alreadyActiveAccount);
		}
	}

	/*
	 * Let's go through our usual login process
	 */

	if (strcmp(username, "") == 0)
	{
		success = FALSE;
	}
	// TODO this currently ignores authentication token, but should check it as well when support for auth token is added
	else if (password == NULL || strlen(password) == 0)
	{
		if (strcmp(serviceName, "irc") != 0)
		{
			/*syslog(LOG_INFO, "Error: null or empty password trying to log in to servicename %s", serviceName);
			success = FALSE;
			invalidParameters = FALSE;
			json_object_object_add(responsePayload, "returnValue", json_object_new_boolean(FALSE));
			json_object_object_add(responsePayload, "errorCode", json_object_new_string("AcctMgr_Bad_Password"));
			json_object_object_add(responsePayload, "errorText", json_object_new_string("Bad password"));
			goto error;*/

			//Work around for yahoo password error
			json_object_object_add(responsePayload, "returnValue", json_object_new_boolean(TRUE));

			retVal = LSMessageReply(serviceHandle, message, json_object_to_json_string(responsePayload),&lserror);
			if (!retVal)
			{
				LSErrorPrint(&lserror, stderr);
			}

			free (payload);
			return TRUE;
		}
		else
		{
			password = "";
		}
	}
	else
	{
		/* save the local IP address that we need to use */
		if (localIpAddress != NULL && strcmp(localIpAddress, "") != 0)
		{
			purple_prefs_remove("/purple/network/preferred_local_ip_address");
			purple_prefs_add_string("/purple/network/preferred_local_ip_address", localIpAddress);
		}
		else
		{
#ifdef DEVICE
			/*
			 * If we're on device you should not accept an empty ipAddress; it's mandatory to be provided
			 */
			success = FALSE;
			json_object_object_add(responsePayload, "errorCode", json_object_new_string("AcctMgr_Network_Error"));
			json_object_object_add(responsePayload, "errorText", json_object_new_string("localIpAddress was null or empty"));
			goto error;
#endif
		}

		/* save the local IP address that we need to use */
		if (connectionType != NULL && strcmp(connectionType, "") != 0)
		{
			g_hash_table_insert(connectionTypeData, accountKey, strdup(connectionType));
		}
		
		prplProtocolId = getPrplProtocolIdFromServiceName(serviceName);

		/*
		 * If we've already logged in to this account before then re-use the old PurpleAccount struct
		 */
		//Except for jabber. Work around for changing resource
		if (strcmp(serviceName, "jabber") == 0)
		{
			if (g_hash_table_lookup(offlineAccountData, accountKey) != NULL)
			{
				g_hash_table_remove(offlineAccountData, accountKey);
			}
		}

		account = g_hash_table_lookup(offlineAccountData, accountKey);
		if (!account)
		{
			/* Create the account */
			account = purple_account_new(transportFriendlyUserName, prplProtocolId);
			if (!account)
			{
				success = FALSE;
				goto error;
			}
		}

		if (strcmp(prplProtocolId, "prpl-jabber") == 0 && g_str_has_suffix(transportFriendlyUserName, "@gmail.com") == FALSE && g_str_has_suffix(transportFriendlyUserName, "@googlemail.com") == FALSE && strcmp(serviceName, "gmail") == 0)
		{
			/*
			 * Special case for gmail... don't try to connect to mydomain.com if the username is me@mydomain.com. They might not have
			 * setup the SRV record. Always connect to gmail.
			 */
			purple_account_set_string(account, "connect_server", "talk.google.com");

			//Set Account Alias to gtalk
			purple_account_set_alias (account,"gtalk");
		}
		if (strcmp(serviceName, "gmail") == 0)
		{
			//Set Account Alias to gtalk
			purple_account_set_alias (account,"gtalk");
		}
		if (strcmp(serviceName, "gmail") == 0)
		{
			//Don't load chat history
			purple_account_set_bool(account, "facebook_show_history", FALSE);
		}
		if (strcmp(serviceName, "gadu") == 0)
		{
			//Set connect server
			purple_account_set_string(account, "gg_server", GaduServer);
		}
		if (strcmp(serviceName, "icq") == 0)
		{
			//Set connect server (fix for ICQ)
			purple_account_set_string(account, "transport", "auto");
			purple_account_set_string(account, "server", "login.icq.com");
		}
		if (strcmp(prplProtocolId, "prpl-msn") == 0 && g_str_has_suffix(transportFriendlyUserName, "@hotmail.com")
				== FALSE)
		{
			/*
			 * Special case for hotmail... don't try to connect to theraghavans.com if the username is nash@theraghavans.com
			 * Always connect to hotmail.
			 */
			purple_account_set_string(account, "connect_server", "messenger.hotmail.com");
		}

		if (strcmp(prplProtocolId, "prpl-jabber") == 0 && JabberServer != NULL && strcmp(serviceName, "jabber") == 0)
		{
			/*
			 * Special case for jabber... connect to user defined jabber server
			 */
			purple_account_set_string(account, "connect_server", JabberServer);
			purple_account_set_int(account, "port", atoi(JabberServerPort));
			purple_account_set_bool(account, "require_tls", JabberServerTLS);

			//Set Account Alias to Jabber
			purple_account_set_alias (account,"jabber");
		}
		if (strcmp(prplProtocolId, "prpl-sipe") == 0 && SIPEServer != NULL)
		{
			/*
			 * Special case for sipe... connect to user defined sipe server
			 */

			//Set ServerName
			if(strcmp(SIPEServer, "") != 0) 
			{
				char *SIPEFullServerName = NULL;
				SIPEFullServerName = (char *)calloc(strlen(SIPEServer) + strlen(SIPEServerPort) + 1, sizeof(char));
				strcat(SIPEFullServerName, SIPEServer);
				strcat(SIPEFullServerName, ":");
				strcat(SIPEFullServerName, SIPEServerPort);
				purple_account_set_string(account, "server", SIPEFullServerName);

				if (SIPEFullServerName)
				{
					free(SIPEFullServerName);
				}
			}

			//Require TLS?
			purple_account_set_bool(account, "require_tls", SIPEServerTLS);
			if (SIPEServerTLS)
			{
				purple_account_set_string(account, "transport", "tls");
			}
			else
			{
				purple_account_set_string(account, "transport", "auto");
			}

			//Proxy?
			if (SIPEServerProxy)
			{
				//Leave Default
			}
			else
			{
				//Disable Proxy
				static PurpleProxyInfo info = {0, NULL, 0, NULL, NULL};
				purple_proxy_info_set_type(&info, PURPLE_PROXY_NONE);
			}

			//User Agent
			if(strcmp(SIPEUserAgent, "") != 0) 
			{
				purple_account_set_string(account, "useragent", SIPEUserAgent);
			}
		}
		if (strcmp(prplProtocolId, "prpl-meanwhile") == 0 && SametimeServer != NULL)
		{
			/*
			 * Special case for sametime... connect to user defined sametime server
			 */

			purple_account_set_string(account, "server", SametimeServer);
			purple_account_set_int(account, "port", atoi(SametimeServerPort));
			purple_account_set_bool(account, "require_tls", SametimeServerTLS);
			purple_account_set_bool(account, "fake_client_id", SametimehideID);
		}
		if (strcmp(prplProtocolId, "prpl-novell") == 0 && gwimServer != NULL)
		{
			/*
			 * Special case for Group Wise... connect to user defined Group Wise server
			 */

			purple_account_set_string(account, "server", gwimServer);
			purple_account_set_int(account, "port", atoi(gwimServerPort));
			purple_account_set_bool(account, "require_tls", gwimServerTLS);
		}
		if (strcmp(prplProtocolId, "prpl-qq") == 0)
		{
			/*
			 * Special case for QQ... Set QQ Server Version
			 */
			printf("Setting QQ Client Version to qq2008\n");
			purple_account_set_string(account, "client_version", "qq2008");
		}
		if (strcmp(prplProtocolId, "prpl-xfire") == 0)
		{
			/*
			 * Special case for XFire... Set XFire Server Version
			 */
			purple_account_set_int(account, "version", atoi(XFireversion));
			purple_account_set_string(account, "server", XFireServer);
			purple_account_set_int(account, "port", atoi(XFireServerPort));
		}

		syslog(LOG_INFO, "Logging in...");

		purple_account_set_password(account, password);

		if (registeredForAccountSignals == FALSE)
		{
			static int handle;
			/*
			 * Listen for a number of different signals:
			 */
			purple_signal_connect(purple_connections_get_handle(), "signed-on", &handle,
					PURPLE_CALLBACK(account_logged_in), NULL);
			purple_signal_connect(purple_connections_get_handle(), "signed-off", &handle,
					PURPLE_CALLBACK(account_signed_off_cb), NULL);

			purple_signal_connect(purple_connections_get_handle(), "account-status-changed", &handle,
					PURPLE_CALLBACK(account_status_changed), NULL);

			/*purple_signal_connect(purple_connections_get_handle(), "account-authorization-denied", &handle,
			 PURPLE_CALLBACK(account_login_failed), NULL);*/
			purple_signal_connect(purple_connections_get_handle(), "connection-error", &handle,
					PURPLE_CALLBACK(account_login_failed), NULL);
			registeredForAccountSignals = TRUE;
		}
	}

	if (success)
	{
		/* keep the message in order to respond to it in either account_logged_in or account_login_failed */
		LSMessageRef(message);
		g_hash_table_insert(loginMessages, accountKey, message);
		/* mark the account as pending */
		g_hash_table_insert(pendingAccountData, accountKey, account);

		if (localIpAddress != NULL && strcmp(localIpAddress, "") != 0)
		{
			/* keep track of the local IP address that we bound to when logging in to this account */
			g_hash_table_insert(ipAddressesBoundTo, accountKey, strdup(localIpAddress));
		}

		/* It's necessary to enable the account first. */
		purple_account_set_enabled(account, UI_ID, TRUE);

		/* Now, to connect the account, create a status and activate it. */

		/*
		 * Create a timer for this account's login. If after 180 seconds login has not succeeded
		 */
		guint timerHandle = purple_timeout_add_seconds(CONNECT_TIMEOUT_SECONDS, connectTimeoutCallback, accountKey);
		g_hash_table_insert(accountLoginTimers, accountKey, (gpointer)timerHandle);

		PurpleStatusPrimitive prim = getPrplAvailabilityFromPalmAvailability(availability);
		PurpleSavedStatus *savedStatus = purple_savedstatus_new(NULL, prim);
		purple_savedstatus_set_message(savedStatus, customMessage);
		purple_savedstatus_activate_for_account(savedStatus, account);

		json_object_object_add(responsePayload, "returnValue", json_object_new_boolean(TRUE));
	}
	else
	{
		json_object_object_add(responsePayload, "errorCode", json_object_new_string("AcctMgr_Generic_Error"));
		json_object_object_add(responsePayload, "errorText", json_object_new_string("AcctMgr_Generic_Error"));
	}

	error:

	if (!success)
	{
		if (invalidParameters)
		{
			json_object_object_add(responsePayload, "returnValue", json_object_new_boolean(FALSE));
			json_object_object_add(responsePayload, "errorCode", json_object_new_string("1"));
			json_object_object_add(responsePayload, "errorText",
					json_object_new_string("Invalid parameter. Please double check the passed parameters."));
		}
		retVal = LSMessageReturn(lshandle, message, json_object_to_json_string(responsePayload), &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
	}
	//TODO: do I need to do this?
	// LSErrorFree (&lserror);
	if (myJavaFriendlyUsername)
	{
		free(myJavaFriendlyUsername);
	}
	if (prplProtocolId)
	{
		free(prplProtocolId);
	}
	if (transportFriendlyUserName)
	{
		free(transportFriendlyUserName);
	}
	if (!is_error(responsePayload)) 
	{
		json_object_put(responsePayload);
	}
	if (payload)
	{
		free(payload);
	}
	if (!is_error(params)) 
	{
		json_object_put(params);
	}
	return TRUE;
}

static bool logout(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal;
	bool success = TRUE;
	LSError lserror;
	LSErrorInit(&lserror);

	/* Passed parameters */
	const char *serviceName = "";
	const char *username = "";

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	// get the message's payload (json object)
	json_t *object = LSMessageGetPayloadJSON(message);

	if (!object)
	{
		success = FALSE;
		goto error;
	}

	retVal = json_get_string(object, "serviceName", &serviceName);
	if (!retVal)
	{
		success = FALSE;
		goto error;
	}

	retVal = json_get_string(object, "username", &username);
	if (!retVal)
	{
		success = FALSE;
		goto error;
	}

	syslog(LOG_INFO, "Parameters: servicename %s", serviceName);

	char *accountKey = getAccountKey(username, serviceName);

	PurpleAccount *accountTologoutFrom = g_hash_table_lookup(onlineAccountData, accountKey);
	if (accountTologoutFrom == NULL)
	{
		accountTologoutFrom = g_hash_table_lookup(pendingAccountData, accountKey);
		if (accountTologoutFrom == NULL)
		{
			GString *jsonResponse = g_string_new("{\"serviceName\":\"");
			g_string_append(jsonResponse, serviceName);
			g_string_append(jsonResponse, "\",  \"username\":\"");
			g_string_append(jsonResponse, username);
			g_string_append(
					jsonResponse,
					"\",  \"returnValue\":false, \"errorCode\":\"1\", \"errorText\":\"Trying to logout from an account that is not logged in\"}");
			bool retVal = LSMessageReturn(lshandle, message, jsonResponse->str, &lserror);
			if (!retVal)
			{
				LSErrorPrint(&lserror, stderr);
			}
			success = FALSE;
			LSErrorFree(&lserror);
			g_string_free(jsonResponse, TRUE);
			return TRUE;
		}
	}

	/* keep the message in order to respond to it in either account_logged_in or account_login_failed */
	LSMessageRef(message);
	g_hash_table_insert(logoutMessages, getAccountKeyFromPurpleAccount(accountTologoutFrom), message);

	purple_account_disconnect(accountTologoutFrom);

	error: if (!success)
	{
		retVal
				= LSMessageReturn(
						lshandle,
						message,
						"{\"returnValue\":false, \"errorCode\":\"1\", \"errorText\":\"Invalid parameter. Please double check the passed parameters.\"}",
						&lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		LSErrorFree(&lserror);
	}

	if (accountKey)
	{
		free(accountKey);
	}
	return TRUE;
}

static bool setMyAvailability(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal = FALSE;
//	bool success = TRUE;
	LSError lserror;
	LSErrorInit(&lserror);

	/* Passed parameters */
	const char *serviceName = "";
	const char *username = "";
	int availability = 0;

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	// get the message's payload (json object)
	json_t *object = LSMessageGetPayloadJSON(message);

	if (!object)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_string(object, "serviceName", &serviceName);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_string(object, "username", &username);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_int(object, "availability", &availability);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	syslog(LOG_INFO, "Parameters: serviceName %s, availability %i", serviceName, availability);

	char *accountKey = getAccountKey(username, serviceName);

	PurpleAccount *account = g_hash_table_lookup(onlineAccountData, accountKey);

	if (account == NULL)
	{
		//this should never happen based on MessagingService's logic
		syslog(LOG_INFO,
				"setMyAvailability was called on an account that wasn't logged in. serviceName: %s, availability: %i",
				serviceName, availability);
		retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		//success = FALSE;
		goto error;
	}
	else
	{
		/*
		 * Let's get the current custom message and set it as well so that we don't overwrite it with ""
		 */
		PurplePresence *presence = purple_account_get_presence(account);
		const PurpleStatus *status = purple_presence_get_active_status(presence);
		const PurpleValue *value = purple_status_get_attr_value(status, "message");
		const char *customMessage = NULL;
		if (value != NULL)
		{
			customMessage = purple_value_get_string(value);
		}
		if (customMessage == NULL)
		{
			customMessage = "";
		}

		PurpleStatusPrimitive prim = getPrplAvailabilityFromPalmAvailability(availability);
		PurpleStatusType *type = purple_account_get_status_type_with_primitive(account, prim);
		GList *attrs = NULL;
		attrs = g_list_append(attrs, "message");
		attrs = g_list_append(attrs, (char*)customMessage);
		purple_account_set_status_list(account, purple_status_type_get_id(type), TRUE, attrs);

		char availabilityString[2];
		sprintf(availabilityString, "%i", availability);

		GString *jsonResponse = g_string_new("{\"serviceName\":\"");
		g_string_append(jsonResponse, serviceName);
		g_string_append(jsonResponse, "\",  \"username\":\"");
		g_string_append(jsonResponse, username);
		g_string_append(jsonResponse, "\",  \"availability\":");
		g_string_append(jsonResponse, availabilityString);
		g_string_append(jsonResponse, ", \"returnValue\":true}");
		LSError lserror;
		LSErrorInit(&lserror);
		retVal = LSMessageReturn(lshandle, message, jsonResponse->str, &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		g_string_free(jsonResponse, TRUE);
	}

	error: if (!retVal)
	{
		syslog(LOG_INFO, "%s: sending response failed", __FUNCTION__);
	}
	LSErrorFree(&lserror);
	return TRUE;
}

static bool getMyAvailability(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal = FALSE;
//	bool success = TRUE;
	LSError lserror;
	LSErrorInit(&lserror);

	/* Passed parameters */
	const char *serviceName = "";
	const char *username = "";

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	// get the message's payload (json object)
	json_t *object = LSMessageGetPayloadJSON(message);

	if (!object)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_string(object, "serviceName", &serviceName);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_string(object, "username", &username);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	char *accountKey = getAccountKey(username, serviceName);

	PurpleAccount *account = g_hash_table_lookup(onlineAccountData, accountKey);

	if (account == NULL)
	{
		//this should never happen based on MessagingService's logic
		syslog(LOG_INFO, "getMyAvailability was called on an account that wasn't logged in. serviceName: %s",serviceName);
		retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		//success = FALSE;
		goto error;
	}
	else
	{
		//Get current status for account
		PurpleStatus *AccountStatus = purple_account_get_active_status(account);
		int CurrentStatus = purple_status_type_get_primitive(purple_status_get_type(AccountStatus));

		char StatusString[2];
		sprintf(StatusString, "%i", CurrentStatus);

		GString *jsonResponse = g_string_new("{\"serviceName\":\"");
		g_string_append(jsonResponse, serviceName);
		g_string_append(jsonResponse, "\",  \"username\":\"");
		g_string_append(jsonResponse, username);
		g_string_append(jsonResponse, "\",  \"status\":");
		g_string_append(jsonResponse, StatusString);
		g_string_append(jsonResponse, ", \"returnValue\":true}");
		LSError lserror;
		LSErrorInit(&lserror);
		retVal = LSMessageReturn(lshandle, message, jsonResponse->str, &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		g_string_free(jsonResponse, TRUE);
	}

	error: if (!retVal)
	{
		syslog(LOG_INFO, "%s: sending response failed", __FUNCTION__);
	}
	LSErrorFree(&lserror);
	return TRUE;
}

static bool setMyCustomMessage(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal = FALSE;
	//bool success = TRUE;
	LSError lserror;
	LSErrorInit(&lserror);

	/* Passed parameters */
	const char *serviceName = "";
	const char *username = "";
	const char *customMessage = "";

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	char *payload = strdup(LSMessageGetPayload(message));

	if (!payload)
	{
		//success = FALSE;
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		//success = FALSE;
		goto error;
	}

	serviceName = getField(params, "serviceName");
	if (!serviceName)
	{
		//success = FALSE;
		goto error;
	}

	username = getField(params, "username");
	if (!username)
	{
		//success = FALSE;
		goto error;
	}
	customMessage = getField(params, "customMessage");
	if (!customMessage)
	{
		//success = FALSE;
		goto error;
	}

	syslog(LOG_INFO, "Parameters: serviceName %s", serviceName);

	char *accountKey = getAccountKey(username, serviceName);

	PurpleAccount *account = g_hash_table_lookup(onlineAccountData, accountKey);
	if (account != NULL)
	{
		// get the account's current status type
		PurpleStatusType *type = purple_status_get_type(purple_account_get_active_status(account));
		GList *attrs = NULL;
		attrs = g_list_append(attrs, "message");
		attrs = g_list_append(attrs, (char*)customMessage);
		purple_account_set_status_list(account, purple_status_type_get_id(type), TRUE, attrs);

		struct json_object *payload = json_object_new_object();
		json_object_object_add(payload, "serviceName", json_object_new_string((char*)serviceName));
		json_object_object_add(payload, "username", json_object_new_string((char*)username));
		json_object_object_add(payload, "customMessage", json_object_new_string((char*)customMessage));
		json_object_object_add(payload, "returnValue", json_object_new_boolean(TRUE));
		LSError lserror;
		LSErrorInit(&lserror);

		retVal = LSMessageReturn(lshandle, message, json_object_to_json_string(payload), &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		if (!is_error(payload)) 
		{
			json_object_put(payload);
		}
	}

	error: if (!retVal)
	{
		syslog(LOG_INFO, "%s: sending response failed", __FUNCTION__);
	}
	LSErrorFree(&lserror);
	if (!is_error(params)) 
	{
		json_object_put(params);
	}
	return TRUE;
}

static bool blockBuddy(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal = FALSE;
	bool success = FALSE;
	LSError lserror;
	struct json_object *payload = json_object_new_object();
	LSErrorInit(&lserror);

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	/* Passed parameters */
	const char *serviceName = "";
	const char *username = "";
	const char *buddyUsername = "";
	char *errorCode = "";
	char *errorText = "";

	bool block = TRUE;

	// get the message's payload (json object)
	json_t *object = LSMessageGetPayloadJSON(message);

	if (!object)
	{
		goto error;
	}

	retVal = json_get_string(object, "serviceName", &serviceName);
	if (!retVal)
	{
		goto error;
	}
	json_object_object_add(payload, "serviceName", json_object_new_string((char*)serviceName));

	retVal = json_get_string(object, "username", &username);
	if (!retVal)
	{
		goto error;
	}
	json_object_object_add(payload, "username", json_object_new_string((char*)username));

	retVal = json_get_string(object, "usernameToBlock", &buddyUsername);
	if (!retVal)
	{
		goto error;
	}

	retVal = json_get_bool(object, "block", &block);
	if (!retVal)
	{
		goto error;
	}

	char *accountKey = getAccountKey(username, serviceName);
	PurpleAccount *account = g_hash_table_lookup(onlineAccountData, accountKey);
	success = (account != NULL);
	if (success)
	{
		if (block) {
			purple_privacy_deny(account, buddyUsername, false, true);
		}
		else
		{
			purple_privacy_allow(account, buddyUsername, false, true);
		}
	}
	else
	{
		errorCode = "11";
		errorText = "Trying to send from an account that is not logged in";
	}

error:
	if (!retVal)
	{
		errorCode = "1";
		errorText = "Invalid parameter. Please double check the passed parameters.";
		syslog(LOG_INFO, "%s: block user failed", __FUNCTION__);
	}

	if (accountKey)
	{
		free(accountKey);
	}

	if (success)
	{
		json_object_object_add(payload, "returnValue", json_object_new_boolean(TRUE));
	}
	else
	{
		json_object_object_add(payload, "returnValue", json_object_new_boolean(FALSE));
		json_object_object_add(payload, "errorCode", json_object_new_string(errorCode));
		json_object_object_add(payload, "errorText", json_object_new_string(errorText));
	}

	retVal = LSMessageReturn(lshandle, message, json_object_to_json_string(payload), &lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	return success;
}

static bool getBuddyList(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal;
	//bool success = TRUE;
	LSError lserror;
	LSErrorInit(&lserror);

	/* Passed parameters */
	const char *serviceName = "";
	const char *username = "";
	bool subscribe = FALSE;

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	// get the message's payload (json object)
	json_t *object = LSMessageGetPayloadJSON(message);

	if (!object)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_string(object, "serviceName", &serviceName);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_string(object, "username", &username);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_bool(object, "subscribe", &subscribe);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	syslog(LOG_INFO, "Parameters: serviceName %s", serviceName);

	/* subscribe to the buddy list if subscribe:true is present. LSSubscriptionProcess takes care of this for us */
	bool subscribed;
	retVal = LSSubscriptionProcess(lshandle, message, &subscribed, &lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	/*
	 * Send over the full buddy list if the account is already logged in
	 */
	char *accountKey = getAccountKey(username, serviceName);
	PurpleAccount *account = g_hash_table_lookup(onlineAccountData, accountKey);
	if (account != NULL)
	{
		respondWithFullBuddyList(account, (char*)serviceName, (char*)username);
	}

	error: LSErrorFree(&lserror);
	if (accountKey)
	{
		free(accountKey);
	}
	return TRUE;
}

static bool sendMessage(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal;
	//bool success = TRUE;
	LSError lserror;
	LSErrorInit(&lserror);

	/* Passed parameters */
	const char *serviceName = "";
	const char *username = "";
	const char *usernameTo = "";
	const char *messageText = "";
	char *IRCusernameTo = "";

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	// get the message's payload (json object)
	char *payload = strdup(LSMessageGetPayload(message));

	if (!payload)
	{
		//success = FALSE;
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		//success = FALSE;
		goto error;
	}

	serviceName = getField(params, "serviceName");
	if (!serviceName)
	{
		//success = FALSE;
		goto error;
	}

	username = getField(params, "username");
	if (!username)
	{
		//success = FALSE;
		goto error;
	}
	usernameTo = getField(params, "usernameTo");
	if (!usernameTo)
	{
		//success = FALSE;
		goto error;
	}
	messageText = getField(params, "messageText");
	if (!messageText)
	{
		//success = FALSE;
		goto error;
	}

	char *messageTextUnescaped = g_strcompress(messageText);

	char *accountKey = getAccountKey(username, serviceName);

	PurpleAccount *accountToSendFrom = g_hash_table_lookup(onlineAccountData, accountKey);
	if (accountToSendFrom == NULL)
	{
		retVal
				= LSMessageReturn(
						lshandle,
						message,
						"{\"returnValue\":false, \"errorCode\":\"11\", \"errorText\":\"Trying to send from an account that is not logged in\"}",
						&lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
		//success = FALSE;
		goto error;
	}

	if (strcmp(serviceName, "irc") == 0 && strstr(usernameTo, "/join ") != NULL)
	{
		PurpleChat *chat;
		GHashTable *hash = NULL;
		PurpleConnection *gc;
		PurpleConversation *purpleConversation;

		//Remove join from usernameTo
		GString *IRCusernameTo = g_string_new(usernameTo);
		g_string_erase(IRCusernameTo, 0, 6);

		syslog(LOG_INFO, "Joining IRC channel: %s",IRCusernameTo->str);

		gc = purple_account_get_connection(accountToSendFrom);

		if (!(purpleConversation = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, IRCusernameTo->str, accountToSendFrom))) {
			purpleConversation = purple_conversation_new(PURPLE_CONV_TYPE_CHAT, accountToSendFrom, IRCusernameTo->str);
			purple_conv_chat_left(PURPLE_CONV_CHAT(purpleConversation));
		} else {
			purple_conversation_present(purpleConversation);
		}

		chat = purple_blist_find_chat(accountToSendFrom, IRCusernameTo->str);
		if (chat == NULL) {
			PurplePluginProtocolInfo *info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
			if (info->chat_info_defaults != NULL)
				hash = info->chat_info_defaults(gc, IRCusernameTo->str);
		} else {
			hash = purple_chat_get_components(chat);
		}
		serv_join_chat(gc, hash);
		if (chat == NULL && hash != NULL)
			g_hash_table_destroy(hash);
	}
	else
	{
		//If SIPE append sip: to username if required!
		if (strcmp(serviceName, "sipe") == 0 && strstr(usernameTo, "sip:") == NULL)
		{
			char *SIPEUserName = NULL;
			SIPEUserName = (char *)calloc(strlen("sip:") + strlen(usernameTo) + 1, sizeof(char));
			strcat(SIPEUserName, "sip:");
			strcat(SIPEUserName, usernameTo);

			PurpleConversation *purpleConversation = purple_conversation_new(PURPLE_CONV_TYPE_IM, accountToSendFrom, SIPEUserName);
			purple_conv_im_send(purple_conversation_get_im_data(purpleConversation), messageTextUnescaped);
		}
		else
		{
			PurpleConversation *purpleConversation = purple_conversation_new(PURPLE_CONV_TYPE_IM, accountToSendFrom, usernameTo);
			purple_conv_im_send(purple_conversation_get_im_data(purpleConversation), messageTextUnescaped);
		}
	}

	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":true}", &lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}

	error: LSErrorFree(&lserror);
	if (messageTextUnescaped)
	{
		free(messageTextUnescaped);
	}
	if (payload)
	{
		free(payload);
	}
	if (accountKey)
	{
		free(accountKey);
	}
	if (!is_error(params)) 
	{
		json_object_put(params);
	}
	return TRUE;
}

static bool registerForIncomingMessages(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal;
	//bool success = TRUE;
	LSError lserror;
	LSErrorInit(&lserror);

	/* Passed parameters */
	bool subscribe = FALSE;

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	// get the message's payload (json object)
	json_t *object = LSMessageGetPayloadJSON(message);

	if (!object)
	{
		//success = FALSE;
		goto error;
	}

	retVal = json_get_bool(object, "subscribe", &subscribe);
	if (!retVal)
	{
		//success = FALSE;
		goto error;
	}

	if (LSMessageIsSubscription(message))
	{
		bool subscribed;
		retVal = LSSubscriptionProcess(lshandle, message, &subscribed, &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
			retVal = LSMessageReply(lshandle, message,
					"{\"returnValue\": false, \"errorText\": \"Subscription error\"}", &lserror);
			if (!retVal)
			{
				LSErrorPrint(&lserror, stderr);
			}
			goto error;
		}

	}
	else
	{
		retVal = LSMessageReply(lshandle, message,
				"{\"returnValue\": false, \"errorText\": \"We were expecting a subscribe type message,"
					" but we did not receive one.\"}", &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
	}

	error: LSErrorFree(&lserror);
	return TRUE;
}







static bool enable(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	LSError lserror;
	LSErrorInit(&lserror);
	queuePresenceUpdates(TRUE);
	LSMessageReturn(lshandle, message, "{\"returnValue\":true}", &lserror);
	return TRUE;
}

static bool disable(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	LSError lserror;
	LSErrorInit(&lserror);
	//queuePresenceUpdates(FALSE);
	purple_timeout_add_seconds(DISABLE_QUEUE_TIMEOUT_SECONDS, queuePresenceUpdatesTimer, NULL);
	LSMessageReturn(lshandle, message, "{\"returnValue\":true}", &lserror);
	return TRUE;
}







static bool deviceConnectionClosed(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool success = TRUE;
	LSError lserror;

	/* Passed parameters */
	const char *ipAddress = "";

	PurpleAccount *account;
	GSList *accountToLogoutList = NULL;
	GList *onlineAndPendingAccountKeys = NULL;
	GList *iterator = NULL;
	char *accountKey = "";
	GSList *accountIterator = NULL;

	syslog(LOG_INFO, "%s called.", __FUNCTION__);

	LSErrorInit(&lserror);

	// get the message's payload (json object)
	json_t *object = LSMessageGetPayloadJSON(message);

	if (!object)
	{
		success = FALSE;
		goto error;
	}

	bool retVal = json_get_string(object, "ipAddress", &ipAddress);
	if (!retVal)
	{
		success = FALSE;
		goto error;
	}

	syslog(LOG_INFO, "deviceConnectionClosed");

	onlineAndPendingAccountKeys = g_hash_table_get_keys(ipAddressesBoundTo);
	for (iterator = onlineAndPendingAccountKeys; iterator != NULL; iterator = g_list_next(iterator))
	{
		accountKey = iterator->data;
		char *accountBoundToIpAddress = g_hash_table_lookup(ipAddressesBoundTo, accountKey);
		if (accountBoundToIpAddress != NULL && strcmp(accountBoundToIpAddress, "") != 0 && strcmp(ipAddress,
				accountBoundToIpAddress) == 0)
		{
			bool accountWasLoggedIn = FALSE;

			account = g_hash_table_lookup(onlineAccountData, accountKey);
			if (account == NULL)
			{
				account = g_hash_table_lookup(pendingAccountData, accountKey);
				if (account == NULL)
				{
					syslog(LOG_INFO, "account was not found in the hash");
					continue;
				}
				syslog(LOG_INFO, "Abandoning login");
			}
			else
			{
				accountWasLoggedIn = TRUE;
				syslog(LOG_INFO, "Logging out");
			}

			if (g_hash_table_lookup(onlineAccountData, accountKey) != NULL)
			{
				g_hash_table_remove(onlineAccountData, accountKey);
			}
			if (g_hash_table_lookup(pendingAccountData, accountKey) != NULL)
			{
				g_hash_table_remove(pendingAccountData, accountKey);
			}
			if (g_hash_table_lookup(offlineAccountData, accountKey) == NULL)
			{
				/*
				 * Keep the PurpleAccount struct to reuse in future logins
				 */
				g_hash_table_insert(offlineAccountData, accountKey, account);
			}
			
			purple_account_disconnect(account);

			accountToLogoutList = g_slist_append(accountToLogoutList, account);

			char *serviceName = getServiceNameFromPrplProtocolId(account);
			char *username = getJavaFriendlyUsername(account->username, serviceName);
			char *accountKey = getAccountKeyFromPurpleAccount(account);
			char *connectionType = g_hash_table_lookup(connectionTypeData, accountKey);
			if (connectionType == NULL)
			{
				connectionType = "";
			}

			GString *jsonResponse = g_string_new("{\"serviceName\":\"");
			g_string_append(jsonResponse, serviceName);
			g_string_append(jsonResponse, "\",  \"username\":\"");
			g_string_append(jsonResponse, username);

			if (accountWasLoggedIn)
			{
				g_string_append(
						jsonResponse,
						"\", \"returnValue\":false, \"errorCode\":\"AcctMgr_Network_Error\", \"errorText\":\"Connection failure\", \"connectionStatus\":\"loggedOut\", \"connectionType\":\"");
			}
			else
			{
				g_string_append(
						jsonResponse,
						"\", \"returnValue\":false, \"errorCode\":\"AcctMgr_Network_Error\", \"errorText\":\"Connection failure\", \"connectionType\":\"");
			}
			g_string_append(jsonResponse, connectionType);
			g_string_append(jsonResponse, "\"}");

			g_hash_table_remove(onlineAccountData, accountKey);
			// We can't remove this guy since we're iterating through its keys. We'll remove it after the break
			//g_hash_table_remove(ipAddressesBoundTo, accountKey);

			LSError lserror;
			LSErrorInit(&lserror);

			LSMessage *loginMsg = g_hash_table_lookup(loginMessages, accountKey);
			if (loginMsg != NULL)
			{
				retVal = LSMessageReply(serviceHandle, loginMsg, jsonResponse->str, &lserror);
				if (!retVal)
				{
					LSErrorPrint(&lserror, stderr);
				}
				g_hash_table_remove(loginMessages, accountKey);
				LSMessageUnref(loginMsg);
			}
			LSErrorFree(&lserror);
			free(serviceName);
			free(username);
			free(accountKey);
			g_string_free(jsonResponse, TRUE);
		}
	}

	if (g_slist_length(accountToLogoutList) == 0)
	{
		syslog(LOG_INFO, "No accounts were connected on the requested ip address");
		retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":true}", &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
		}
	}
	else
	{
		for (accountIterator = accountToLogoutList; accountIterator != NULL; accountIterator = accountIterator->next)
		{
			account = (PurpleAccount *) accountIterator->data;
			char *serviceName = getServiceNameFromPrplProtocolId(account);
			char *username = getJavaFriendlyUsername(account->username, serviceName);
			char *accountKey = getAccountKeyFromPurpleAccount(account);
			g_hash_table_remove(ipAddressesBoundTo, accountKey);

			free(serviceName);
			free(username);
			free(accountKey);
		}
	}

	error: if (!success)
	{
		retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);
	}
	else
	{
		retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":true}", &lserror);
	}

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}

	LSErrorFree(&lserror);
	return TRUE;
}

static bool setserver(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	char *payload = strdup(LSMessageGetPayload(message));
	bool subscribe = FALSE;
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	const char *payloadServer = NULL;
	const char *payloadServerPort = NULL;
	const char *payloadServerTLS = NULL;
	const char *payloadServerProxy = NULL;
	const char *payloadServerLogin = NULL;
	const char *payloadversion = NULL;
	const char *payloadhideID = NULL;
	const char *payloadUserAgent = NULL;
	const char *serviceName = NULL;

	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}
	subscribe = json_object_get_boolean(json_object_object_get(params, "subscribe"));

	payloadServer = getField(params, "ServerName");
	if (!payloadServer)
	{
		printf("ERROR, ServerName not specified.\n");
		goto error;
	}
	serviceName = getField(params, "serviceName");
	if (!serviceName)
	{
		printf("ERROR, serviceName not specified.\n");
		goto error;
	}
	payloadServerPort = getField(params, "ServerPort");
	if (strcmp(serviceName, "sipe") != 0)
	{
		if (!payloadServerPort)
		{
			printf("ERROR, ServerPort not specified.\n");
			goto error;
		}
	}
	payloadServerTLS = getField(params, "TLS");
	if (!payloadServerTLS)
	{
		printf("ERROR, TLS not specified. (TLS = TRUE or FALSE)\n");
		goto error;
	}
	if (strcmp(serviceName, "sipe") == 0)
	{
		payloadServerLogin = getField(params, "ServerLogin");
		payloadServerProxy = getField(params, "Proxy");
		payloadUserAgent = getField(params, "UserAgent");
	}
	if (strcmp(serviceName, "jabber") == 0)
	{
		payloadServerLogin = getField(params, "ServerLogin"); //Jabber Resource
	}
	if (strcmp(serviceName, "xfire") == 0)
	{
		payloadversion = getField(params, "ServerLogin"); //Xfire version
	}
	if (strcmp(serviceName, "lcs") == 0)
	{
		payloadhideID = getField(params, "ServerLogin"); //Hide ID?
	}

	//Set Filename
	char *filename = NULL;
	filename = (char *)calloc(strlen("/var/preferences/org.webosinternals.messaging/") + strlen(serviceName) + strlen(".cfg") + 1, sizeof(char));
	strcat(filename, "/var/preferences/org.webosinternals.messaging/");
	strcat(filename, serviceName);
	strcat(filename, ".cfg");

	if (strcmp(serviceName, "jabber") == 0)
	{
		//Set Jabber Server
		JabberServer = (char *)calloc(strlen(payloadServer) + 1,sizeof(char));
		strcat(JabberServer, payloadServer);
		
		//Set Jabber Server Port
		JabberServerPort = (char *)calloc(strlen(payloadServerPort) + 1,sizeof(char));
		strcat(JabberServerPort, payloadServerPort);

		//Set Jabber Server TLS
		if (payloadServerTLS == "true")
		{
			JabberServerTLS = TRUE;
		}
		else
		{
			JabberServerTLS = FALSE;
		}

		//Set Resource
		if (payloadServerLogin == NULL || strcmp(payloadServerLogin, "") == 0)
		{
			JabberResource = (char *)calloc(strlen("$$BLANK$$") + 1,sizeof(char));
			strcat(JabberResource, "$$BLANK$$");
		}
		else
		{
			JabberResource = (char *)calloc(strlen(payloadServerLogin) + 1,sizeof(char));
			strcat(JabberResource, payloadServerLogin);
		}
	}
	if (strcmp(serviceName, "sipe") == 0)
	{
		//Set SIPE Server
		if (payloadServer == NULL || strcmp(payloadServer, "") == 0)
		{
			SIPEServer = (char *)calloc(strlen("$$BLANK$$") + 1,sizeof(char));
			strcat(SIPEServer, "$$BLANK$$");
		}
		else
		{
			SIPEServer = (char *)calloc(strlen(payloadServer) + 1,sizeof(char));
			strcat(SIPEServer, payloadServer);
		}

		//Set SIPE Server Port
		if (payloadServerPort == NULL || strcmp(payloadServerPort, "") == 0)
		{
			SIPEServerPort = (char *)calloc(strlen("$$BLANK$$") + 1,sizeof(char));
			strcat(SIPEServerPort, "$$BLANK$$");
		}
		else
		{
			SIPEServerPort = (char *)calloc(strlen(payloadServerPort) + 1,sizeof(char));
			strcat(SIPEServerPort, payloadServerPort);
		}

		//Set SIPE Server TLS
		if (payloadServerTLS == "true")
		{
			SIPEServerTLS = TRUE;
		}
		else
		{
			SIPEServerTLS = FALSE;
		}

		//Set SIPE Server Proxy
		if (payloadServerProxy == "true")
		{
			SIPEServerProxy = TRUE;
		}
		else
		{
			SIPEServerProxy = FALSE;
		}

		//Set SIPE Server Login
		if (payloadServerLogin == NULL || strcmp(payloadServerLogin, "") == 0)
		{
			SIPEServerLogin = (char *)calloc(strlen("$$BLANK$$") + 1, sizeof(char));
			strcat(SIPEServerLogin, "$$BLANK$$");
		}
		else
		{
			SIPEServerLogin = (char *)calloc(strlen(payloadServerLogin) + 1,sizeof(char));
			strcat(SIPEServerLogin, payloadServerLogin);
		}

		//Set SIPE User Agent
		if (payloadUserAgent == NULL || strcmp(payloadUserAgent, "") == 0)
		{
			SIPEUserAgent = (char *)calloc(strlen("$$BLANK$$") + 1, sizeof(char));
			strcat(SIPEUserAgent, "$$BLANK$$");
		}
		else
		{
			SIPEUserAgent = (char *)calloc(strlen(payloadUserAgent) + 1,sizeof(char));
			strcat(SIPEUserAgent, payloadUserAgent);
		}
	}
	if (strcmp(serviceName, "lcs") == 0)
	{
		//Set Sametime Server
		SametimeServer = (char *)calloc(strlen(payloadServer) + 1,sizeof(char));
		strcat(SametimeServer, payloadServer);
		
		//Set Sametime Server Port
		SametimeServerPort = (char *)calloc(strlen(payloadServerPort) + 1,sizeof(char));
		strcat(SametimeServerPort, payloadServerPort);

		//Set Sametime Server TLS
		if (payloadServerTLS == "true")
		{
			SametimeServerTLS = TRUE;
		}
		else
		{
			SametimeServerTLS = FALSE;
		}

		//Set hide ID
		if (payloadhideID == "true")
		{
			SametimehideID = TRUE;
		}
		else
		{
			SametimehideID = FALSE;
		}
	}
	if (strcmp(serviceName, "gwim") == 0)
	{
		//Set Group Wsie Server
		gwimServer = (char *)calloc(strlen(payloadServer) + 1,sizeof(char));
		strcat(gwimServer, payloadServer);
		
		//Set Group Wise Server Port
		gwimServerPort = (char *)calloc(strlen(payloadServerPort) + 1,sizeof(char));
		strcat(gwimServerPort, payloadServerPort);

		//Set Group Wise Server TLS
		if (payloadServerTLS == "true")
		{
			gwimServerTLS = TRUE;
		}
		else
		{
			gwimServerTLS = FALSE;
		}
	}
	if (strcmp(serviceName, "myspace") == 0)
	{
		//Set MySpace Server
		MySpaceServer = (char *)calloc(strlen(payloadServer) + 1,sizeof(char));
		strcat(MySpaceServer, payloadServer);
		
		//Set MySpace Server Port
		MySpaceServerPort = (char *)calloc(strlen(payloadServerPort) + 1,sizeof(char));
		strcat(MySpaceServerPort, payloadServerPort);
	}
	if (strcmp(serviceName, "gadu") == 0)
	{
		//Set Gadu Gadu Server
		GaduServer = (char *)calloc(strlen(payloadServer) + 1,sizeof(char));
		strcat(GaduServer, payloadServer);
	}
	if (strcmp(serviceName, "xfire") == 0)
	{
		//Set XFire Server
		XFireServer = (char *)calloc(strlen(payloadServer) + 1,sizeof(char));
		strcat(XFireServer, payloadServer);
		
		//Set XFire Server Port
		XFireServerPort = (char *)calloc(strlen(payloadServerPort) + 1,sizeof(char));
		strcat(XFireServerPort, payloadServerPort);

		//Set XFire version
		if (payloadversion == NULL || strcmp(payloadversion, "") == 0)
		{
			XFireversion = (char *)calloc(strlen(payloadversion) + 1,sizeof(char));
			strcat(XFireversion, "122");
		}
		else
		{
			XFireversion = (char *)calloc(strlen(payloadversion) + 1,sizeof(char));
			strcat(XFireversion, payloadversion);
		}
	}

	FILE * hFile;

	printf("Opening %s\n",filename);
	hFile = fopen(filename, "w");
	if (hFile == NULL)
	{
		printf("Error! Unable to create %s.\n",filename);
		goto error;
	}
	else
	{
		printf("Opened file %s\n",filename);
		
		if (strcmp(serviceName, "sipe") == 0)
		{
			printf("Setting %s server address to: %s:%s\n", serviceName, SIPEServer, SIPEServerPort);
			printf("Setting %s server TLS to: %s\n", serviceName,payloadServerTLS);
			printf("Setting %s server login to: %s\n", serviceName,SIPEServerLogin);
			printf("Setting %s server proxy to: %s\n", serviceName,payloadServerProxy);
			printf("Setting %s server user agent to: %s\n", serviceName,SIPEUserAgent);
			fprintf(hFile, "%s:%s:%s:%s:%s:%s", SIPEServer, SIPEServerPort, payloadServerTLS, SIPEServerLogin, payloadServerProxy, SIPEUserAgent);
		}
		else
		{
			if (strcmp(serviceName, "jabber") == 0)
			{
				printf("Setting %s server address to: %s:%s\n", serviceName, payloadServer, payloadServerPort);
				printf("Setting %s server TLS to: %s\n", serviceName,payloadServerTLS);
				printf("Setting %s server resource to: %s\n", serviceName,JabberResource);
				fprintf(hFile, "%s:%s:%s:%s", payloadServer, payloadServerPort, payloadServerTLS, JabberResource);
			}
			else
			{
				if (strcmp(serviceName, "lcs") == 0)
				{
					printf("Setting %s server address to: %s:%s\n", serviceName, payloadServer, payloadServerPort);
					printf("Setting %s server TLS to: %s\n", serviceName,payloadServerTLS);
					printf("Setting %s server hideID to: %s\n", serviceName,payloadhideID);
					fprintf(hFile, "%s:%s:%s:%s", payloadServer, payloadServerPort, payloadServerTLS, payloadhideID);
				}
				else
				{
					if (strcmp(serviceName, "xfire") == 0)
					{
						printf("Setting %s server address to: %s:%s\n", serviceName, payloadServer, payloadServerPort);
						printf("Setting %s server TLS to: %s\n", serviceName,payloadServerTLS);
						printf("Setting %s server version to: %s\n", serviceName,payloadversion);
						fprintf(hFile, "%s:%s:%s:%s", payloadServer, payloadServerPort, payloadServerTLS, payloadversion);
					}
					else
					{
						printf("Setting %s server address to: %s:%s\n", serviceName, payloadServer, payloadServerPort);
						printf("Setting %s server TLS to: %s\n", serviceName,payloadServerTLS);
						fprintf(hFile, "%s:%s:%s", payloadServer, payloadServerPort, payloadServerTLS);
					}
				}
			}
		}

		printf("%s server has been set.\n", serviceName);

		// Close file
		fclose(hFile);
	}

	free (payload);

	//Set Sipe Server Variable Correctly
	if (strcmp(serviceName, "sipe") == 0)
	{
		//Set SIPE Server
		if (strcmp(SIPEServer, "$$BLANK$$") == 0)
		{
			SIPEServer = "";
		}

		//Set SIPE Server Login
		if (strcmp(SIPEServerLogin, "$$BLANK$$") == 0)
		{
			SIPEServerLogin = "";
		}

		//Set SIPE Server Port
		if (strcmp(SIPEServerPort, "$$BLANK$$") == 0)
		{
			SIPEServerPort = "";
		}

		//Set SIPE User Agent
		if (strcmp(SIPEUserAgent, "$$BLANK$$") == 0)
		{
			SIPEUserAgent = "";
		}
	}

	//Set Jabber Resource Variable Correctly
	if (strcmp(serviceName, "jabber") == 0)
	{
		//Set SIPE Server
		if (strcmp(JabberResource, "$$BLANK$$") == 0)
		{
			JabberResource = "";
		}
	}

	//Set XFire version Variable Correctly
	if (strcmp(serviceName, "xfire") == 0)
	{
		//Default to 122
		if (strcmp(XFireversion, "$$BLANK$$") == 0)
		{
			XFireversion = "122";
		}
	}

	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

static bool getserver(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool subscribe = FALSE;
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	char *payload = strdup(LSMessageGetPayload(message));
	const char *serviceName = NULL;
	char *ServerTLS = "false";
	char *ServerProxy = "false";
	char *hideID = "false";

	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}
	subscribe = json_object_get_boolean(json_object_object_get(params, "subscribe"));

	serviceName = getField(params, "serviceName");
	if (!serviceName)
	{
		printf("ERROR, serviceName not specified.\n");
		goto error;
	}

	struct json_object *responsePayload = json_object_new_object();

	//Load the server details
	loadserver(serviceName);

	//Is this jabber?
	if (strcmp(serviceName, "jabber") == 0)
	{
		printf("%s server address: %s\n",serviceName, JabberServer);
		printf("%s server port: %s\n",serviceName, JabberServerPort);

		json_object_object_add(responsePayload, "ServerName", json_object_new_string(JabberServer));
		json_object_object_add(responsePayload, "ServerPort", json_object_new_string(JabberServerPort));

		//Set Jabber Server TLS?
		if (JabberServerTLS)
		{
			ServerTLS = "true";
		}

		printf("%s server TLS: %s\n",serviceName, ServerTLS);
		json_object_object_add(responsePayload, "ServerTLS", json_object_new_string(ServerTLS));

		//Resource?
		json_object_object_add(responsePayload, "Resource", json_object_new_string(JabberResource));
		printf("%s server resource: %s\n",serviceName, JabberResource);
	}
	//Is this XFire?
	if (strcmp(serviceName, "xfire") == 0)
	{
		printf("%s server address: %s\n",serviceName, XFireServer);
		printf("%s server port: %s\n",serviceName, XFireServerPort);

		json_object_object_add(responsePayload, "ServerName", json_object_new_string(XFireServer));
		json_object_object_add(responsePayload, "ServerPort", json_object_new_string(XFireServerPort));
		
		//Version?
		json_object_object_add(responsePayload, "version", json_object_new_string(XFireversion));
		printf("%s server version: %s\n",serviceName, XFireversion);
	}
	//Is this SIPE?
	if (strcmp(serviceName, "sipe") == 0)
	{
		printf("%s server address: %s\n",serviceName, SIPEServer);
		printf("%s server port: %s\n",serviceName, SIPEServerPort);
		printf("%s server login: %s\n",serviceName, SIPEServerLogin);
		printf("%s user agent: %s\n",serviceName, SIPEUserAgent);

		json_object_object_add(responsePayload, "ServerName", json_object_new_string(SIPEServer));
		json_object_object_add(responsePayload, "ServerPort", json_object_new_string(SIPEServerPort));
		json_object_object_add(responsePayload, "ServerLogin", json_object_new_string(SIPEServerLogin));
		json_object_object_add(responsePayload, "UserAgent", json_object_new_string(SIPEUserAgent));

		//Set SIPE Server TLS?
		if (SIPEServerTLS)
		{
			ServerTLS = "true";
		}

		printf("%s server TLS: %s\n",serviceName, ServerTLS);
		json_object_object_add(responsePayload, "ServerTLS", json_object_new_string(ServerTLS));

		//Set SIPE Server Proxy?
		if (SIPEServerProxy)
		{
			ServerProxy = "true";
		}

		printf("%s server Proxy: %s\n",serviceName, ServerProxy);
		json_object_object_add(responsePayload, "ServerProxy", json_object_new_string(ServerProxy));
	}
	//Is this sametime?
	if (strcmp(serviceName, "lcs") == 0)
	{
		printf("%s server address: %s\n",serviceName, SametimeServer);
		printf("%s server port: %s\n",serviceName, SametimeServerPort);

		json_object_object_add(responsePayload, "ServerName", json_object_new_string(SametimeServer));
		json_object_object_add(responsePayload, "ServerPort", json_object_new_string(SametimeServerPort));

		//Set Sametime Server TLS?
		if (SametimeServerTLS)
		{
			ServerTLS = "true";
		}

		printf("%s server TLS: %s\n",serviceName, ServerTLS);
		json_object_object_add(responsePayload, "ServerTLS", json_object_new_string(ServerTLS));

		//Set Sametime Hide ID?
		if (SametimehideID)
		{
			hideID = "true";
		}

		printf("%s server TLS: %s\n",serviceName, hideID);
		json_object_object_add(responsePayload, "hideID", json_object_new_string(hideID));
	}
	//Is this Group Wise?
	if (strcmp(serviceName, "gwim") == 0)
	{
		printf("%s server address: %s\n",serviceName, gwimServer);
		printf("%s server port: %s\n",serviceName, gwimServerPort);

		json_object_object_add(responsePayload, "ServerName", json_object_new_string(gwimServer));
		json_object_object_add(responsePayload, "ServerPort", json_object_new_string(gwimServerPort));

		//Set Group Wise Server TLS?
		if (gwimServerTLS)
		{
			ServerTLS = "true";
		}

		printf("%s server TLS: %s\n",serviceName, ServerTLS);
		json_object_object_add(responsePayload, "ServerTLS", json_object_new_string(ServerTLS));
	}
	//Is this MySpace?
	if (strcmp(serviceName, "myspace") == 0)
	{
		printf("%s server address: %s\n",serviceName, MySpaceServer);
		printf("%s server port: %s\n",serviceName, MySpaceServerPort);

		json_object_object_add(responsePayload, "ServerName", json_object_new_string(MySpaceServer));
		json_object_object_add(responsePayload, "ServerPort", json_object_new_string(MySpaceServerPort));
	}
	//Is this Gadu Gadu?
	if (strcmp(serviceName, "gadu") == 0)
	{
		printf("%s server address: %s\n",serviceName, GaduServer);

		json_object_object_add(responsePayload, "ServerName", json_object_new_string(GaduServer));
	}

	//Return information
	retVal = LSMessageReply(serviceHandle, message, json_object_to_json_string(responsePayload),&lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}

	free (payload);
	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

static bool clearserver(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool subscribe = FALSE;
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	char *payload = strdup(LSMessageGetPayload(message));
	const char *serviceName = NULL;
	
	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}
	subscribe = json_object_get_boolean(json_object_object_get(params, "subscribe"));

	serviceName = getField(params, "serviceName");
	if (!serviceName)
	{
		printf("ERROR, serviceName not specified.\n");
		goto error;
	}

	//Set Filename
	char *filename = NULL;
	filename = (char *)calloc(strlen("/var/preferences/org.webosinternals.messaging/") + strlen(serviceName) + strlen(".cfg") + 1, sizeof(char));
	strcat(filename, "/var/preferences/org.webosinternals.messaging/");
	strcat(filename, serviceName);
	strcat(filename, ".cfg");

	//Remove config file
	remove(filename);

	if (strcmp(serviceName, "jabber") == 0)
	{
		//Clear Jabber Server Details
		JabberServer = "";
		JabberServerPort = "";
		JabberServerTLS = "";
		JabberResource = "";
	}
	if (strcmp(serviceName, "sipe") == 0)
	{
		//Clear SIPE Server Details
		SIPEServer = "";
		SIPEServerPort = "";
		SIPEServerTLS = "";
		SIPEServerProxy = "";
		SIPEServerLogin = "";
		SIPEUserAgent = "";
	}
	if (strcmp(serviceName, "lcs") == 0)
	{
		//Clear Sametime Server Details
		SametimeServer = "";
		SametimeServerPort = "";
		SametimeServerTLS = "";
		SametimehideID = "";
	}
	if (strcmp(serviceName, "gwim") == 0)
	{
		//Clear Group Wise Server Details
		gwimServer = "";
		gwimServerPort = "";
		gwimServerTLS = "";
	}
	if (strcmp(serviceName, "myspace") == 0)
	{
		//Clear MySpace Server Details
		MySpaceServer = "";
		MySpaceServerPort = "";
	}
	if (strcmp(serviceName, "gadu") == 0)
	{
		//Clear Gadu Gadu Server Details
		GaduServer = "";
	}
	if (strcmp(serviceName, "xfire") == 0)
	{
		//Clear XFire Server Details
		XFireServer = "";
		XFireServerPort = "";
		XFireversion = "";
	}
	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

static bool EnableContactsReadWrite(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	char *payload = strdup(LSMessageGetPayload(message));
	const char *PluginName = NULL;
	
	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}

	PluginName = getField(params, "PluginName");
	if (!PluginName)
	{
		printf("ERROR, PluginName not specified.\n");
		goto error;
	}

	ReadWriteContacts (PluginName);

	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

static bool setpreference(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	char *payload = strdup(LSMessageGetPayload(message));
	bool subscribe = FALSE;
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	const char *state = NULL;
	const char *serviceName = NULL;
	const char *preference = NULL;

	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}
	subscribe = json_object_get_boolean(json_object_object_get(params, "subscribe"));

	serviceName = getField(params, "serviceName");
	if (!serviceName)
	{
		printf("ERROR, serviceName not specified.\n");
		goto error;
	}
	preference = getField(params, "preference");
	if (!preference)
	{
		printf("ERROR, preference not specified.\n");
		goto error;
	}
	state = getField(params, "state");
	if (!state)
	{
		printf("ERROR, state not specified.\n");
		goto error;
	}
	
	//Set Filename
	char *filename = NULL;
	filename = (char *)calloc(strlen("/var/preferences/org.webosinternals.messaging/") + strlen(serviceName) + strlen(".") + strlen(preference) + 1, sizeof(char));
	strcat(filename, "/var/preferences/org.webosinternals.messaging/");
	strcat(filename, serviceName);
	strcat(filename, ".");
	strcat(filename, preference);

	if (strcmp(state, "true") == 0)
	{
		//Open file
		FILE * hFile;

		printf("Opening %s\n",filename);
		hFile = fopen(filename, "w");
		if (hFile == NULL)
		{
			printf("Error! Unable to create %s.\n",filename);
			goto error;
		}
		else
		{
			printf("Opened file %s\n",filename);

			// Close file
			fclose(hFile);
		}
	}
	else
	{
		//Remove config file
		remove(filename);
	}

	free (payload);

	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

static bool getpreference(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool subscribe = FALSE;
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	char *payload = strdup(LSMessageGetPayload(message));
	const char *serviceName = NULL;
	const char *preference = NULL;
	char *Alias = "false";
	char *Avatar = "false";

	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}
	subscribe = json_object_get_boolean(json_object_object_get(params, "subscribe"));

	serviceName = getField(params, "serviceName");
	if (!serviceName)
	{
		printf("ERROR, serviceName not specified.\n");
		goto error;
	}
	preference = getField(params, "preference");
	if (!preference)
	{
		printf("ERROR, preference not specified.\n");
		goto error;
	}

	struct json_object *responsePayload = json_object_new_object();

	//Load the preferences
	loadpreference(serviceName, preference);

	//Is this GTalk?
	if (strcmp(serviceName, "gmail") == 0)
	{
		//Load Avatars?
		if (GTalkAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this AOL?
	if (strcmp(serviceName, "aol") == 0)
	{
		//Load Avatars?
		if (AIMAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this Groupwise?
	if (strcmp(serviceName, "gwim") == 0)
	{
		//Load Avatars?
		if (gwimAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this MySpace?
	if (strcmp(serviceName, "myspace") == 0)
	{
		//Load Avatars?
		if (MySpaceAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this Gadu Gadu?
	if (strcmp(serviceName, "gadu") == 0)
	{
		//Load Avatars?
		if (GaduAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this Yahoo?
	if (strcmp(serviceName, "yahoo") == 0)
	{
		//Load Avatars?
		if (YahooAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this ICQ?
	if (strcmp(serviceName, "icq") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			//Load Avatars?
			if (ICQAvatar)
			{
				Avatar = "true";
			}
			else
			{
				Avatar = "false";
			}
			
			json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
		}
		if (strcmp(preference, "Alias") == 0)
		{
			//Load Avatars?
			if (ICQAlias)
			{
				Alias = "true";
			}
			else
			{
				Alias = "false";
			}
			
			json_object_object_add(responsePayload, preference, json_object_new_string((char*)Alias));
		}
	}
	//Is this jabber?
	if (strcmp(serviceName, "jabber") == 0)
	{
		//Load Avatars?
		if (JabberAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this Live?
	if (strcmp(serviceName, "live") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			//Load Avatars?
			if (LiveAvatar)
			{
				Avatar = "true";
			}
			else
			{
				Avatar = "false";
			}
			
			json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
		}
		if (strcmp(preference, "Alias") == 0)
		{
			//Load Avatars?
			if (LiveAlias)
			{
				Alias = "true";
			}
			else
			{
				Alias = "false";
			}
			
			json_object_object_add(responsePayload, preference, json_object_new_string((char*)Alias));
		}
	}
	//Is this SIPE?
	if (strcmp(serviceName, "sipe") == 0)
	{
		//Load Avatars?
		if (SIPEAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this QQ?
	if (strcmp(serviceName, "qqim") == 0)
	{
		//Load Avatars?
		if (QQAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this Facebook?
	if (strcmp(serviceName, "facebook") == 0)
	{
		//Load Avatars?
		if (FacebookAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this sametime?
	if (strcmp(serviceName, "lcs") == 0)
	{
		//Load Avatars?
		if (SametimeAvatar)
		{
			Avatar = "true";
		}
		else
		{
			Avatar = "false";
		}
		json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
	}
	//Is this XFire?
	if (strcmp(serviceName, "XFire") == 0)
	{
		if (strcmp(preference, "Avatar") == 0)
		{
			//Load Avatars?
			if (XFireAvatar)
			{
				Avatar = "true";
			}
			else
			{
				Avatar = "false";
			}
			
			json_object_object_add(responsePayload, preference, json_object_new_string((char*)Avatar));
		}
		if (strcmp(preference, "Alias") == 0)
		{
			//Load Avatars?
			if (XFireAlias)
			{
				Alias = "true";
			}
			else
			{
				Alias = "false";
			}
			
			json_object_object_add(responsePayload, preference, json_object_new_string((char*)Alias));
		}
	}
	
	//Return information
	retVal = LSMessageReply(serviceHandle, message, json_object_to_json_string(responsePayload),&lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}

	free (payload);
	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

static bool AcceptBadCert(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	char *payload = strdup(LSMessageGetPayload(message));

	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}

	free (payload);

	if (libpurpleInitialized == FALSE)
	{
		initializeLibpurple();
	}

	purple_prefs_remove("/purple/acceptbadcert");
	purple_prefs_add_string("/purple/acceptbadcert", "true");

	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

static bool RejectBadCert(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	char *payload = strdup(LSMessageGetPayload(message));

	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}

	free (payload);

	if (libpurpleInitialized == FALSE)
	{
		initializeLibpurple();
	}

	purple_prefs_remove("/purple/acceptbadcert");

	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

static bool GetAcceptBadCertSetting(LSHandle* lshandle, LSMessage *message, void *ctx)
{
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);
	char *payload = strdup(LSMessageGetPayload(message));

	if (!payload)
	{
		printf("ERROR, No payload.\n");
		goto error;
	}

	struct json_object *params = json_tokener_parse(payload);
	if (is_error(params))
	{
		printf("ERROR, Parameters not specified correctly.\n");
		goto error;
	}

	struct json_object *responsePayload = json_object_new_object();

	const char *acceptbadcert;
	acceptbadcert = purple_prefs_get_string("/purple/acceptbadcert");

	if (acceptbadcert != NULL)
	{
		json_object_object_add(responsePayload, "Setting", json_object_new_string("Accept"));
	}
	else
	{
		json_object_object_add(responsePayload, "Setting", json_object_new_string("Reject"));
	}

	//Return information
	retVal = LSMessageReply(serviceHandle, message, json_object_to_json_string(responsePayload),&lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}

	free (payload);
	return true;

	error:
	retVal = LSMessageReturn(lshandle, message, "{\"returnValue\":false}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
	}
	LSErrorFree(&lserror);
	free (payload);
	return true;
}

/*
 * End of service methods
 */

/*
 * Methods exposed over the bus:
 */
static LSMethod methods[] =
{
{ "login", login },
{ "logout", logout },
{ "getBuddyList", getBuddyList },
{ "registerForIncomingMessages", registerForIncomingMessages },
{ "sendMessage", sendMessage },
{ "setMyAvailability", setMyAvailability },
{ "setMyCustomMessage", setMyCustomMessage },
{ "getMyAvailability", getMyAvailability },
{ "deviceConnectionClosed", deviceConnectionClosed },
{ "enable", enable },
{ "disable", disable },
{ "blockBuddy", blockBuddy },
{ "setserver", setserver },
{ "getserver", getserver },
{ "clearserver", clearserver },
{ "EnableContactsReadWrite", EnableContactsReadWrite },
{ "setpreference", setpreference },
{ "getpreference", getpreference },
{ "AcceptBadCert", AcceptBadCert },
{ "GetAcceptBadCertSetting", GetAcceptBadCertSetting },
{ "RejectBadCert", RejectBadCert},
{ }, 
};

int main(int argc, char *argv[])
{
	/* lunaservice variables */
	bool retVal = FALSE;
	LSError lserror;
	LSErrorInit(&lserror);

	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	if (loop == NULL)
		goto error;

	syslog(LOG_INFO, "Registering %s ... ", dbusAddress);
	g_message("Registering %s ... ", dbusAddress);

	retVal = LSRegister(dbusAddress, &serviceHandle, &lserror);
	if (!retVal)
		goto error;

	retVal = LSRegisterCategory(serviceHandle, "/", methods, NULL, NULL, &lserror);
	if (!retVal)
		goto error;

	syslog(LOG_INFO, "Succeeded.");
	g_message("Succeeded.");

	retVal = LSGmainAttach(serviceHandle, loop, &lserror);
	if (!retVal)
		goto error;

	//TODO: replace the NULLs with real functions to prevent memory leaks
	onlineAccountData = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	pendingAccountData = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	accountLoginTimers = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	loginMessages = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	logoutMessages = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	ipAddressesBoundTo = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free);
	connectionTypeData = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free);
	offlineAccountData = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	g_main_loop_run(loop); 

	error: if (LSErrorIsSet(&lserror))
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	if (serviceHandle)
	{
		retVal = LSUnregister(serviceHandle, &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
		}
	}

	if (loop)
		g_main_loop_unref(loop);

	return 0;
}
