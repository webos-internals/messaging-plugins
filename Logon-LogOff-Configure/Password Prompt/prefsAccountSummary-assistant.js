/* Copyright 2009 Palm, Inc.  All rights reserved. */

var PrefsAccountSummaryAssistant = Class.create({
   initialize: function(params){
        this.appController = Mojo.Controller.getAppController();
        this.Messaging = this.appController.Messaging;      
        this.params = params || {}
        this.requests = [];
		
		this.handleAccountListTap = this.handleAccountListTap.bindAsEventListener(this);
		this.handleAddAccountTap = this.handleAddAccountTap.bindAsEventListener(this);
		this.notificationToggleChanged = this.notificationToggleChanged.bindAsEventListener(this);
		this.notificationSoundToggleChanged = this.notificationSoundToggleChanged.bindAsEventListener(this);
		this.chooseNotificationRingtone = this.chooseNotificationRingtone.bindAsEventListener(this);
		this.autoDownloadToggleChanged = this.autoDownloadToggleChanged.bindAsEventListener(this);

		this.availabilities = [
			{label: $L('Sign On'), command:'Login', secondaryIcon:'status-available'},
			{label: $L('Sign Off'),  command:'Logoff',   secondaryIcon:'status-offline'},
			{label: $L('Configure Account'),  command:'ConfigAccount'}
		];
    },
 
    setup: function(){
      this.preferencesAccountsModel = {items: []};
	  this.controller.setupWidget('preferences-accounts', {itemTemplate:'prefsAccountSummary/accountsRow'}, this.preferencesAccountsModel);

      this.preferencesSetup();
  		
      // make the service call that will populate this list
      this.requests.push(MessagingMojoService.getIMAccounts({
	  	sceneController: this.controller,
	  	onSuccess: this.renderAccountsList.bind(this)
	  }));      
    
      MessagingMojoService.getOwnPhoneNumber(this.controller, (function(data) {
        this.controller.get('phoneNumber').update(MessagingUtils.formatPhoneNumber(MessagingUtils.handleGetOwnPhoneNumber(data.phoneNumber)));
      }).bind(this));  
   
      this.controller.listen('preferences-accounts',Mojo.Event.listTap, this.handleAccountListTap);      
      this.controller.listen('addAccountButton',Mojo.Event.tap, this.handleAddAccountTap);   
   
      this.controller.listen('currentringtonerow',Mojo.Event.tap, this.chooseNotificationRingtone);
	  
	  //Password Dialog
	  this.controller.setupWidget('password', this.passwordAttributes, "");
    },
	
	//Password Dialog
	passwordAttributes: {
		textReplacement: false,
		maxLength: 64,
		focus: true,
		acceptBack: true,
		hintText: 'Enter Password...',
		changeOnKeyPress: true,
	},
	
	//Show password dialog
	ShowPasswordDialog: function(e) {
		this.controller.showDialog({
			template: 'prefsAccountSummary/passwordDialog',
			assistant: new PasswordDialogAssistant(this)
		});
	},
	
	cleanup: function() {
		this.controller.stopListening('preferences-accounts',Mojo.Event.listTap, this.handleAccountListTap);
		this.controller.stopListening('addAccountButton',Mojo.Event.tap, this.handleAddAccountTap);
		if (this.saveSMSPrefs) {
			this.controller.stopListening('savePrefs', Mojo.Event.tap, this.saveSMSPrefs);
		}
      this.controller.stopListening('notificationToggle',Mojo.Event.propertyChange,this.notificationToggleChanged);
      this.controller.stopListening('notificationSoundSelector',Mojo.Event.propertyChange,this.notificationSoundToggleChanged);		
      this.controller.stopListening('autoDownloadToggle',Mojo.Event.propertyChange,this.autoDownloadToggleChanged);
	},

	handleAccountListTap: function(e) {
		this.controller.popupSubmenu({
			onChoose: this.handleAvailabilitySelection.bind(this,e),
			placeNear: e.target,
			items: this.availabilities
		});
	},

	handleAvailabilitySelection: function(accountitem, selectedValue) {
		if (selectedValue == undefined) 
			return;

		//Configure Account?
		if (selectedValue == "ConfigAccount")
		{
			MessagingUtils.simpleListClick(this.controller.get(accountitem.originalEvent.target),"accountRow",this.editAccount.bind(this),false);   
			return;
		}
		
		//Get Target Row
		var targetRow = this.controller.get(accountitem.originalEvent.target);
		if (!targetRow.hasClassName('accountRow')) {
		  targetRow = targetRow.up('.accountRow');
		}
		
		//Setup variables
		var serviceName;
		var username;
		var id;
		
		targetRow.select('input').each(function(input){
			if (input.name == 'domain')
			{
				serviceName = input.value;
			}
			if (input.name == 'username')
			{
				username = input.value;
			}
			if (input.name == 'id')
			{
				id = input.value;
			}
		  });
		
		//Login?
		if (selectedValue == "Login")
		{
			//Show password dialog
			this.ShowPasswordDialog(this);
		}
		
		//Logoff?
		if (selectedValue == "Logoff")
		{
			this.controller.serviceRequest('palm://com.palm.messaging', {
				method: 'setMyAvailability',
				parameters: {serviceName:serviceName, username: username, availability: '4'}
			});
			return;
		}
	},
	
	handleAddAccountTap: function() {
		this.controller.stageController.pushScene('prefsAccountTypeList', this.params);
	},
    
    autoSignonChange : function(prop){
    },
    
    /**************************************************
     * This method renders and handles everything to do
     * with the preferences on the account summary view
     **************************************************/
    
    preferencesSetup: function() {
      this.prefsAutoSignIn();
      //this.prefsSmsMmsSetup();
      this.prefsNotification();
    },
    
    prefsAutoSignIn: function() {
      /*
      var autoSignin = {
        modelProperty : 'auto-signin-toggle',
        choices : [
          {name : 'Y', value : 1 },
          {name : 'N', value : 0 }
        ]
      }
      this.controller.setupWidget('auto-signin-toggle',  autoSignin, {'auto-signin-toggle' : 1 })
//        this.controller.setupWidget('AutoSignon', {modelProperty : 'AutoSignon', 
//          options : [{name : 'Y', value: 1}, {name: 'N', value: 0}]},
//          {AutoSignon : 1});
//        this.controller.get('AutoSignon').observe('mojo-property-change', 
//			this.autoSignonChange.bind(this));            
       */       
    },
    
    prefsSmsMmsSetup: function() {
      /************************
      * Setup widgets
      ************************/
      var smscAttributes = {
        textFieldName: "smscAddressText",
        hintText: '',
        modelProperty: 'original',
        multiline: false,
        focus: false
      };
      this.smscModel = {
        original: ''
      };
      this.controller.setupWidget('smscAddress', smscAttributes, this.smscModel);  
      
      var emailGatewayAttributes = {
        textFieldName: "emailGatewayText",
        hintText: '',
        modelProperty: 'original',
        multiline: false,
        focus: false
      };
      this.emailGatewayModel = {
        original: ''
      };
      this.controller.setupWidget('emailGateway', emailGatewayAttributes, this.emailGatewayModel);              
      
      var mmscAttributes = {
        textFieldName: "mmscAddressText",
        hintText: '',
        modelProperty: 'original',
        multiline: false,
        focus: false
      };
      this.mmscModel = {
        original: ''
      };
      this.controller.setupWidget('mmscAddress', mmscAttributes, this.mmscModel);  
      
      var mmscProxyAttributes = {
        textFieldName: "mmscProxyText",
        hintText: '',
        modelProperty: 'original',
        multiline: false,
        focus: false
      };
      this.mmscProxyModel = {
        original: ''
      };
      this.controller.setupWidget('mmscProxy', mmscProxyAttributes, this.mmscProxyModel);          

      var mmsSmsUseSettingsAttributes = {
        modelProperty: "value"
      };
      this.mmsSmsUseSettingsModel = {
        value: false
      };
      this.controller.setupWidget('useSettings', mmsSmsUseSettingsAttributes, this.mmsSmsUseSettingsModel);        
        
      /*********************************************
      * Methods for rendering existing pref values
      *********************************************/        
      this.renderEditSMSCAddress = function(response) {
        this.smscModel.original = response.smscAddr;
        this.controller.modelChanged(this.smscModel,this);
        this.emailGatewayModel.original = response.emailGateway;
        this.controller.modelChanged(this.emailGatewayModel,this);        
      };            
          
	  this.renderEditMMSSettings = function(response){
	  	this.mmscModel.original = response.mmsc;
	  	this.controller.modelChanged(this.mmscModel, this);
	  	this.mmscProxyModel.original = response.proxy;
	  	this.controller.modelChanged(this.mmscProxyModel, this);
	  	
	  if (response.useMmscProxyPrefs == true) {
	  		this.mmsSmsUseSettingsModel.value = true;
	  		this.controller.modelChanged(this.mmsSmsUseSettingsModel, this);
	  	} else {
	  		this.mmsSmsUseSettingsModel.value = false;
	  		this.controller.modelChanged(this.mmsSmsUseSettingsModel, this);
	  	}
	  };       
      
      // Saving the pref data in a batch like this because that is how the service
      // is currently set up.  Since these SMS/MMS prefs are for GCF this is fine.      
      this.saveSMSPrefs = function(e) {
        var smscAddress = this.controller.get('smscAddress').querySelector('[name=smscAddressText]').value;
        var emailGateway = this.controller.get('emailGateway').querySelector('[name=emailGatewayText]').value;
        var mmscAddress = this.controller.get('mmscAddress').querySelector('[name=mmscAddressText]').value;
        var mmscProxy = this.controller.get('mmscProxy').querySelector('[name=mmscProxyText]').value;
        var useMmsSettings = this.mmsSmsUseSettingsModel.value
    		this.requests.push(MessagingMojoService.setSMSCAddressAndEmailGateway(smscAddress, emailGateway,this.controller));
    		this.requests.push(MessagingMojoService.setMMSSettings(mmscAddress, mmscProxy, useMmsSettings, this.controller));
    		this.controller.stageController.popScene();
    	}.bind(this);        
      
      this.controller.listen('savePrefs',Mojo.Event.tap,this.saveSMSPrefs);        
      
      // Retrieve SMS & MMS preferences to populate pref text fields
      this.requests.push(MessagingMojoService.getSMSCAddressAndEmailGateway(this.renderEditSMSCAddress.bind(this), this.controller));
      this.requests.push(MessagingMojoService.getMMSSettings(this.renderEditMMSSettings.bind(this), this.controller)); 
    },
    
    prefsNotification: function() {
      // The notification prefs were loaded on headless app boot
      var messagingPrefs = this.Messaging.messagingPrefs;
      if(messagingPrefs.enableNotification == undefined)
        messagingPrefs.enableNotification = false;      
      if(messagingPrefs.enableNotificationSound == undefined)
        messagingPrefs.enableNotificationSound = 0;
      
      if(!messagingPrefs.enableNotification)
        this.controller.get('notificationSoundContainer').hide();
      
      var notificationAttributes = {
        modelProperty: "value"
      };
      this.notificationModel = {
        value: messagingPrefs.enableNotification
      };
      this.controller.setupWidget('notificationToggle', notificationAttributes, this.notificationModel);   
      
      var autoDownloadAttributes = {
        modelProperty: "value"
      };
      this.autoDownloadModel = {
        value: messagingPrefs.useImmediateMmsRetrieval
      };
	  if (messagingPrefs.supportDelayedRetrievalOption === false){
	  	this.controller.get('autoDownloadToggleRow').hide();
		this.controller.get('currentringtonerow').addClassName("last");
	  }
      this.controller.setupWidget('autoDownloadToggle', autoDownloadAttributes, this.autoDownloadModel);   
      
      this.notificationSoundModel = {
        value: messagingPrefs.enableNotificationSound
      };
	           
      soundSelections = {
      	modelProperty: 'value',
      	label: $L("Sound"),
      	choices: [
      		{label: $L('System Alert'), value: "1"},
      		{label: $L('Ringtone'), value: "2"},
      		{label: $L('Mute'), value: "0"}
      	]
      };
	  this.controller.setupWidget('notificationSoundSelector', soundSelections, this.notificationSoundModel);
      
	  this.controller.get('currentringtone').update(messagingPrefs.notificationRingtoneName);
	  
      this.controller.listen('notificationToggle',Mojo.Event.propertyChange,this.notificationToggleChanged);
      this.controller.listen('notificationSoundSelector',Mojo.Event.propertyChange,this.notificationSoundToggleChanged);
	  if (messagingPrefs.enableNotificationSound != 2) {
	  	this.controller.get('currentringtonerow').hide();
		if (messagingPrefs.supportDelayedRetrievalOption === false){
			this.controller.get('soundselectrow').addClassName("last");
		}
	  }
      this.controller.listen('autoDownloadToggle',Mojo.Event.propertyChange,this.autoDownloadToggleChanged);
    },
	
	notificationToggleChanged: function(event) {
 		// if disabling notifications, hide the sound toggle       
        if(event.value == false)
          this.controller.get('notificationSoundContainer').hide();
        else
          this.controller.get('notificationSoundContainer').show();
          
        // save the pref
		var params = {
			isEnabledNotification: event.value
		};		
        MessagingMojoService.setNotificationPreferences(this.controller,params);		
	},
	
	autoDownloadToggleChanged: function(event) {
        MessagingMojoService.setNotificationPreferences(this.controller, { isImmediateMmsRetrieval: event.value });		
	},
	
	notificationSoundToggleChanged: function(event) {
        // save the pref 
		Mojo.Log.info("notificationSoundToggleChanged %j", event);
		if (event.value == "2") {
		  	this.controller.get('currentringtonerow').show();
			if (this.Messaging.messagingPrefs.supportDelayedRetrievalOption === false) {
				this.controller.get('soundselectrow').removeClassName("last")
			}
			// If no ringtone is set, then display the picker
			var ringtoneName = this.Messaging.messagingPrefs.notificationRingtoneName;
			Mojo.Log.info("ringtoneName",ringtoneName);
			if (ringtoneName == null || ringtoneName == "") {
				this.chooseNotificationRingtone();
			}
		} else {
		  	this.controller.get('currentringtonerow').hide();
			if (this.Messaging.messagingPrefs.supportDelayedRetrievalOption === false) {
				this.controller.get('soundselectrow').addClassName("last");
			}
		}

		var params = {
			isEnabledNotificationSound: event.value
		};		
        MessagingMojoService.setNotificationPreferences(this.controller,params);    		
	},
    
	chooseNotificationRingtone: function() {
    	var params = {
			actionType: "attach",
            defaultKind: 'ringtone',
			kinds: ["ringtone"],
			filePath: this.Messaging.messagingPrefs.notificationRingtonePath,
			actionName: $L("Done"),
	        onSelect: this.handleRingtoneSelect.bind(this)
	    };
	    Mojo.FilePicker.pickFile(params,this.controller.stageController);
		//this.handleRingtoneSelect.bind(this, {"fullPath": "/media/internal/ringtones/Discreet.mp3", "name": "Discreet"}).delay(1);
	},

	handleRingtoneSelect: function(file) {
		Mojo.Log.info("handleRingtoneSelect %j", file);
		var params = {
			ringtonePath: file.fullPath,
			ringtoneName: file.name
		};
		MessagingMojoService.setNotificationPreferences(this.controller, params);
		this.controller.get('currentringtone').update(file.name);
	},
	
    renderAccountsList: function(data){   
      var that = this;  
      data.list.each(function(item){
	  	var transport = that.Messaging.transports.getTransportByName(item.domain);
        item.serviceIcon = transport.largeIcon;    
		item.accountDisplayName = transport.displayName;
      });      
      
      this.preferencesAccountsModel.items = data.list;
      this.controller.modelChanged(this.preferencesAccountsModel, this);     
    },
    editAccount: function(targetRow){
      // iterate through hidden inputs
      var inputHash = {};
      targetRow.select('input').each(function(input){
        inputHash[input.name] = input.value;
      });
	  var newParams = {editMode: true};
	  Object.extend(newParams,this.params);
      Object.extend(newParams,inputHash);
      this.controller.stageController.pushScene('prefsSetupAccount',newParams);
    }
});

var PasswordDialogAssistant = Class.create({
	
	initialize: function(sceneAssistant) {
		this.sceneAssistant = sceneAssistant;
		this.controller = sceneAssistant.controller;
	},
	
	setup : function(widget) {
		this.widget = widget;
		this.controller.get('login_button').addEventListener(Mojo.Event.tap, this.loginclick.bindAsEventListener(this));
		this.controller.get('cancel_button').addEventListener(Mojo.Event.tap, this.cancelclick.bindAsEventListener(this));
	},
	
	loginclick: function() {
		//Login
		this.controller.serviceRequest('palm://com.palm.messaging', {
			method: 'loginToAccount',
			parameters: {serviceName:'live', username: 'commodoreking@hotmail.com', password: 'hsvvtgts' ,connectionType:'wifi' ,ipAddress:'10.0.2.15' ,accountId:'20890720927749'}
		});
	
		this.widget.mojo.close();
	},
	
	cancelclick: function() {
		this.widget.mojo.close();
	}
});