/* Copyright 2009 Palm, Inc.  All rights reserved. */

var PrefsSetupAccountAssistant = Class.create({
   initialize: function(params){
        this.appController = Mojo.Controller.getAppController();
        this.Messaging = this.appController.Messaging;      
        this.params = params || {}
        this.requests = [];
		this.saveAccountFn;
		
		this.loginButtonDefault = $L('Sign in');
		this.loginButtonPending = $L('Signing in...');
		this.deleteButtonDefault = $L("Remove account");
		this.deleteButtonPending = $L("Removing account...");
		
		this.handlePasswordKeyUp = this.handlePasswordKeyUp.bindAsEventListener(this);
		this.removeAccount = this.removeAccount.bindAsEventListener(this);
		this.confirmRemoveAccount = this.confirmRemoveAccount.bindAsEventListener(this);
    },

    setup: function(){
        this.accountId;
        this.createAccountRequest;
		/*
		if (this.params.domain) {
			this.controller.get('preferencesHeader').addClassName(this.Messaging.transports.getTransportByName(this.params.domain).className);
		}
		*/
		if (this.params.serviceIcon) {
			this.controller.get('accountTypeIcon').src = this.params.serviceIcon;
		}
		
		if (this.params.editMode) {
			this.controller.get('prefHeaderText').update($L('Change login settings'));
			this.saveAccountFn = this.updateAccount.bind(this);
			
			this.removeAccountButton = this.controller.get('removeAccountButton');
			this.removeAccountButton.show();
			var deleteButtonAttributes = {
				disabledProperty: 'disabled',
				type: Mojo.Widget.activityButton
			};
			this.deleteButtonModel = {
				buttonLabel: this.deleteButtonDefault,
				buttonClass: 'negative',
				disabled: false
			};
			this.controller.setupWidget('removeAccountButton', deleteButtonAttributes, this.deleteButtonModel);
			this.controller.listen('removeAccountButton', Mojo.Event.tap, this.confirmRemoveAccount);
			
			// Special case for AIM - add extra content at the bottom of the edit screen
			if (this.Messaging.transports.getTransportByName(this.params.domain).className == "aol") {
				var extraContent = Mojo.View.render({object:{}, template:"prefsSetupAccount/extraContent-aol"});				
				this.controller.get('extraContent').update(extraContent);
				this.handleExtraContentTap = function(event) {
					var targetRow = MessagingUtils.getClassUpChain(this.controller.get(event.target),"linkRow");
					if(targetRow) {
						var url = targetRow.getAttribute('x-messaging-link-url');
						if(url) {
							MessagingMojoService.launchBrowser(url);	
						}
					}
				}.bind(this);
				this.imLinksElement = this.controller.get('im-links');
				this.imLinksElement.observe(Mojo.Event.tap, this.handleExtraContentTap);
			}
		} else {
			this.controller.get('prefHeaderText').update($L('Add an account'));
			this.saveAccountFn = this.saveAccount.bind(this);
		}
		
        var usernameAttributes = {
            textFieldName: "username",
            hintText: '',
            modelProperty: 'original',
            multiline: false,
            focus: false,
			textReplacement: false
        };
        this.usernameModel = {
            original: this.params.username || '',
			disabled: this.params.editMode
        };
        this.controller.setupWidget('username', usernameAttributes, this.usernameModel);
      		
        var passwordAttributes = {
            textFieldName: "password",
            hintText: '',
            modelProperty: 'original',
            multiline: false,
            focus: false,
			focusMode: Mojo.Widget.focusSelectMode,
			textReplacement: false,
			requiresEnterKey: true
        };
        this.passwordModel = {
            original: '',
			disabled: false
        };
        // In edit mode, display a dummy password so the user isn't confused into thinking
        // the password is missing. The dummy password gets cleared when the user types
        // anything or taps on the password field.
		if (this.params.editMode === true) {
			this.passwordModel.original = PrefsSetupAccountAssistant.kDummyPassword;
			this.passwordTapHandler = this.clearDummyPassword.bindAsEventListener(this);
		}
        this.controller.setupWidget('password', passwordAttributes, this.passwordModel);		

		this.saveButton = this.controller.get('saveAccountButton');
		var saveButtonAttributes = {
			disabledProperty: 'disabled',
			type: Mojo.Widget.activityButton
		};
		this.saveButtonModel = {
			buttonLabel: this.loginButtonDefault,
			buttonClass: '',
			disabled: false
		};
		this.controller.setupWidget('saveAccountButton', saveButtonAttributes, this.saveButtonModel);
		this.controller.listen('saveAccountButton', Mojo.Event.tap, this.saveAccountFn);
    },
	
	ready: function() {
		this.passwordElement  = this.controller.get('password').querySelector('[name=password]');
		this.passwordElement.observe('keyup', this.handlePasswordKeyUp);
		// Only observe taps if the handler was setup, otherwise don't care about taps.
		if (this.passwordTapHandler !== undefined) {
			this.passwordElement.observe(Mojo.Event.tap, this.passwordTapHandler);
		}
	},
	
	handlePasswordKeyUp: function(event) {
		// If enter is pressed then simulate tapping on "Sign In"
		if (event && Mojo.Char.isEnterKey(event.keyCode)) {
			// If the submit button is enabled then create the account
			if (this.saveButtonModel.disabled == false) {
				this.passwordElement.blur();
				this.saveAccountFn();
				Event.stop(event);
			}
		} else {
			if (this.passwordTapHandler !== undefined) {
				this.passwordElement.stopObserving(Mojo.Event.tap, this.passwordTapHandler);
				this.passwordTapHandler = undefined;
			}
		}
	},
	
	cleanup: function() {
		if (this.passwordTapHandler !== undefined) {
			this.passwordElement.stopObserving(Mojo.Event.tap, this.passwordTapHandler);
		}

		this.passwordElement.stopObserving('keyup', this.handlePasswordKeyUp);
		if (this.saveAccountFn) {
			this.controller.stopListening('saveAccountButton', Mojo.Event.tap, this.saveAccountFn);
		}
		this.controller.stopListening('removeAccountButton', Mojo.Event.tap, this.removeAccount);
		if (this.imLinksElement && this.handleExtraContentTap) {
			this.imLinksElement.stopObserving(Mojo.Event.tap, this.handleExtraContentTap);
		}
	},
	
	clearDummyPassword: function(event) {
		this.passwordElement.stopObserving(Mojo.Event.tap, this.passwordTapHandler);
		this.passwordTapHandler = undefined;
		this.passwordModel.original = "";
		this.controller.modelChanged(this.passwordModel);
	},
	
    saveAccount : function(){
	  var accountDetails = Mojo.View.serializeMojo(this.controller.get('AccountSetupForm'), true);
	  var statusMessageElement = this.controller.get('StatusMessage');
	  var statusMessageWrapper = this.controller.get('StatusMessageWrapper');
	  
	  if (accountDetails.username) {
	  	accountDetails.username = accountDetails.username.strip();
	  }
	  if (accountDetails.password) {
	  	accountDetails.password = accountDetails.password.strip();
	  }	  
	  var hasEmptyField = false;
	  if(!accountDetails.username) {
	  	statusMessageElement.update($L('Please enter a username'));
		statusMessageWrapper.show();
		hasEmptyField = true;
	  } else if(!accountDetails.password) {
	  	statusMessageElement.update($L('Please enter a password'));
		statusMessageWrapper.show();
		hasEmptyField = true;
	  } else {
	  	statusMessageElement.update('');
		statusMessageWrapper.hide();
	  }
	  
	  if(hasEmptyField) {
	  	this.stopLoginSpinner();
		return;
	  }
	  
      accountDetails.accountTypeId = this.params.accountTypeId;
      accountDetails.domain = this.params.domain;

	  this.startLoginSpinner();
      this.createAccountRequest = MessagingMojoService.createIMAccount(this.controller, accountDetails,this.accountSaved.bind(this),this.createLoginFailed.bind(this));
    },
	
	accountSaved : function(result){
		if (result.status) {
			if (result.status == "pending" && result.accountId) {
				this.accountId = result.accountId; // store the accountId for use in considerForNotification
			} else if (result.status == "successful") {
				if (this.createAccountRequest)
					this.controller.cancelServiceRequest(this.createAccountRequest);
				
				// Set pref to show the buddylist -- we just added an account, so
				// it makes sense to jump over to the buddy list
				this.Messaging.setListViewVisible(this.Messaging.Views.BUDDY);
				
				// set the last availability selection to online
				MessagingMojoService.setLastAvailabilitySelection(this.controller, this.Messaging.Availability.AVAILABLE);
				
				this.popScene();
			}
		}
	},
	
    createLoginFailed : function(result){
		if (this.createAccountRequest) {
			this.controller.cancelServiceRequest(this.createAccountRequest);
			this.createAccountRequest = undefined;
		}
		
		if (result.errorCode && (result.errorCode == "AcctMgr_Acct_Already_Exists" || result.errorCode == "AcctMgr_No_Setup_Service")) {
			if (this.params.popScenesTo && this.params.popScenesTo.length > 0) {
				this.controller.stageController.popScenesTo(this.params.popScenesTo);
			} else {
				Mojo.Log.warn("Error: popScenesTo param is not set");
				this.controller.stageController.popScene();
			}
		} else {
			this.stopLoginSpinner();
			var errorText = this.Messaging.Error.getLoginErrorFromCode(result.errorCode);
			this.controller.get('StatusMessage').update(errorText);
			this.controller.get('StatusMessageWrapper').show();
		}
    },
    
	/**
	 * UPDATE ACCOUNT METHODS
	 */
    updateAccount : function(){
	  if (this.passwordModel.original === PrefsSetupAccountAssistant.kDummyPassword) {
	  	Mojo.Log.info("updateAccount doing noop b/c password hasn't changed");
		this.popScene();
	  	return;
	  }
	  var accountDetails = Mojo.View.serializeMojo(this.controller.get('AccountSetupForm'), true);
	  var statusMessageElement = this.controller.get('StatusMessage');
	  var statusMessageWrapper = this.controller.get('StatusMessageWrapper');
	
	  if (accountDetails.password) {
	  	accountDetails.password = accountDetails.password.strip();
	  }	
	  var hasEmptyField = false;
	  if(!accountDetails.password) {
	  	statusMessageElement.update($L('Please enter a password'));
		statusMessageWrapper.show();
		hasEmptyField = true;
	  } else {
	  	statusMessageElement.update('');
	    statusMessageWrapper.hide();
	  }	  
	  
	  if(hasEmptyField) {
	  	this.stopLoginSpinner();
		return;
	  }
	  
	  this.startLoginSpinner();
      this.accountId = this.params.id;
      this.updateAccountRequest = MessagingMojoService.updateAccountPassword(this.controller, this.params.id,accountDetails.password,this.accountUpdated.bind(this),this.updateLoginFailed.bind(this));
    },
	
    accountUpdated : function(result) {
		// do nothing when we encounter a pending status.  If returnValue = true is found, this means
		// that the user is currently logged in and we have just saved their new credentials.
		if ((result.status && result.status == "successful") || result.returnValue) {
			if (this.updateAccountRequest) {
				this.controller.cancelServiceRequest(this.updateAccountRequest);
			}
			// set the last availability selection to online
			MessagingMojoService.setLastAvailabilitySelection(this.controller, this.Messaging.Availability.AVAILABLE);
			this.popScene();
		}
    },

	popScene: function() {    
		if (this.params.popScenesTo && this.params.popScenesTo.length > 0) {
			this.controller.stageController.popScenesTo(this.params.popScenesTo);
		} else {
			Mojo.Log.warn("Error: popScenesTo param is not set");
			this.controller.stageController.popScene();
		}
	},
    
	updateLoginFailed : function(result){
		if(this.updateAccountRequest)
			this.controller.cancelServiceRequest(this.updateAccountRequest);

		this.stopLoginSpinner();
		var errorText = this.Messaging.Error.getLoginErrorFromCode(result.errorCode);
		this.controller.get('StatusMessage').update(errorText);		 
 	    this.controller.get('StatusMessageWrapper').show();		 
    },

	startLoginSpinner: function() {
		this.saveButtonModel.buttonLabel = this.loginButtonPending;
		this.controller.modelChanged(this.saveButtonModel);
		this.disableForm();
		this.saveButton.mojo.activate();
	},
	
	stopLoginSpinner: function() {
		this.saveButtonModel.buttonLabel = this.loginButtonDefault;
		this.controller.modelChanged(this.saveButtonModel);	
		this.enableForm();
		this.saveButton.mojo.deactivate();
	},
	
	startRemoveSpinner: function() {
		this.deleteButtonModel.buttonLabel = this.deleteButtonPending;
		this.controller.modelChanged(this.deleteButtonModel);			
		this.disableForm();
		this.removeAccountButton.mojo.activate();
	},
	
	stopRemoveSpinner: function() {
		this.deleteButtonModel.buttonLabel = this.deleteButtonDefault;
		this.controller.modelChanged(this.deleteButtonModel);	
		this.enableForm();
		this.removeAccountButton.mojo.deactivate();
	},	
    
	enableForm: function() {
		this.saveButtonModel.disabled = false;
		this.controller.modelChanged(this.saveButtonModel);	  
		this.passwordModel.disabled = false;
		this.controller.modelChanged(this.passwordModel);	 	
		
		// the username field is always disabled in edit mode
		// the delete account button is only visible in edit mode
		if (this.params.editMode) {
			this.deleteButtonModel.disabled = false;
			this.controller.modelChanged(this.deleteButtonModel);			
		} else {
			this.usernameModel.disabled = false;
			this.controller.modelChanged(this.usernameModel);	
		}		
	},
	
	disableForm: function() {
		this.saveButtonModel.disabled = true;
		this.controller.modelChanged(this.saveButtonModel);
		this.passwordModel.disabled = true;
		this.controller.modelChanged(this.passwordModel);	 

		// the username field is always disabled in edit mode
		// the delete account button is only visible in edit mode
		if (this.params.editMode) {
			this.deleteButtonModel.disabled = true;
			this.controller.modelChanged(this.deleteButtonModel);			
		} else {
			this.usernameModel.disabled = true;
			this.controller.modelChanged(this.usernameModel);				
		}		
	},
	
	confirmRemoveAccount: function() {
		this.controller.showAlertDialog({
			onChoose: function(removeAccount) {
				if(removeAccount) {
					this.removeAccount();
				} else {
					this.stopRemoveSpinner();
				}
			}.bind(this),
			title: $L("Remove account"),
			message: $L("Are you sure you want to remove this instant messaging account and all associated data from your device?"),
			preventCancel: true,
			choices: [
				{label: $L("Remove account"), value: true, type: 'negative'},
				{label: $L("Keep account"), value: false, type: 'color'}
			]
		});
	},
	
	removeAccount: function() {
		this.startRemoveSpinner();
        MessagingMojoService.removeAccount(this.controller, this.params.id,this.handleRemoveAccountSuccess.bind(this));        
	},	
	
	handleRemoveAccountSuccess: function() {
		this.stopRemoveSpinner();
		this.popScene();
	},
	
    considerForNotification: function(data) {
      if (data.notificationType == this.Messaging.notificationTypes.connectionFailure && data.accountId == this.accountId) {
        data = {};
      }
      return data;
    }
});

PrefsSetupAccountAssistant.kDummyPassword = "******";
