/* Copyright 2009 Palm, Inc.  All rights reserved. */

var ListviewAssistant = Class.create(App.Scene, {
	initialize: function($super, params) {
		$super();
		this.params = params;
		this.appController = Mojo.Controller.appController;
		this.Messaging = this.appController.Messaging;
		this.Messaging.listviewSceneAssistant = this; // store a copy of the app assistant in the app controller
													  // this is nulled out in the cleanup method.  Keep an eye on this
													  // because we need to be sure to always destroy the reference.
		this.isWindowFocused = true;
		this.isSceneActive = false;
		this.isScreenOn = true;
		this.isScreenLocked = false;
		this.buddyListContainer;
		this.historyListContainer;
		this.isDoneLoggingIn = true;
		this.currentAvailability;
		this.currentListView;
		this.offlineGroup = "\uFAD7\uFAD7\uFAD7\uFAD7";
		this.groups = {};
		this.groupSubscriptions = [];
		this.subscriptions = {};
		this.buddyViewScrollPosition = null;
		this.historyViewScrollPosition = null;
		this.historyListPrevDateBucket;
		this.isNoConversationsVisible = false;
		this.customMessage = '';
		this.customMessageField;
		this.filterField = {};
		this.historyListFilterString = "";
		this.hasAppLoaded = false;
		this.isHistoryListInitialized = false;
		this.isBuddyListInitialized = false;
		this.isBuddyListHeaderVisible = true;
		this.buddyListSubscriberId;
		this.buddyListFilterString;
		this.buddyListDelay;
		this.buddyListReset = false;
		this.historyListFilterString;
		this.historyListSubscriberId;
		this.historyListReset = false;
		this.delayUpdateCount = 0;
		this.delayExecuteCount = 0;
	
		this.COMMAND_MENU = {
    		loadComposeView: {command:'cmdLoadComposeView',target: this.loadComposeView.bind(this)},
	    	loadPreferencesView: {command:Mojo.Menu.prefsCmd,target: this.loadPreferencesView.bind(this)},
    		loadHistoryView: {command:'cmdLoadHistoryView',target: this.loadHistoryView.bind(this)},
    		loadBuddyView: {command: 'cmdLoadBuddyView',target: this.loadBuddyView.bind(this)},
			toggleOfflineBuddies: {command: 'cmdShowOfflineBuddies',target: this.toggleOfflineBuddies.bind(this)},
			loadDebugView: {command: 'cmdLoadDebugView',target: this.loadDebugView.bind(this)}
		};
		
		var selectedView = this.COMMAND_MENU.loadHistoryView.command;
		if (this.params && this.params.forceListView) {
			if (this.params.forceListView == this.Messaging.Views.BUDDY) {
				selectedView = this.COMMAND_MENU.loadBuddyView.command;
			}
		}
		// Specifying the width in pixels is a hack.  The widget should support percentages.
		this.topMenuModelItems = [
			{label:'Chat History / Buddy List', toggleCmd:selectedView, items:[
				{label:$L('Conversations'), command:this.COMMAND_MENU.loadHistoryView.command, width:160},
				{label:$L('Buddies'),       command:this.COMMAND_MENU.loadBuddyView.command,   width:160}
			]}
		];
		
		this.commandMenuModel = [
			{label:$L('Compose new message'), icon:'newchat', command:this.COMMAND_MENU.loadComposeView.command}
		];

		this.availabilities = [
			{label: $L('Available'), command:this.Messaging.Availability.getAvailabilityAsConstText(this.Messaging.Availability.AVAILABLE), secondaryIcon:'status-available'},
			{label: $L('Busy'),      command:this.Messaging.Availability.getAvailabilityAsConstText(this.Messaging.Availability.BUSY),      secondaryIcon:'status-away'},
			// libpurple doesn't support invisible status for gtalk so commenting this out for now.
			//{label: $L('Invisible'), command:this.Messaging.Availability.getAvailabilityAsConstText(this.Messaging.Availability.INVISIBLE),  secondaryIcon:'status-invisible'},
			{label: $L('Sign off'),  command:this.Messaging.Availability.getAvailabilityAsConstText(this.Messaging.Availability.OFFLINE),   secondaryIcon:'status-offline'}
		];
		
		// To avoid the overhead of binding to "this" for methods
		// that are called repetedly, we are binding them once in
		// this initialize() method and referencing them
		this.handleCustomMessageFieldKeyUp = function(e) {
			this.updateCustomMessageCommitIcon();
			if (e.keyCode == 13) {
				e.target.blur();
			}
		}.bind(this);
		
		this.updateCustomMessageCommitIcon = function() {
			if (this.customMessage != this.customMessageField.value) 
				this.controller.get('commitCustomMessage').show();
			else 
				this.controller.get('commitCustomMessage').hide();
		}.bind(this);
	
		this.handleIMAccountDataChange = function(data, forceUpdate) {
			if (data.count > 0) {
				// we have at least one IM account - display the view toggle
				this.controller.setMenuVisible(Mojo.Menu.viewMenu, true);
				if (!this.controller.sceneElement.hasClassName('has-im-account')) {
					this.controller.sceneElement.addClassName('has-im-account');
				}

				// get the current availability
				var currentAvailability = null;
				var hasOffline = false;
				var isPending = false;
				for (var i = 0; i < data.list.length && !isPending; i++) {
					var account = data.list[i];
					// If an account is not offline, then we can clear out any notifications
					// that may have been queued from losing wifi connectivity when the screen turned off
					if (account.availability != this.Messaging.Availability.OFFLINE) 
						this.appController.notificationAssistant.clearErrorNotificationsForAccount(account.accountId);

					if (currentAvailability == null || account.availability < currentAvailability) 
						currentAvailability = account.availability;

					if (account.availability == this.Messaging.Availability.OFFLINE) 
						hasOffline = true;
					
					if (account.availability == this.Messaging.Availability.PENDING) {
						currentAvailability = this.Messaging.Availability.PENDING;
						isPending = true;
					}
				}
				
				if (!this.Messaging.pendingManager.isPending() || forceUpdate) {
					this.setAvailabilityIndicator(currentAvailability, hasOffline);
				} else {
					this.setAvailabilityIndicator(this.Messaging.Availability.PENDING, hasOffline);
				}
			} else {
				// has no IM accounts - hide toggle and show history view only
				this.controller.setMenuVisible(Mojo.Menu.viewMenu, false);
				if (this.controller.sceneElement.hasClassName('has-im-account')) {
					this.controller.sceneElement.removeClassName('has-im-account');
				}
				this.loadHistoryView(false);
			}
		}.bind(this);
		
		this.Messaging.pendingManager.setPendingCompleteFunction(this.handleIMAccountDataChange.bind(this));
		
		if (this.Messaging.messagingPrefs && this.Messaging.messagingPrefs.customMessage) 
			this.customMessage = this.Messaging.messagingPrefs.customMessage;
		
		this.filterImmediate = this.filterImmediate.bind(this);
		this.filter = this.filter.bind(this);
		this.handleHistoryListTap = this.handleHistoryListTap.bindAsEventListener(this);
		this.handleHistoryListDelete = this.handleHistoryListDelete.bindAsEventListener(this);
		this.handleBuddyListTap = this.handleBuddyListTap.bindAsEventListener(this);
		this.handleAvailabilityPickerTap = this.handleAvailabilityPickerTap.bindAsEventListener(this);
		this.customMessageUpdate = this.customMessageUpdate.bind(this);
		this.handleScreenStateChange = this.handleScreenStateChange.bind(this);
		this.handleFocus = this.handleFocus.bind(this);
		this.handleLoseFocus = this.handleLoseFocus.bind(this);
	},
	
	setAvailabilityIndicator: function(availability, hasOffline) {
		this.currentAvailability = availability;
		if (availability === this.Messaging.Availability.PENDING) {
			if (this.presenceSpinnerModel.spinning == false) {
				this.presenceSpinnerModel.spinning = true;
				this.controller.get('presence-spinner').show();
				this.controller.get('presenceicon-button').hide();
				this.controller.modelChanged(this.presenceSpinnerModel);
			}
			this.setCustomMessageHintText(true);
			return;
		} else if (this.presenceSpinnerModel.spinning === true) {
			this.presenceSpinnerModel.spinning = false;
			this.controller.get('presence-spinner').hide();
			this.controller.get('presenceicon-button').show();
			this.controller.modelChanged(this.presenceSpinnerModel);
		}
	
		// Build the class name that defines the availability icon
		// Example: icon status-available partial
		var partial = '';
		if ((availability == this.Messaging.Availability.AVAILABLE ||
			availability == this.Messaging.Availability.BUSY) &&
			hasOffline) {
			partial = " partial";
		}
		this.presenceIconElement.className = 'icon status-' + this.Messaging.Availability.getNormalAvailabilityClass(availability) + partial;
		this.setCustomMessageHintText(true);
	},
	
	setup: function() {
		this.appMenuAttrs = {
			omitDefaultItems: true
		};
		this.appMenuModel = {
			visible: true,
			label: $L('List view menu'),
			items: this.buildAppMenuItems()
		};

		this.controller.setupWidget(Mojo.Menu.appMenu, this.appMenuAttrs, this.appMenuModel);

		// Setup the top (view) menu
		this.topMenuModel = {
			visible: false,
			items: this.topMenuModelItems
		};
		this.controller.setupWidget(Mojo.Menu.viewMenu, undefined, this.topMenuModel);
	
		// Setup the bottom (command) menu
		this.cmdMenuModel = {
			visible: true,
			items: this.commandMenuModel
		};
	
		this.controller.setupWidget(Mojo.Menu.commandMenu, {}, this.cmdMenuModel);
	
		this.filterField = this.controller.get('filterField');
		this.buddyListHeader = this.controller.get('buddyListHeader');
		this.controller.setupWidget('filterField', {filterFieldName: 'filterFieldElement'}, this.filterField);
		this.filterField.observe(Mojo.Event.filterImmediate, this.filterImmediate);
		this.filterField.observe(Mojo.Event.filter, this.filter);

		this.presenceSpinnerAttrs = {
			superClass: 'presence-spinner-animation',
			startFrameCount: 0,
			mainFrameCount: 12,
			finalFrameCount: 0,
			fps: 24,
			frameHeight: 32
		};
		this.presenceSpinnerModel = {
			spinning: false
		};
		this.controller.setupWidget('presence-spinner', this.presenceSpinnerAttrs, this.presenceSpinnerModel);

		this.buddyListModel = {}; // This is used to reset the list by calling modelChanged
		this.buddyListAttributes = {
			itemTemplate: 'listview/buddyList-row',
			itemsCallback: this.buddyListItemsCallback.bind(this),
			dividerTemplate: 'listview/buddyList-divider',
			dividerFunction: this.handleDrawerSelection.bind(this),
			delay: 1,
			optimizedOptIn: true,
			lookahead: 50,
			renderLimit: 60,
			scrollThreshold: 1200
		};
		this.controller.setupWidget('buddy-list', this.buddyListAttributes, this.buddyListModel);
		this.buddyListWidget = this.controller.get('buddy-list');
		this.buddyListUpdateEventHandler = this.buddyListUpdateEvent.bind(this, this.buddyListWidget);
		this.buddyListThrottler = new MessagingUtils.delayedListUpdater(this.buddyListUpdateEventHandler, 5, 30);
		
		// Render the history view regardless if we have an account
		// For SMS & MMS
		this.historyListModel = {}; // This is used to reset the list by calling modelChanged
		this.historyListAttributes = {
			itemTemplate: 'listview/historyList-row',
			itemsCallback: this.historyListItemsCallback.bind(this),
			dividerTemplate: 'listview/historyList-divider',
			dividerFunction: function(item) {
				if (!item) {
					return;
				}
				return item.dividerText;
			}.bind(this),
			swipeToDelete: true,
			uniquenessProperty: 'id',
			delay: 1,
			optimizedOptIn: true,
			lookahead: 50,
			renderLimit: 60,
			scrollThreshold: 1200
		};
		this.controller.setupWidget('buddy-list-history', this.historyListAttributes, this.historyListModel);
		this.historyListWidget = this.controller.get('buddy-list-history');
		this.historyNoConversations = this.controller.get('historyNoConversations');
		this.historyListUpdateEventHandler = this.historyListUpdateEvent.bind(this, this.historyListWidget);
		this.historyListThrottler = new MessagingUtils.delayedListUpdater(this.historyListUpdateEventHandler, 5, 30);

		// observe clicks on the buddy list & history list
		this.historyListWidget.observe(Mojo.Event.listTap, this.handleHistoryListTap);
		// observe swipe to delete on the history list
		this.historyListWidget.observe(Mojo.Event.listDelete, this.handleHistoryListDelete);
		this.buddyListWidget.observe(Mojo.Event.listTap, this.handleBuddyListTap);
		
		//Handle Collapse
		this.controller.listen('buddy-list', Mojo.Event.tap, this.handleDrawerSelection.bind(this.buddyListWidget.show()));
	
		//set hint text later
		this.customMessageTextWidgetAttributes = {
			textFieldName: "customMessageTextElement",
			multiline: false,
			focus: false,
			requiresEnterKey: true,
			modelProperty: "value",
			focusMode: Mojo.Widget.focusSelectMode
		};
		this.customMessageTextWidgetModel = {};
		this.setCustomMessageHintText(); //setup the custom message
		this.controller.setupWidget('customMessageTextWidget', this.customMessageTextWidgetAttributes, this.customMessageTextWidgetModel);
		MessagingMojoService.isOfflineGroupCollapsed(this.controller, this.handleIsBuddyListCollapsed.bind(this));
	},
	
	handleHistoryListTap: function(e) {
		this.launchChatView(e.item.id);
	},
	
	handleHistoryListDelete: function(event) {
		var chatThreadId = event.item.id;
		Mojo.Controller.appController.notificationAssistant.clearNotificationsForChat(chatThreadId);
		MessagingMojoService.deleteChatThread(this.controller, chatThreadId);
	},
	
	handleBuddyListTap: function(e) {
		this.launchChatView(e.item.chatId, { selectIMTransport: true });
	},
	
	handleAvailabilityPickerTap: function(event) {
		this.controller.popupSubmenu({
			onChoose: this.handleAvailabilitySelection.bind(this),
			toggleCmd: this.Messaging.Availability.getAvailabilityAsConstText(this.currentAvailability),
			placeNear: event.target,
			items: this.availabilities
		});
	},

	//custom message is either: user set custom message OR availability OR custom message
	setCustomMessageHintText: function(update) {
		var availabilityString = this.Messaging.Availability.getAvailabilityAsDisplayText(this.currentAvailability);
		this.customMessageTextWidgetAttributes.hintText = availabilityString;
		if (this.currentAvailability == this.Messaging.Availability.OFFLINE ||
			this.currentAvailability == this.Messaging.Availability.PENDING) {
			this.customMessageTextWidgetModel.value = "";
			this.customMessageTextWidgetModel.disabled = true;
		} else {
			this.customMessageTextWidgetModel.disabled = false;
			if (this.customMessage) {
				this.customMessageTextWidgetModel.value = this.customMessage;
			}
		}
		
		if (update) {
			this.controller.modelChanged(this.customMessageTextWidgetModel);
		}
	},
	
	ready: function() {
		this.buddyListContainer = this.controller.get('buddyListContainer');
		this.historyListContainer = this.controller.get('historyListContainer');
		this.presenceIconElement = this.controller.get('presenceicon');
		this.commitCustomMessageElement = this.controller.get('commitCustomMessage');
		this.availabilityPickerElement = this.controller.get('AvailabilityPicker');
		
		this.handleLaunchParams(this.params);
		// call handleIMAccountDataChange immediately because the headless window
		// already has the data
		this.handleIMAccountDataChange(this.Messaging.IMAccounts.data);
		// observe for changes from now on
		this.Messaging.IMAccounts.observe(this.handleIMAccountDataChange);
		
		// Moving custom field setup/observer code from setup to here.  When the scene is first loaded, focus
		// is being put into the field and then taken out.  This causes the blur event to fire and cause problems
		// in the callback.
		
		// Setup the custom message field with hint text
		// Observe changes to the custom message and show/hide
		// the checkmark image to commit changes
		this.customMessageField = this.controller.get('customMessageTextWidget').querySelector('[name=customMessageTextElement]');
		//this.customMessage = this.customMessageField.value;
		this.customMessageField.observe('blur', this.customMessageUpdate);
		this.customMessageField.observe('keyup', this.handleCustomMessageFieldKeyUp);
		this.commitCustomMessageElement.observe(Mojo.Event.tap, this.customMessageUpdate);
		this.availabilityPickerElement.observe(Mojo.Event.tap, this.handleAvailabilityPickerTap);
		
		this.controller.document.addEventListener(Mojo.Event.stageActivate, this.handleFocus, false);
		this.controller.document.addEventListener(Mojo.Event.stageDeactivate, this.handleLoseFocus, false);
	
		this.Messaging.DisplayState.observe(this.handleScreenStateChange);
		MessagingMojoService.getLockStatus(this.controller, this.handleLockScreenStateChange.bind(this));
	
		// The custom message textbox will be focused by default
		// Unfocus it so the typedown filter can capture keys
		this.controller.setInitialFocusedElement(null);
	},
	
	handleLaunchParams: function(params) {
		if (params) {
			if (params.forceListView) {
				if (params.forceListView == this.Messaging.Views.HISTORY) {
					this.loadHistoryView(true, params.listDataChunk);
				} else {
					this.loadBuddyView(true, params.listDataChunk);
				}
			}
			
			// handle focusing the window last
			if (params.focusWindow) {
				this.controller.stageController.activate();
			}
			
			if (params.showDialog && params.showDialog.title && params.showDialog.message) {
				this.controller.showAlertDialog({
					onChoose: function(choice){},
					title: params.showDialog.title,
					message: params.showDialog.message,
					preventCancel: false,
					choices: [{
						label: $L('Close'),
						value: 'Close'
					}]
				});
			}
		}
	},
	
	handleCommand: function(event) {
		// handle menu button command events
		if (event.type == Mojo.Event.command) {
			// determine if we have a target function for this command
			var menu = $H(this.COMMAND_MENU);
			menu.keys().each(function(key) {
				var menuCommand = menu.get(key);
				if (menuCommand.command == event.command) {
					menuCommand.target();
					event.stop();
					throw $break;
				}
			});
		} else if (event.type == Mojo.Event.commandEnable && event.command == Mojo.Menu.prefsCmd) { // this is weird
			// Enable prefs menuitem for this scene.
			event.stopPropagation();
		}
	},

	considerForNotification: function(data) {
		// always put the notification through if the screen is off
		// if we are in the history view and the screen is on, do not display a banner + dashboard, just play a notification sound
		if (this.isScreenOn) {
			if (data.notificationType == this.Messaging.notificationTypes.newMessage && this.currentListView == this.Messaging.Views.HISTORY) {
				data = {playSoundOnly:true};
			}
		}
		return data;
	},

	buildAppMenuItems: function() {
		var appMenuPreferencesItem = {
			label: $L('Preferences & Accounts'),
			command: this.COMMAND_MENU.loadPreferencesView.command,
			enabled: true
		};
		var items = [Mojo.Menu.editItem];

		if (this.isOfflineCollapsed != undefined && this.currentListView == this.Messaging.Views.BUDDY) {
			var appMenuOfflineItem = {
				label: '',
				command: this.COMMAND_MENU.toggleOfflineBuddies.command,
				enabled: true
			};
			if (this.isOfflineCollapsed) {
				appMenuOfflineItem.label = $L('Show Offline Buddies');
			} else {
				appMenuOfflineItem.label = $L('Hide Offline Buddies');
			}
			items.push(appMenuOfflineItem);
		}
		
		items.push(appMenuPreferencesItem);
		items.push(Mojo.Menu.helpItem);
		
		var appMenuDebugItem = {
			label: $L('Debug'),
			command: this.COMMAND_MENU.loadDebugView.command,
			enabled: true
		};
		
		if (this.Messaging.debugMenuEnabled) {
			items.push(appMenuDebugItem);
		}
		
		return items;
	},
	
	updateAppMenuItems: function() {
		this.appMenuModel.items = this.buildAppMenuItems();
		this.controller.modelChanged(this.appMenuModel);
	},
	
	handleIsBuddyListCollapsed: function(data) {
		if (!data || data.isCollapsed == undefined) {
			return;
		}
		this.isOfflineCollapsed = data.isCollapsed;
		this.updateAppMenuItems();
	},
	
	toggleOfflineBuddies: function() {
		if (this.isOfflineCollapsed) {
			this.isOfflineCollapsed = false;
			MessagingMojoService.expandGroup(this.controller, "", true);
		} else {
			this.isOfflineCollapsed = true;
			MessagingMojoService.collapseGroup(this.controller, "", true);
		}
		this.updateAppMenuItems();
		// force the list to update
		this.buddyListThrottler.forceUpdate();
	},

	/***************************
	 * BUDDY VIEW
	 ***************************/
	buddyListItemsCallback: function(listWidget, offset, limit) {
		if (!this.isBuddyListInitialized) 
			return;
		//Mojo.Log.info("*** buddyListItemsCallback: offset: " + offset + " limit: " + limit);
		
		this.buddyListQuery(listWidget, offset, limit);
	},
	
	buddyListQuery: function(listWidget, offset, limit) {
		if (!listWidget) {
			return;
		}
		
		this.buddyListOffset = offset || 0;
		this.buddyListLimit = limit || 160;

		if (!this.preFormatBuddyListFn) {
			this.preFormatBuddyListFn = this.preFormatBuddyList.bind(this, listWidget);
		}
			
		var serviceRequest = MessagingMojoService.getBuddyList(
														this.controller, 
														this.buddyListFilterString, 
														this.preFormatBuddyListFn, 
														true, 
														this.buddyListSubscriberId, 
														this.buddyListReset, 
														this.buddyListOffset, 
														this.buddyListLimit);
		this.buddyListReset = false;
		if (!this.buddyListSubscription && serviceRequest) {
			this.buddyListSubscription = serviceRequest;
		}
	},
	
	// Format the buddy list data before it is rendered
	preFormatBuddyList: function(listWidget, result) {
		var offset = 0;
		var restorePosition = false;
		var headerElement;
		var topOffsetBeforeUpdate;
		var topOffsetDelta;
		var lastItem;
		
		if (result.subscriberId) {
			this.buddyListSubscriberId = result.subscriberId;
		}
		
		if (result.offset) {
			offset = result.offset;
		}
		
		if (result.updated) {
			// If pending, reset the timeout to 6 seconds.  This will allow the pending state
			// to finish after 6 seconds of no list updates
			if (this.Messaging.pendingManager.isPending()) {
				this.Messaging.pendingManager.resetTimeoutIfRunning(6000);
			}

			// Do not process any list updates if this buddy view is not visible
			if (this.currentListView != this.Messaging.Views.BUDDY) {
				return;
			}

			// delay the list updates if we are in a pending state
			// priority updates should be pushed through right away
			if (result.descriptions && (result.descriptions.incomingMsg || result.descriptions.markChatUnread || result.descriptions.loggedOut)) {
				this.buddyListThrottler.forceUpdate(); // force the update
			} else { // delay list updates for things that are not high priority
				this.buddyListThrottler.delayUpdate();
			}

			return; // return from this function because we have an update
		}

		for (var i = 0; i < result.list.length; i++) {
			var buddy = result.list[i];
			this.transformBuddy(buddy);
			if (buddy.customMessage == undefined || buddy.customMessage.length == 0) {
				buddy.hideCustomMessage = "hide-custom-message";
			}
			if (buddy.groupName && buddy.groupName == this.offlineGroup) {
				buddy.dividerText = $L("Offline");
			} else {
				buddy.dividerText = buddy.groupName;
			}
		}

		// If the availability picker header is visible in the window, check if its position changes
		// after we call noticeUpdatedItems.  The list will maintain the position of list elements. A new
		// divider causes scroll position problemsproblems.
		
		headerElement = this.controller.get('buddyListHeader');
		if (headerElement) {
			topOffsetBeforeUpdate = Mojo.View.viewportOffset(headerElement).top;
			restorePosition = topOffsetBeforeUpdate > 0 && topOffsetBeforeUpdate < this.controller.window.innerHeight;
		}
	
		listWidget.mojo.noticeUpdatedItems(offset, result.list);
	
		if (restorePosition) {
			headerElement = this.controller.get('buddyListHeader');
			if (headerElement) {
				topOffsetDelta = (topOffsetBeforeUpdate - Mojo.View.viewportOffset(headerElement).top);
				if (topOffsetDelta) {
					this.controller.sceneScroller.mojo.adjustBy(0, topOffsetDelta);
				}
			}
		} else {
			// if the last list item if scrolled off the top of the screen, call revealBottom so the user is not
			// at a blank list.  We can get in this state when the offline group is collapsed while it takes up the 
			// entire viewable area
			lastItem = this.buddyListWidget.mojo.getNodeByIndex(this.buddyListWidget.mojo.getLength() - 1);
			if (lastItem && Mojo.View.viewportOffset(lastItem).top < 0) {
				this.controller.sceneScroller.mojo.revealBottom();
			}
		}


		// Get the real count of the full list after we have rendered the first window
		if (!this.hasSetInitialLength) {
			this.hasSetInitialLength = true;
			MessagingMojoService.getBuddyListCount(this.controller, this.buddyListFilterString, function(listWidget, data) {
				// if the length is already set to this value, force the list to create the subscription
				if (listWidget.mojo.getLength() == data.count) {
					this.buddyListQuery(listWidget);
				} else {
					listWidget.mojo.setLength(data.count);
				}
			}.bind(this, listWidget));
			return;
		}
	},
	
	buddyListUpdateEvent: function(listWidget) {
		// cancel our request and null out the subscriber ID
		// we need to do this because the sort key in the database
		// has probably changed
		this.buddyListResetSubscription();
		// update the count and reload the window
		MessagingMojoService.getBuddyListCount(this.controller, this.buddyListFilterString, function(listWidget, data) {
			var newDataSize = data.count;
			// if the count has changed, set the length
			//var range = listWidget.mojo.getLoadedItemRange();
			var listLength = listWidget.mojo.getLength();
			// if the list is empty and we don't have a subscription, call model changed to set it up
			if (newDataSize == 0 && !this.buddyListSubscription) {
				this.controller.modelChanged(this.buddyListModel);
			} else if (listLength == 0) {
				// if the list is empty, make the service request ourselves to avoid the scrolling bug that happens
				// when you set the list length before having any items in the list
				var sizeDelta = newDataSize - listLength;
				this.buddyListQuery(listWidget, listLength, sizeDelta);
			} else if (newDataSize != listLength) {
				//console.log("Length has changed, List length: " + listLength + " New Length: " + newDataSize);
				listWidget.mojo.setLengthAndInvalidate(newDataSize);
				if (newDataSize == 0) {
					// snap to the top when the offline group is collapsed and the new list size is zero
					this.controller.sceneScroller.mojo.scrollTo(undefined, 0);
				}
			} else { // just invalidate the items because we have the same amount
				listWidget.mojo.invalidateItems(0);
			}
		}.bind(this, listWidget));
	},
	
	buddyListResetSubscription: function() {
		this.buddyListReset = true;
	},
	
	
	/***************************
	 * HISTORY VIEW
	 ***************************/
	historyListItemsCallback: function(listWidget, offset, limit) {
		if (!this.isHistoryListInitialized) {
			return;
		}
		this.historyListQuery(listWidget, offset, limit);
	},

	historyListQuery: function(listWidget, offset, limit) {
		if (!listWidget) {
			return;
		}
		
		// it would be better to ask the list for the initial window size instead of hard coding it here
		this.historyListOffset = offset || 0;
		this.historyListLimit = limit || 160;

		if (!this.preFormatHistoryListFn) {
			this.preFormatHistoryListFn = this.preFormatHistoryList.bind(this, listWidget);
		}

		var serviceRequest = MessagingMojoService.getConversationList(
															this.controller, 
															this.historyListFilterString, 
															this.preFormatHistoryListFn, 
															true, 
															this.historyListSubscriberId, 
															this.historyListReset, 
															this.historyListOffset, 
															this.historyListLimit);

		this.historyListReset = false;
		if (!this.historyListSubscription && serviceRequest) {
			this.historyListSubscription = serviceRequest;
		}
	},

	// Format the history list data before it is rendered
	preFormatHistoryList: function(listWidget, result) {
		if (result.subscriberId) {
			this.historyListSubscriberId = result.subscriberId;
		}
	
		var offset = 0;
		if (result.offset) 
			offset = result.offset;
	
		if (result.updated) {
			// Do not process any list updates if this history view is not visible
			if (this.currentListView != this.Messaging.Views.HISTORY) {
				return;
			}

			// priority updates should be pushed through right away
			if (result.descriptions && (result.descriptions.incomingMsg || result.descriptions.outgoingMsg || result.descriptions.markChatUnread || result.descriptions.loggedOut)) {
				this.historyListThrottler.forceUpdate(); // force the update
			} else { // delay list updates for things that are not high priority
				this.historyListThrottler.delayUpdate();
			}

			return;
		}
		
		// add extra formatting to the history list data
		for (var i = 0; i < result.list.length; i++) {
			var buddy = result.list[i];
			if (ChatFlags.isOutgoing(buddy.flags)) {
				buddy.messageActionImage = "images/outgoing.png";
			} else {
				buddy.messageActionImage = "images/incoming.png";
			}
			
			this.transformBuddy(buddy);
			
			if (buddy.summary)
				buddy.summary = buddy.summary.stripTags().strip();

			if (!buddy.summary)
				buddy.summary = $L('(No summary)');
			
			var d = new Date();
			d.setTime(buddy.chatTimeStamp);
			buddy.dividerText = BucketDateFormatter.getDateBucket(d);
		}
		
		listWidget.mojo.noticeUpdatedItems(offset, result.list);
		
		// Get the real count of the full list after we have rendered the first window
		if (!this.historyHasSetInitialLength) {
			this.historyHasSetInitialLength = true;
			MessagingMojoService.getConversationListCount(this.controller, this.historyListFilterString, function(listWidget, data) {
				// set the list size
				// this will make the list call the chatListItemsCallback with offset: 0 limit: 50
				this.handleHistoryListNoConversations(data.count);
				// if the length is already set to this value, force the list to call the itemsCallback to create the subscription
				if (listWidget.mojo.getLength() == data.count) {
					this.historyListQuery(listWidget);
				} else {
					listWidget.mojo.setLength(data.count);
				}
			}.bind(this, listWidget));
			return;
		}
	},
	
	handleHistoryListNoConversations: function(length) {
		if (!length && !this.isNoConversationsVisible) {
			this.isNoConversationsVisible = true;
			this.historyNoConversations.show();
		} else if (length && this.isNoConversationsVisible) {
			this.isNoConversationsVisible = false;
			this.historyNoConversations.hide();
		}
	},
	
	historyListUpdateEvent: function(listWidget) {
		// Need to trash the java cache because the sort keys have changed
		this.historyListResetSubscription();
		
		// update the count and reload the window
		MessagingMojoService.getConversationListCount(this.controller, this.historyListFilterString, function(listWidget, data) {
			var newDataSize = data.count;
			this.handleHistoryListNoConversations(newDataSize);
			// if the count has changed, set the length
			var listLength = listWidget.mojo.getLength();
			
			// if the list is empty and we don't have a subscription, call model changed to set it up
			// this situation can happen when you remove an account
			if (newDataSize == 0 && !this.historyListSubscription) {
				this.controller.modelChanged(this.historyListModel);
			} else if (listLength == 0) { //&& newDataSize > 0
				// if the list is empty, make the service request ourselves to avoid the scrolling bug that happens
				// when you set the list length before having any items in the list
				var sizeDelta = newDataSize - listLength;
				this.historyListQuery(listWidget, listLength, sizeDelta);
			} else if (newDataSize != listLength) {
				listWidget.mojo.setLengthAndInvalidate(newDataSize);
			} else { // just invalidate the items because we have the same amount
				listWidget.mojo.invalidateItems(0);
			}
		}.bind(this, listWidget));
	},

	setupSubscriptions: function() { // need this stubbed out method so subscriptions will be enabled in app_scene.js
	},

	historyListResetSubscription: function() {
		this.historyListReset = true;
	},

	/**
	 * maximize/minimize will fire when we are in/out of card mode and when this scene is activated/deactivated
	 */
	maximizeSubscriptions: function() {
		this.buddyListThrottler.maximize();
		this.historyListThrottler.maximize();
	},
	
	minimizeSubscriptions: function() {
		this.buddyListThrottler.minimize();
		this.historyListThrottler.minimize();
	},

	transformBuddy: function(buddy) {
		if ((buddy.firstName == undefined || buddy.firstName.length == 0) && (buddy.lastName == undefined || buddy.lastName.length == 0)) 
			buddy.firstName = buddy.chatAddress;

		buddy.displayName = MessagingUtils.formatPhoneNumber((buddy.firstName + " " + buddy.lastName).strip());
		
		buddy.avatar = buddy.pictureLocation || buddy.imAvatarLocation || "images/list-view-default-avatar.png";
		buddy.imAvatarOverlay = "images/list-view-avatar-frame.png";
		
		if (buddy.unreadCount == undefined || buddy.unreadCount == 0) {
			buddy.unreadCountClass = "hide-unread-count";
		} else {
			buddy.unreadCountChatAddress = "unreadCountChatAddress";
		}
		
		buddy.availabilityClass = this.Messaging.Availability.getNormalAvailabilityClass(buddy.availability);
	},
	
	filterImmediate: function(event) {
		var filterString = event.filterString;
		if (this.currentListView == this.Messaging.Views.BUDDY) {
			// hide the buddy list header during typedown filter
			// we need to do this because it is part of the list, simply
			// overlaying the filter field will not work
			if (filterString && this.isBuddyListHeaderVisible) {
				this.buddyListHeader.hide();
				this.isBuddyListHeaderVisible = false;
			}
		}
	},
	
	filter: function(event) {
		var filterString = event.filterString;
		// filter the list that we are looking at:
		if (this.currentListView == this.Messaging.Views.HISTORY) {
			this.filterHistoryList(filterString);
		} else {
			this.filterBuddyList(filterString);
		}
		
		if (!filterString && !this.isBuddyListHeaderVisible) {
			this.buddyListHeader.show();
			this.isBuddyListHeaderVisible = true;
		}
	},
	
	filterBuddyList: function(filterString) {
		this.buddyListFilterString = filterString;
		this.buddyListResetSubscription();
		MessagingMojoService.getBuddyList(
									this.controller, 
									this.buddyListFilterString, 
									this.handleFilteredBuddyList.bind(this,filterString), 
									false, 
									null, 
									false, 
									0, 
									this.buddyListWidget.mojo.maxLoadedItems());
	},

	handleFilteredBuddyList: function(filterString, result) {
		if (filterString == this.buddyListFilterString) {
			this.filterField.mojo.setCount(result.list.length);
			this.buddyListWidget.mojo.setLength(0);
			this.controller.sceneScroller.mojo.scrollTo(undefined, 0);
			this.preFormatBuddyList(this.buddyListWidget, result);
		}
	},

	filterHistoryList: function(filterString) {
		this.historyListFilterString = filterString;
		this.historyListResetSubscription();
		MessagingMojoService.getConversationList(
										this.controller, 
										this.historyListFilterString, 
										this.handleFilteredHistoryList.bind(this,filterString),
										false, 
										null, 
										false, 
										0, 
										this.buddyListWidget.mojo.maxLoadedItems());
		
	},
	
	handleFilteredHistoryList: function(filterString, result) {
		if (filterString == this.historyListFilterString) {
			this.filterField.mojo.setCount(result.list.length);
			this.historyListWidget.mojo.setLength(0);
			this.controller.sceneScroller.mojo.scrollTo(undefined, 0);
			this.preFormatHistoryList(this.historyListWidget, result);
		}
	},
	
	clearFilterField: function() {
		this.filterField.mojo.close();
		if (!this.isBuddyListHeaderVisible) {
			this.buddyListHeader.show();
			this.isBuddyListHeaderVisible = true;
		}
	},

	loadBuddyView: function(shouldUpdateToggle, listDataChunk) {
		if (shouldUpdateToggle && this.topMenuModelItems[0].toggleCmd != this.COMMAND_MENU.loadBuddyView.command) {
			this.topMenuModelItems[0].toggleCmd = this.COMMAND_MENU.loadBuddyView.command;
			this.controller.modelChanged(this.topMenuModel);
		}

		if (!this.currentListView || this.currentListView != this.Messaging.Views.BUDDY) {
			this.clearFilterField();
			this.currentListView = this.Messaging.Views.BUDDY;
			this.updateAppMenuItems();
			this.Messaging.messagingPrefs.isHistoryViewSelected = false;
			this.historyViewScrollPosition = this.controller.sceneScroller.mojo.getState();
			
			var transition = null;
			if (this.isBuddyListInitialized) {
				transition = this.controller.prepareTransition(Mojo.Transition.crossFade);
			}
			
			this.historyListContainer.hide();
			this.buddyListContainer.show();
			this.controller.showWidgetContainer(this.buddyListContainer);
			this.controller.hideWidgetContainer(this.historyListContainer);
			
			if (this.isBuddyListInitialized) {
				this.buddyListThrottler.forceUpdate(); // execute update event after the list is visible so it can retrieve an accurate item count
			} else {
				this.isBuddyListInitialized = true;
				
				if (!listDataChunk) {
					listDataChunk = {list:[]};
				}
				this.preFormatBuddyList(this.buddyListWidget, listDataChunk);
				
				if (!this.isHistoryListInitialized) {
					var initHistoryList = function() {
						this.isHistoryListInitialized = true;
						this.historyHasSetInitialLength = true;
						this.historyListThrottler.forceUpdate();
					}.bind(this);
					initHistoryList.delay(1);
				}
			}
			
			if (this.buddyViewScrollPosition) {
				this.controller.sceneScroller.mojo.setState(this.buddyViewScrollPosition);
			}
			if (transition) {
				transition.run();
			}
		}
	},

	loadHistoryView: function(shouldUpdateToggle, listDataChunk) {
		if (shouldUpdateToggle && this.topMenuModelItems[0].toggleCmd != this.COMMAND_MENU.loadHistoryView.command) {
			this.topMenuModelItems[0].toggleCmd = this.COMMAND_MENU.loadHistoryView.command;
			this.controller.modelChanged(this.topMenuModel);
		}
			
		if (!this.currentListView || this.currentListView != this.Messaging.Views.HISTORY) {
			this.clearFilterField();
			this.currentListView = this.Messaging.Views.HISTORY;
			this.updateAppMenuItems();
			this.Messaging.messagingPrefs.isHistoryViewSelected = true;
			this.buddyViewScrollPosition = this.controller.sceneScroller.mojo.getState();
			
			var transition = null;
			if (this.isHistoryListInitialized) {
				transition = this.controller.prepareTransition(Mojo.Transition.crossFade);
			}
			
			this.buddyListContainer.hide();
			this.historyListContainer.show();
			this.controller.hideWidgetContainer(this.buddyListContainer);
			this.controller.showWidgetContainer(this.historyListContainer);

			if (this.isHistoryListInitialized) {
				this.historyListThrottler.forceUpdate(); // execute update event after the list is visible so it can retrieve an accurate item count
			} else {
				this.isHistoryListInitialized = true;

				if (!listDataChunk) {
					listDataChunk = {list:[]};
				}
				this.preFormatHistoryList(this.historyListWidget, listDataChunk);
				
				if (!this.isBuddyListInitialized) {
					var initBuddyList = function() {
						this.isBuddyListInitialized = true;
						this.hasSetInitialLength = true;
						this.buddyListThrottler.forceUpdate();
					}.bind(this);
					initBuddyList.delay(1);
				}
			}

			if (this.historyViewScrollPosition) {
				this.controller.sceneScroller.mojo.setState(this.historyViewScrollPosition);
			}
			
			if (transition) {
				transition.run();
			}
			
			// don't clear the dashboard if we are pushing the listview + another scene on top
			if (!this.params || (this.params && !this.params.skipListViewFocus)) 
				this.handleClearNotifications(); // clear the dashboard when loading the history view, handleFocus() takes care of this logic
			else 
				this.skipListViewFocus = false;
			
		}
	},
	
	loadComposeView: function(params) {
		this.controller.stageController.pushScene('compose', params);
	},
	
	loadPreferencesView: function() {
		this.controller.stageController.pushScene({
			name: 'prefsAccountSummary',
			id: CONSTANTS.SCENE_ID_ACCOUNT_SUMMARY
		}, {
			popScenesTo: CONSTANTS.SCENE_ID_ACCOUNT_SUMMARY
		});
	},
	
	launchChatView: function(chatThreadId, params) {
		if (params === undefined) {
			params = {};
		}
		params.clearListBadgeFn = this.clearListBadgeForChatThreadId.bind(this, chatThreadId);
		this.controller.stageController.pushScene('chatview', chatThreadId, params);
	},
	
	clearListBadgeForChatThreadId: function(chatThreadId) {
		var badgeContainers = ["buddyBageContainer", "historyBageContainer"];
		var listItem;
		for (var i = 0; i < badgeContainers.length; i++) {
			listItem = this.controller.get(badgeContainers[i] + chatThreadId);
			if (listItem && !listItem.hasClassName('hide-unread-count')) {
				listItem.addClassName('hide-unread-count');
			}
		}
	},
	
	loadDebugView: function() {
		this.controller.stageController.pushScene('debug');
	},
	
	cleanup: function() {
		// save the listview state
		MessagingMojoService.setIsHistoryViewSelected(this.controller, (this.currentListView == this.Messaging.Views.HISTORY));
		this.Messaging.IMAccounts.stopObserving(this.handleIMAccountDataChange);
		this.filterField.stopObserving(Mojo.Event.filterImmediate, this.filterImmediate);
		this.filterField.stopObserving(Mojo.Event.filter, this.filter);
		this.historyListWidget.stopObserving(Mojo.Event.listTap, this.handleHistoryListTap);
		this.historyListWidget.stopObserving(Mojo.Event.listDelete, this.handleHistoryListDelete);
		this.buddyListWidget.stopObserving(Mojo.Event.listTap, this.handleBuddyListTap);
		this.customMessageField.stopObserving('blur', this.customMessageUpdate);
		this.customMessageField.stopObserving('keyup', this.handleCustomMessageFieldKeyUp);
		this.commitCustomMessageElement.stopObserving(Mojo.Event.tap, this.customMessageUpdate);
		this.availabilityPickerElement.stopObserving(Mojo.Event.tap, this.handleAvailabilityPickerTap);
		this.Messaging.DisplayState.stopObserving(this.handleScreenStateChange);
		this.controller.document.removeEventListener(Mojo.Event.stageActivate, this.handleFocus, false);
		this.controller.document.removeEventListener(Mojo.Event.stageDeactivate, this.handleLoseFocus, false);
		this.Messaging.pendingManager.clearPendingCompleteFunction();
		this.Messaging.listviewSceneAssistant = null; // null out the reference to the listview so that it can get garbage collected
	},

	buddyListDividerFunction: function(item) {
		if (!item) {
			return;
		}
		return item.groupName;
	},
	
	
	handleAvailabilitySelection: function(selectedValue) {
		if (selectedValue == undefined) 
			return;
		
		if (Object.isString(selectedValue)) {
			selectedValue = this.Messaging.Availability.getAvailabilityAsInteger(selectedValue);
		}
		
		if (selectedValue != this.Messaging.Availability.OFFLINE &&
		this.currentAvailability != this.Messaging.Availability.AVAILABLE &&
		this.currentAvailability != this.Messaging.Availability.BUSY) {
			this.setAvailabilityIndicator(this.Messaging.Availability.PENDING);
		} else {
			this.setAvailabilityIndicator(selectedValue);
		}
		
		MessagingMojoService.setPresence(this.controller, selectedValue, this.handleSetPresenceFailure.bind(this));
	},
	
	// if set presence failed, kill the pending state
	handleSetPresenceFailure: function() {
		this.Messaging.pendingManager.forcePendingComplete();
	},
	
	// activate will not be called when pushing multiple scenes at the same time
	// this is important when we push the listview + chatview because in this scenario 
	// we don't want the dashboard to be destroyed when the listview is pushed
	activate: function(params) {
		this.isSceneActive = true;
		// params will contain focusWindow:true when the app is launched with launchListView:true
		// the purpose of this is to focus the window after the async popScenesTo() method has been called
		if (params) {
			this.handleLaunchParams(params);
		}
		
		// We should clear the dashboard whenever a scene on top of the listview is popped
		this.handleClearNotifications();
	},

	deactivate: function() {
		this.isSceneActive = false;
	},

	handleScreenStateChange: function(isScreenOn) {
		Mojo.Log.info("[HV] ***** handle screen state change: " + isScreenOn);
		this.isScreenOn = isScreenOn;
		
		this.handleClearNotifications();
	},
	
	handleLockScreenStateChange: function(result) {
		Mojo.Log.info("[HV] ****** getLockStatus: %j", result);
		if (result && result.locked) {
			this.isScreenLocked = true;
		} else {
			this.isScreenLocked = false;
		}
		this.handleClearNotifications();
	},

	handleFocus: function() {
		this.isWindowFocused = true;
		this.handleClearNotifications();
	},
	
	handleLoseFocus: function() {
		this.isWindowFocused = false;
	},
	
	handleClearNotifications: function() {
		// kill the dashboard if we become focused while in the history view
		if (this.currentListView == this.Messaging.Views.HISTORY && this.isWindowFocused && this.isSceneActive && this.isScreenOn && !this.isScreneLocked) {
			this.appController.notificationAssistant.clearMessageDashboard();
		}
	},
	
	customMessageUpdate: function(e) {
		var newMessage = this.customMessageField.value;
		// Set the new custom message if it has changed or if it has been cleared
		if ((newMessage.length > 0 && newMessage != this.customMessage) ||
		(newMessage.length == 0 && this.customMessage.length > 0)) {
			this.customMessage = newMessage;
			MessagingMojoService.setCustomMessage(this.controller, newMessage);
			this.updateCustomMessageCommitIcon();
		}
	},
	
	toggleDebugMenu: function() {
		if (!this.Messaging.debugMenuEnabled) {
			this.Messaging.debugMenuEnabled = true;
			this.appMenuModel.items.push(this.appMenuDebugItem);
			this.controller.modelChanged(this.appMenuModel);
		} else {
			this.Messaging.debugMenuEnabled = false;
			// find the debug menu item and remove it
			var index = -1;
			for (var i = 0; i < this.appMenuModel.items.length; i++) {
				if (this.appMenuModel.items[i] === this.appMenuDebugItem) {
					index = i;
				}
			}
			if (i > -1) {
				this.appMenuModel.items.splice(index, 1);
			}
		}
	},
	
	handleDrawerSelection: function(item) {
		Mojo.Log.info("==============================111111111111111111111========================================");
	
		if (!item) {
			return;
		}
		
		Mojo.Log.info("==============================2222222222222222222222========================================");
		Mojo.Log.info("handleDrawerSelection ");
		
		var targetRow = this.controller.get('buddy_list');
		if (targetRow) {
			Mojo.Log.info("33333333333333333333333333333333333333333333333333");
			// if (!targetRow.hasClassName("selection_target")) {
				// Mojo.Log.info("handleSoftwareSelection !selection_target" );
				// Mojo.Log.info("4444444444444444444444444444444444444444444444");
				// targetRow = targetRow.up('.selection_target');
			// }		
			
			if (targetRow) {
			Mojo.Log.info("55555555555555555555555555555555555555555555555");
				var toggleButton = targetRow.down("div.arrow_button");
				if (!toggleButton.hasClassName('palm-arrow-expanded') && !toggleButton.hasClassName('palm-arrow-closed')) {
				Mojo.Log.info("66666666666666666666666666666666666666666");
					return;
				}
				var show = toggleButton.className;
				Mojo.Log.info("77777777777777777777777777777777777777777777777777777777");
				Mojo.Log.info("handleSoftwareSelection open/close " + show );
				this.toggleShowHideBuddies(targetRow,this.controller.window.innerHeight);
			}
		}
		
		Mojo.Log.info("888888888888888888888888888888888888888888");
		return item.groupName;
	},

	toggleShowHideBuddies: function (rowElement, viewPortMidway,noScroll) {
	Mojo.Log.info("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
		// if (!rowElement.hasClassName("details")) {
		// Mojo.Log.info("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
			// return;
		// }

		var toggleButton = rowElement.down("div.arrow_button");
		Mojo.Log.info("ccccccccccccccccccccccccccccccccccc");
		if (!toggleButton.hasClassName('palm-arrow-expanded') && !toggleButton.hasClassName('palm-arrow-closed')) {
		Mojo.Log.info("dddddddddddddddddddddddddddddddddddd");
			return;
		}

		Mojo.Log.info("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
		var showBuddies = toggleButton.hasClassName('palm-arrow-closed');
		//var buddyContainer = rowElement.down('.collapsor');
		var buddyContainer = rowElement;
		//var maxHeight = buddyContainer.getHeight();
		
		var maxHeight = 10;
		if (showBuddies) {
		Mojo.Log.info("ffffffffffffffffffffffffffffffff");
			toggleButton.addClassName('palm-arrow-expanded');
			toggleButton.removeClassName('palm-arrow-closed');
			buddyContainer.setStyle({ height:'1px' });
			buddyContainer.show();

			// See if the div should scroll up a little to show the contents
			var elementTop = buddyContainer.viewportOffset().top;
			var scroller = Mojo.View.getScrollerForElement(buddyContainer);
			Mojo.Log.info("gggggggggggggggggggggggggggggggggggggggg");
			if (elementTop > viewPortMidway && scroller && !noScroll) {
				//Using setTimeout to give the animation time enough to give the div enough height to scroll to
				var scrollToPos = scroller.mojo.getScrollPosition().top - (elementTop - viewPortMidway);
				setTimeout(function() {scroller.mojo.scrollTo(undefined, scrollToPos, true);}, 200);
				Mojo.Log.info("hhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
			}
		} else {
		Mojo.Log.info("iiiiiiiiiiiiiiiiiiiiiiiiiiii");
			buddyContainer.setStyle({ height: maxHeight + 'px' });
			toggleButton.addClassName('palm-arrow-closed');
			toggleButton.removeClassName('palm-arrow-expanded');
		}
      var options = {reverse: !showBuddies,
					   onComplete: this.animationComplete.bind(this, showBuddies, rowElement.id, maxHeight),
					   curve: 'over-easy',
					   from: 1,
					   to: maxHeight,
					   duration: 0.4};
		Mojo.Animation.animateStyle(buddyContainer, 'height', 'bezier', options);
	},
	
	animationComplete: function(show, accountId, listHeight, buddyContainer, cancelled) {
		if (!show) {
			buddyContainer.hide();
		}
		buddyContainer.setStyle({height:'auto'});
	}
});
