pidgin-facebookchat_VERSION=1.61
pidgin-facebookchat_DIR=pidgin-facebookchat

#
# If the compilation of the package requires additional
# compilation or linking flags, then list them here.
#
pidgin-facebookchat_CPPFLAGS=
pidgin-facebookchat_LDFLAGS=

#
# pidgin-facebookchat_BUILD_DIR is the directory in which the build is done.
# pidgin-facebookchat_SOURCE_DIR is the directory which holds all the
# patches and ipkg control files.
#
# You should not change any of these variables.
#
pidgin-facebookchat_BUILD_DIR=$(BUILD_DIR)/pidgin-facebookchat
pidgin-facebookchat_SOURCE_DIR=$(SOURCE_DIR)/pidgin-facebookchat

.PHONY: pidgin-facebookchat-source pidgin-facebookchat-unpack pidgin-facebookchat pidgin-facebookchat-stage pidgin-facebookchat-clean pidgin-facebookchat-dirclean pidgin-facebookchat-check

#
# The source code depends on it existing within the download directory.
# This target will be called by the top level Makefile to download the
# source code's archive (.tar.gz, .bz2, etc.)
#
pidgin-facebookchat-source: $(DL_DIR)/$(pidgin-facebookchat_SOURCE) $(pidgin-facebookchat_PATCHES)

#
# This target unpacks the source code in the build directory.
# If the source archive is not .tar.gz or .tar.bz2, then you will need
# to change the commands here.  Patches to the source code are also
# applied in this target as required.
#
# This target also configures the build within the build directory.
# Flags such as LDFLAGS and CPPFLAGS should be passed into configure
# and NOT $(MAKE) below.  Passing it to configure causes configure to
# correctly BUILD the Makefile with the right paths, where passing it
# to Make causes it to override the default search paths of the compiler.
#
# If the compilation of the package requires other packages to be staged
# first, then do that first (e.g. "$(MAKE) <bar>-stage <baz>-stage").
#
# If the package uses  GNU libtool, you should invoke $(PATCH_LIBTOOL) as
# shown below to make various patches to it.
#
$(pidgin-facebookchat_BUILD_DIR)/.configured: $(DL_DIR)/$(pidgin-facebookchat_SOURCE) $(pidgin-facebookchat_PATCHES) make/pidgin-facebookchat.mk
	#$(MAKE) <bar>-stage <baz>-stage
	
	#$(PATCH_LIBTOOL) $(@D)/libtool
	touch $@

pidgin-facebookchat-unpack: $(pidgin-facebookchat_BUILD_DIR)/.configured

#
# This builds the actual binary.
#
$(pidgin-facebookchat_BUILD_DIR)/.built: $(pidgin-facebookchat_BUILD_DIR)/.configured
	rm -f $@
	$(MAKE) libfacebook.so -C $(@D)
	touch $@

#
# This is the build convenience target.
#
pidgin-facebookchat: $(pidgin-facebookchat_BUILD_DIR)/.built

#
# If you are building a library, then you need to stage it too.
#
$(pidgin-facebookchat_BUILD_DIR)/.staged: $(pidgin-facebookchat_BUILD_DIR)/.built
	rm -f $@
	$(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
	touch $@

pidgin-facebookchat-stage: $(pidgin-facebookchat_BUILD_DIR)/.staged

#
# This is called from the top level makefile to clean all of the built files.
#
pidgin-facebookchat-clean:
	rm -f $(pidgin-facebookchat_BUILD_DIR)/.built
	-$(MAKE) -C $(pidgin-facebookchat_BUILD_DIR) clean

#
# This is called from the top level makefile to clean all dynamically created
# directories.
#
pidgin-facebookchat-dirclean:
	rm -rf $(BUILD_DIR)/$(pidgin-facebookchat_DIR) $(pidgin-facebookchat_BUILD_DIR)