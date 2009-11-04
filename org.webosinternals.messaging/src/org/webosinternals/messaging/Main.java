package org.webosinternals.messaging;

import com.palm.oasis.activerecord.*;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;

public class Main {
    private static Batch batch;
    private static Database PalmDB = Database.getDatabase("/var/luna/data/dbdata/PalmDatabase.db3");

    public static void main(String[] args) throws IOException, ClassNotFoundException, SQLException {
        LogEntry("Messaging Plugins");
        LogEntry("Version: 1.1.0");
        LogEntry("By Greg Roll 2009");
        LogEntry("");

        //Check commandline arguments
        if ((args.length == 0))
        {
            LogEntry("CommandName: Must be specified. Options are: RestartLuna, InstallPlugin, RemovePlugin.");
            LogEntry ("");
            System.exit(1);
        }

        //Get CommandToRun
        String CommandName = args[0];

        if (CommandName.equalsIgnoreCase(""))
        {
            LogEntry("CommandName: Must be specified. Options are: RestartLuna, InstallPlugin, RemovePlugin.");
            LogEntry ("");
            return;
        }

        //Restart Luna?
        if (CommandName.equalsIgnoreCase("RestartLuna"))
        {
            RestartLuna();
            LogEntry ("");
            return;
        }

        //Check for plugin name
        if (!(args.length == 2))
        {
            LogEntry("PluginName: Must be specified. Options are: Live, Yahoo, ICQ, Jabber, SIPE, IRC, Sametime, Groupwsie and Facebook.");
            LogEntry ("");
            System.exit(1);
        }

        //Install Plugin?
        if (CommandName.equalsIgnoreCase("InstallPlugin"))
        {
            InstallPlugin(args[1]);

            //Close DB
            PalmDB.close();

            LogEntry ("");
            return;
        }

        //Remove Plugin?
        if (CommandName.equalsIgnoreCase("RemovePlugin"))
        {
            RemovePlugin(args[1]);

            //Close DB
            PalmDB.close();

            LogEntry ("");
            return;
        }

        LogEntry("CommandName: Must be specified. Options are: RestartLuna, InstallPlugin, RemovePlugin.");
        LogEntry ("");
        return;
    }

    private static void RestartLuna() throws IOException
    {
        LogEntry("Restarting Luna...");
        Runtime.getRuntime().exec((new StringBuilder()).append("killall -HUP LunaSysMgr").toString());
    }

    private static void InstallPlugin(String PluginName) throws IOException, ClassNotFoundException, SQLException
        {
            LogEntry("Messaging Plugin Installation...");
            LogEntry("");

            if (PluginName.equalsIgnoreCase(""))
            {
                LogEntry("PluginName: Must be specified. Options are: Live, Yahoo, ICQ, Jabber, SIPE, IRC, Sametime, Groupwise and Facebook.");
                return;
            }

            //Should we install the Live Plugin?
            if (PluginName.equalsIgnoreCase("Live"))
                {
                try {
                    InstallLivePlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Live Messenger Plugin Installation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we install the ICQ Plugin?
            if (PluginName.equalsIgnoreCase("ICQ"))
                {
                try {
                    InstallICQPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("ICQ Plugin Installation Error");
                    LogEntry (ex.toString());
                }
                return;
            }

            //Should we install the Facebook Plugin?
            if (PluginName.equalsIgnoreCase("Facebook"))
                {
                try {
                    InstallFacebookPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Facebook Plugin Installation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we install the Jabber Plugin?
            if (PluginName.equalsIgnoreCase("Jabber"))
                {
                try {
                    InstallJabberPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Jabber Plugin Installation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we install the SIPE Plugin?
            if (PluginName.equalsIgnoreCase("SIPE"))
                {
                try {
                    InstallSIPEPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("SIPE Plugin Installation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we install the IRC Plugin?
            if (PluginName.equalsIgnoreCase("IRC"))
                {
                try {
                    InstallIRCPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("IRC Plugin Installation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we install the Sametime Plugin?
            if (PluginName.equalsIgnoreCase("Sametime"))
                {
                try {
                    InstallSametimePlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Sametime Plugin Installation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we install the Groupwise Plugin?
            if (PluginName.equalsIgnoreCase("Groupwise"))
                {
                try {
                    InstallGroupwisePlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Groupwise Plugin Installation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we install the Yahoo Plugin?
            if (PluginName.equalsIgnoreCase("Palm"))
                {
                try {
                    EnablePalmPlugins();
                } catch (ActiveRecordException ex) {
                    LogEntry("Palm Plugin Installation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            LogEntry("PluginName: Must be specified. Options are: Live, Yahoo, ICQ, Jabber, SIPE, IRC, Sametime, Groupwise and Facebook.");
            return;
        }

    private static void RemovePlugin(String PluginName) throws IOException, ClassNotFoundException, SQLException
        {
            LogEntry("Messaging Plugin Uninstallation...");
            LogEntry("");

            if (PluginName.equals(""))
            {
                LogEntry("PluginName: Must be specified. Options are: Live, Yahoo, ICQ, Jabber, SIPE, IRC, Sametime, Groupwise and Facebook.");
                return;
            }

            //Should we uninstall the Live Plugin?
            if (PluginName.equalsIgnoreCase("Live"))
                {
                try {
                    RemoveLivePlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Live Messenger Plugin uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we uninstall the ICQ Plugin?
            if (PluginName.equalsIgnoreCase("ICQ"))
                {
                try {
                    RemoveICQPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("ICQ Plugin uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we uninstall the Facebook Plugin?
            if (PluginName.equalsIgnoreCase("Facebook"))
                {
                try {
                    RemoveFacebookPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Facebook Plugin uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we uninstall the Yahoo Plugin?
            if (PluginName.equalsIgnoreCase("Palm"))
                {
                try {
                    DisablePalmPlugins();
                } catch (ActiveRecordException ex) {
                    LogEntry("Palm Plugin Uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we uninstall the Jabber Plugin?
            if (PluginName.equalsIgnoreCase("Jabber"))
                {
                try {
                    RemoveJabberPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Jabber Plugin uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we uninstall the SIPE Plugin?
            if (PluginName.equalsIgnoreCase("SIPE"))
                {
                try {
                    RemoveSIPEPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("SIPE Plugin uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we uninstall the IRC Plugin?
            if (PluginName.equalsIgnoreCase("IRC"))
                {
                try {
                    RemoveIRCPlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("IRC Plugin uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we uninstall the Sametime Plugin?
            if (PluginName.equalsIgnoreCase("Sametime"))
                {
                try {
                    RemoveSametimePlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Sametime Plugin uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            //Should we uninstall the Groupwise Plugin?
            if (PluginName.equalsIgnoreCase("Groupwise"))
                {
                try {
                    RemoveGroupwisePlugin();
                } catch (ActiveRecordException ex) {
                    LogEntry("Groupwise Plugin uninstallation Error");
                    LogEntry(ex.toString());
                }
                return;
            }

            LogEntry("PluginName: Must be specified. Options are: Live, Yahoo, ICQ, Jabber, SIPE, IRC, Sametime, Groupwise and Facebook.");
            System.exit(1);
        }

    private static void RemoveLivePlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Uninstalling Messenger Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Removal ID
            String RemovalID = GetID("com_palm_accounts_AccountType", "Live Messenger");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE name='Live Messenger'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE com_palm_accounts_AccountType_id='" + RemovalID + "'").toString());
            batch.execute();

            //Done
            LogEntry("Messenger Plugin Uninstalled.");
    }

    private static void RemoveICQPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Uninstalling ICQ Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Removal ID
            String RemovalID = GetID("com_palm_accounts_AccountType", "ICQ");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE name='ICQ'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE com_palm_accounts_AccountType_id='" + RemovalID + "'").toString());
            batch.execute();

            //Done
            LogEntry("ICQ Plugin Uninstalled.");
    }

     private static void RemoveFacebookPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Uninstalling Facebook Chat Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Removal ID
            String RemovalID = GetID("com_palm_accounts_AccountType", "Facebook Chat");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE name='Facebook Chat'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE com_palm_accounts_AccountType_id='" + RemovalID + "'").toString());
            batch.execute();

            //Done
            LogEntry("Facebook Chat Plugin Uninstalled.");
    }

     private static void RemoveSIPEPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Uninstalling SIPE Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Removal ID
            String RemovalID = GetID("com_palm_accounts_AccountType", "Live Communicator");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE name='Live Communicator'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE com_palm_accounts_AccountType_id='" + RemovalID + "'").toString());
            batch.execute();

            //Done
            LogEntry("SIPE Plugin Uninstalled.");
    }

     private static void RemoveIRCPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Uninstalling IRC Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Removal ID
            String RemovalID = GetID("com_palm_accounts_AccountType", "IRC");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE name='IRC'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE com_palm_accounts_AccountType_id='" + RemovalID + "'").toString());
            batch.execute();

            //Done
            LogEntry("IRC Plugin Uninstalled.");
    }
     
     private static void RemoveSametimePlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Uninstalling Sametime Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Removal ID
            String RemovalID = GetID("com_palm_accounts_AccountType", "Sametime");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE name='Sametime'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE com_palm_accounts_AccountType_id='" + RemovalID + "'").toString());
            batch.execute();

            //Done
            LogEntry("Sametime Plugin Uninstalled.");
    }

     private static void RemoveJabberPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Uninstalling Jabber Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Removal ID
            String RemovalID = GetID("com_palm_accounts_AccountType", "Jabber");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE name='Jabber'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE com_palm_accounts_AccountType_id='" + RemovalID + "'").toString());
            batch.execute();

            //Done
            LogEntry("Jabber Plugin Uninstalled.");
    }

private static void RemoveGroupwisePlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Uninstalling Groupwise Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Removal ID
            String RemovalID = GetID("com_palm_accounts_AccountType", "Novell Groupwise");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE name='Novell Groupwise'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("DELETE FROM ").append(tableName).append(" WHERE com_palm_accounts_AccountType_id='" + RemovalID + "'").toString());
            batch.execute();

            //Done
            LogEntry("Groupwise Plugin Uninstalled.");
    }

     private static void DisablePalmPlugins() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Disabling Yahoo Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Update Palm entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("UPDATE ").append(tableName).append(" SET acctTypeEnabled=0 WHERE name='Yahoo IM'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Update palm entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("UPDATE ").append(tableName).append(" SET dbusAddress=com.nokia.imtransport WHERE com_palm_accounts_AccountType_id='1099511627792'").toString());
            batch.execute();

            //Done
            LogEntry("Yahoo Plugin Disabled.");

            LogEntry("Configuring Palm Plugins.");
            
            //Update palm entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("UPDATE ").append(tableName).append(" SET dbusAddress='im.libpurple.palm' WHERE dbusAddress='im.libpurpleext.greg'").toString());
            batch.execute();

            //Done
            LogEntry("Palm Plugins Configured.");
    }

    private static void InstallLivePlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Installing Messenger Plugin...");

            //Get Installation ID
            String InstallationID = GetID("com_palm_accounts_AccountType", "Live Messenger");

            if (!(InstallationID.equalsIgnoreCase("")))
            {
                LogEntry("Live Messenger Plugin already installed.");
                return;
            }

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Last ID
            long LastIDTable1 = GetNext ("com_palm_accounts_AccountType", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (name,accountClass,domain,iconClass,lunaSetupServiceMethod,accountDeleteMethod,icons,isCrudAccount,isUserCreatable,dataIsReadOnly,acctTypeEnabled,hasSharedAuthToken,backupID,appID,id,_class_id,_mod_num,_flags) VALUES ('Live Messenger','com.palm.messaging.accounts.IMAccount','live','live','luna://com.palm.messaging/createAccountAndLogin','','{\"32x32\":\"images/accounts/messenger-32x32.png\",\"48x48\":\"images/accounts/messenger-48x48.png\"}','0','1','0','1','0','0','defaultAppId','" + LastIDTable1 + "','1','9901','')").toString());

            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Get Last ID
            long LastIDTable2 = GetNext ("com_palm_accounts_AccountTypeService", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (com_palm_accounts_AccountType_id,interfaceName,dbusAddress,serviceType,isCrudService,backupID,id,_class_id,_mod_num,_flags) VALUES ('" + LastIDTable1 + "','','im.libpurpleext.greg','IM','0','0','" + LastIDTable2 + "','2','9902','')").toString());
            batch.execute();

            //Done
            LogEntry("Messenger Plugin Installed.");
    }

    private static void InstallFacebookPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Installing Facebook Chat Plugin...");

            //Get Installation ID
            String InstallationID = GetID("com_palm_accounts_AccountType", "Facebook Chat");

            if (!(InstallationID.equalsIgnoreCase("")))
            {
                LogEntry("Facebook Chat Plugin already installed.");
                return;
            }

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Last ID
            long LastIDTable1 = GetNext ("com_palm_accounts_AccountType", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (name,accountClass,domain,iconClass,lunaSetupServiceMethod,accountDeleteMethod,icons,isCrudAccount,isUserCreatable,dataIsReadOnly,acctTypeEnabled,hasSharedAuthToken,backupID,appID,id,_class_id,_mod_num,_flags) VALUES ('Facebook Chat','com.palm.messaging.accounts.IMAccount','facebook','facebook','luna://com.palm.messaging/createAccountAndLogin','','{\"32x32\":\"images/accounts/facebook-32x32.png\",\"48x48\":\"images/accounts/facebook-48x48.png\"}','0','1','0','1','0','0','defaultAppId','" + LastIDTable1 + "','1','9905','')").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Get Last ID
            long LastIDTable2 = GetNext ("com_palm_accounts_AccountTypeService", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (com_palm_accounts_AccountType_id,interfaceName,dbusAddress,serviceType,isCrudService,backupID,id,_class_id,_mod_num,_flags) VALUES ('" + LastIDTable1 + "','','im.libpurpleext.greg','IM','0','0','" + LastIDTable2 + "','2','9906','')").toString());
            batch.execute();

            //Done
            LogEntry("Facebook Chat Plugin Installed.");
    }

    private static void InstallICQPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Installing ICQ Plugin...");

            //Get Installation ID
            String InstallationID = GetID("com_palm_accounts_AccountType", "ICQ");

            if (!(InstallationID.equalsIgnoreCase("")))
            {
                LogEntry("ICQ Plugin already installed.");
                return;
            }

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Last ID
            long LastIDTable1 = GetNext ("com_palm_accounts_AccountType", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (name,accountClass,domain,iconClass,lunaSetupServiceMethod,accountDeleteMethod,icons,isCrudAccount,isUserCreatable,dataIsReadOnly,acctTypeEnabled,hasSharedAuthToken,backupID,appID,id,_class_id,_mod_num,_flags) VALUES ('ICQ','com.palm.messaging.accounts.IMAccount','icq','icq','luna://com.palm.messaging/createAccountAndLogin','','{\"32x32\":\"images/accounts/icq-32x32.png\",\"48x48\":\"images/accounts/icq-48x48.png\"}','0','1','0','1','0','0','defaultAppId','" + LastIDTable1 + "','1','9903','')").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Get Last ID
            long LastIDTable2 = GetNext ("com_palm_accounts_AccountTypeService", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (com_palm_accounts_AccountType_id,interfaceName,dbusAddress,serviceType,isCrudService,backupID,id,_class_id,_mod_num,_flags) VALUES ('" + LastIDTable1 + "','','im.libpurpleext.greg','IM','0','0','" + LastIDTable2 + "','2','9904','')").toString());
            batch.execute();

            //Done
            LogEntry("ICQ Plugin Installed.");
    }

    private static void InstallSIPEPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Installing SIPE Plugin...");

            //Get Installation ID
            String InstallationID = GetID("com_palm_accounts_AccountType", "Live Communicator");

            if (!(InstallationID.equalsIgnoreCase("")))
            {
                LogEntry("SIPE Plugin already installed.");
                return;
            }

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Last ID
            long LastIDTable1 = GetNext ("com_palm_accounts_AccountType", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (name,accountClass,domain,iconClass,lunaSetupServiceMethod,accountDeleteMethod,icons,isCrudAccount,isUserCreatable,dataIsReadOnly,acctTypeEnabled,hasSharedAuthToken,backupID,appID,id,_class_id,_mod_num,_flags) VALUES ('Live Communicator','com.palm.messaging.accounts.IMAccount','sipe','sipe','luna://com.palm.messaging/createAccountAndLogin','','{\"32x32\":\"images/accounts/sipe-32x32.png\",\"48x48\":\"images/accounts/sipe-48x48.png\"}','0','1','0','1','0','0','defaultAppId','" + LastIDTable1 + "','1','9909','')").toString());

            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Get Last ID
            long LastIDTable2 = GetNext ("com_palm_accounts_AccountTypeService", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (com_palm_accounts_AccountType_id,interfaceName,dbusAddress,serviceType,isCrudService,backupID,id,_class_id,_mod_num,_flags) VALUES ('" + LastIDTable1 + "','','im.libpurpleext.greg','IM','0','0','" + LastIDTable2 + "','2','9910','')").toString());
            batch.execute();

            //Done
            LogEntry("SIPE Plugin Installed.");
    }

    private static void InstallIRCPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Installing IRC Plugin...");

            //Get Installation ID
            String InstallationID = GetID("com_palm_accounts_AccountType", "IRC");

            if (!(InstallationID.equalsIgnoreCase("")))
            {
                LogEntry("IRC Plugin already installed.");
                return;
            }

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Last ID
            long LastIDTable1 = GetNext ("com_palm_accounts_AccountType", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (name,accountClass,domain,iconClass,lunaSetupServiceMethod,accountDeleteMethod,icons,isCrudAccount,isUserCreatable,dataIsReadOnly,acctTypeEnabled,hasSharedAuthToken,backupID,appID,id,_class_id,_mod_num,_flags) VALUES ('IRC','com.palm.messaging.accounts.IMAccount','irc','irc','luna://com.palm.messaging/createAccountAndLogin','','{\"32x32\":\"images/accounts/irc-32x32.png\",\"48x48\":\"images/accounts/irc-48x48.png\"}','0','1','0','1','0','0','defaultAppId','" + LastIDTable1 + "','1','9911','')").toString());

            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Get Last ID
            long LastIDTable2 = GetNext ("com_palm_accounts_AccountTypeService", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (com_palm_accounts_AccountType_id,interfaceName,dbusAddress,serviceType,isCrudService,backupID,id,_class_id,_mod_num,_flags) VALUES ('" + LastIDTable1 + "','','im.libpurpleext.greg','IM','0','0','" + LastIDTable2 + "','2','9912','')").toString());
            batch.execute();

            //Done
            LogEntry("IRC Plugin Installed.");
    }

    private static void InstallSametimePlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Installing Sametime Plugin...");

            //Get Installation ID
            String InstallationID = GetID("com_palm_accounts_AccountType", "Sametime");

            if (!(InstallationID.equalsIgnoreCase("")))
            {
                LogEntry("Sametime Plugin already installed.");
                return;
            }

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Last ID
            long LastIDTable1 = GetNext ("com_palm_accounts_AccountType", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (name,accountClass,domain,iconClass,lunaSetupServiceMethod,accountDeleteMethod,icons,isCrudAccount,isUserCreatable,dataIsReadOnly,acctTypeEnabled,hasSharedAuthToken,backupID,appID,id,_class_id,_mod_num,_flags) VALUES ('Sametime','com.palm.messaging.accounts.IMAccount','sametime','sametime','luna://com.palm.messaging/createAccountAndLogin','','{\"32x32\":\"images/accounts/sametime-32x32.png\",\"48x48\":\"images/accounts/sametime-48x48.png\"}','0','1','0','1','0','0','defaultAppId','" + LastIDTable1 + "','1','9913','')").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Get Last ID
            long LastIDTable2 = GetNext ("com_palm_accounts_AccountTypeService", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (com_palm_accounts_AccountType_id,interfaceName,dbusAddress,serviceType,isCrudService,backupID,id,_class_id,_mod_num,_flags) VALUES ('" + LastIDTable1 + "','','im.libpurpleext.greg','IM','0','0','" + LastIDTable2 + "','2','9914','')").toString());
            batch.execute();

            //Done
            LogEntry("Sametime Plugin Installed.");
    }

    private static void InstallJabberPlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Installing Jabber Plugin...");

            //Get Installation ID
            String InstallationID = GetID("com_palm_accounts_AccountType", "Jabber");

            if (!(InstallationID.equalsIgnoreCase("")))
            {
                LogEntry("Jabber Plugin already installed.");
                return;
            }

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Last ID
            long LastIDTable1 = GetNext ("com_palm_accounts_AccountType", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (name,accountClass,domain,iconClass,lunaSetupServiceMethod,accountDeleteMethod,icons,isCrudAccount,isUserCreatable,dataIsReadOnly,acctTypeEnabled,hasSharedAuthToken,backupID,appID,id,_class_id,_mod_num,_flags) VALUES ('Jabber','com.palm.messaging.accounts.IMAccount','jabber','jabber','luna://com.palm.messaging/createAccountAndLogin','','{\"32x32\":\"images/accounts/jabber-32x32.png\",\"48x48\":\"images/accounts/jabber-48x48.png\"}','0','1','0','1','0','0','defaultAppId','" + LastIDTable1 + "','1','9907','')").toString());

            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Get Last ID
            long LastIDTable2 = GetNext ("com_palm_accounts_AccountTypeService", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (com_palm_accounts_AccountType_id,interfaceName,dbusAddress,serviceType,isCrudService,backupID,id,_class_id,_mod_num,_flags) VALUES ('" + LastIDTable1 + "','','im.libpurpleext.greg','IM','0','0','" + LastIDTable2 + "','2','9908','')").toString());
            batch.execute();

            //Done
            LogEntry("Jabber Plugin Installed.");
    }

    private static void InstallGroupwisePlugin() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Installing Groupwise Plugin...");

            //Get Installation ID
            String InstallationID = GetID("com_palm_accounts_AccountType", "Novell Groupwise");

            if (!(InstallationID.equalsIgnoreCase("")))
            {
                LogEntry("Groupwise Plugin already installed.");
                return;
            }

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Get Last ID
            long LastIDTable1 = GetNext ("com_palm_accounts_AccountType", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (name,accountClass,domain,iconClass,lunaSetupServiceMethod,accountDeleteMethod,icons,isCrudAccount,isUserCreatable,dataIsReadOnly,acctTypeEnabled,hasSharedAuthToken,backupID,appID,id,_class_id,_mod_num,_flags) VALUES ('Novell Groupwise','com.palm.messaging.accounts.IMAccount','gwim','gwim','luna://com.palm.messaging/createAccountAndLogin','','{\"32x32\":\"images/accounts/novell-32x32.png\",\"48x48\":\"images/accounts/novell-48x48.png\"}','0','1','0','1','0','0','defaultAppId','" + LastIDTable1 + "','1','9915','')").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Get Last ID
            long LastIDTable2 = GetNext ("com_palm_accounts_AccountTypeService", "id");

            //Add new entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("INSERT INTO ").append(tableName).append(" (com_palm_accounts_AccountType_id,interfaceName,dbusAddress,serviceType,isCrudService,backupID,id,_class_id,_mod_num,_flags) VALUES ('" + LastIDTable1 + "','','im.libpurpleext.greg','IM','0','0','" + LastIDTable2 + "','2','9916','')").toString());
            batch.execute();

            //Done
            LogEntry("Groupwise Plugin Installed.");
    }

    private static void EnablePalmPlugins() throws ActiveRecordException, IOException, ClassNotFoundException, SQLException
    {
            LogEntry("Enabling Yahoo Plugin...");

            //Update com_palm_account_AccountType
            String tableName = ActiveRecord.tableName(com.palm.accounts.AccountType.class);

            //Update Palm entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("UPDATE ").append(tableName).append(" SET acctTypeEnabled=1 WHERE name='Yahoo IM'").toString());
            batch.execute();

            //Update com_palm_account_AccountTypeService
            tableName = ActiveRecord.tableName(com.palm.accounts.AccountTypeService.class);

            //Update palm entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("UPDATE ").append(tableName).append(" SET dbusAddress='im.libpurpleext.greg' WHERE dbusAddress='com.nokia.imtransport'").toString());
            batch.execute();

            //Done
            LogEntry("Yahoo Plugin Enabled.");

            LogEntry("Configuring Palm Plugins.");

            //Update palm entry
            batch = Batch.create();
            batch.setDatabase(PalmDB);
            batch.sqlWrite((new StringBuilder()).append("UPDATE ").append(tableName).append(" SET dbusAddress='im.libpurpleext.greg' WHERE dbusAddress='im.libpurple.palm'").toString());
            batch.execute();

            //Done
            LogEntry("Palm Plugins Configured.");
    }

    private static void LogEntry(String Value) throws IOException
     {
            System.out.println(Value);

            String tmpFilePath = System.getProperty("java.io.tmpdir");
            BufferedWriter LogFile = new BufferedWriter(new FileWriter(tmpFilePath + "/MessagingPlugins.log", true));

            try {
              //Write to log file
              LogFile.newLine();
              LogFile.write (Value);
            }
            finally {
              LogFile.close();
            }
     }

    private static long GetNext(String TableName, String ColumnName) throws ClassNotFoundException, SQLException, IOException
    {
        LogEntry ("Querying " + TableName + " for last " + ColumnName + "...");
        long NextValue = 0;
        String ColumnValue = null;

        Class.forName("org.sqlite.JDBC");
        Connection conn = DriverManager.getConnection("jdbc:sqlite:/var/luna/data/dbdata/PalmDatabase.db3");
        java.sql.Statement stat = conn.createStatement();

        ResultSet rs = stat.executeQuery("select * from " + TableName + ";");
        while (rs.next()) {
            ColumnValue = rs.getString(ColumnName);
        }
        rs.close();
        conn.close();

        LogEntry ("Found: " + ColumnValue);

        NextValue = Long.parseLong(ColumnValue.trim());
        NextValue += 1;

        LogEntry ("Next " + ColumnName + " is: " + NextValue);
        return NextValue;
    }

    private static String GetID(String TableName, String PluginName) throws ClassNotFoundException, SQLException, IOException
    {
        LogEntry ("Querying " + TableName + " for id...");
        String ColumnValue = "";

        Class.forName("org.sqlite.JDBC");
        Connection conn = DriverManager.getConnection("jdbc:sqlite:/var/luna/data/dbdata/PalmDatabase.db3");
        java.sql.Statement stat = conn.createStatement();

        ResultSet rs = stat.executeQuery("select * from " + TableName + " where name='" + PluginName + "';");
        while (rs.next()) {
            ColumnValue = rs.getString("id");
        }
        rs.close();
        conn.close();

        LogEntry ("Found: " + ColumnValue);

        return ColumnValue;
    }
}