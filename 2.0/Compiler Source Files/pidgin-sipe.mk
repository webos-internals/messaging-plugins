pidgin-sipe_VERSION=1.6.3
pidgin-sipe_DIR=pidgin-sipe-$(pidgin-sipe_VERSION)

#pidgin-sipe_CPPFLAGS=`pkg-config --cflags glib-2.0 purple`
#pidgin-sipe_LDFLAGS=`pkg-config --libs purple`

#
# pidgin-sipe_BUILD_DIR is the directory in which the build is done.
# pidgin-sipe_SOURCE_DIR is the directory which holds all the
# patches and ipkg control files.
#
# You should not change any of these variables.
#
pidgin-sipe_BUILD_DIR=$(BUILD_DIR)/pidgin-sipe-$(pidgin-sipe_VERSION)
pidgin-sipe_SOURCE_DIR=$(SOURCE_DIR)/pidgin-sipe-$(pidgin-sipe_VERSION)

.PHONY: pidgin-sipe-source pidgin-sipe-unpack pidgin-sipe pidgin-sipe-stage pidgin-sipe-clean pidgin-sipe-dirclean pidgin-sipe-check


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
$(pidgin-sipe_BUILD_DIR)/.configured: $(DL_DIR)/$(pidgin-sipe_SOURCE) $(pidgin-sipe_PATCHES) make/pidgin-sipe.mk
	#$(MAKE) <bar>-stage <baz>-stage
	(cd $(@D); \
		$(TARGET_CONFIGURE_OPTS) \
		CPPFLAGS="$(STAGING_CPPFLAGS) $(pidgin-sipe_CPPFLAGS)" \
		LDFLAGS="$(STAGING_LDFLAGS) $(pidgin-sipe_LDFLAGS)" \
		./configure \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--target=$(GNU_TARGET_NAME) \
		--prefix=/usr\
	)
	$(PATCH_LIBTOOL) $(@D)/libtool
	touch $@

pidgin-sipe-unpack: $(pidgin-sipe_BUILD_DIR)/.configured

#
# This builds the actual binary.
#
$(pidgin-sipe_BUILD_DIR)/.built: $(pidgin-sipe_BUILD_DIR)/.configured
	rm -f $@
	$(MAKE) -C $(@D)
	touch $@

#
# This is the build convenience target.
#
pidgin-sipe: $(pidgin-sipe_BUILD_DIR)/.built

#
# If you are building a library, then you need to stage it too.
#
$(pidgin-sipe_BUILD_DIR)/.staged: $(pidgin-sipe_BUILD_DIR)/.built
	rm -f $@
	$(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
	touch $@

pidgin-sipe-stage: $(pidgin-sipe_BUILD_DIR)/.staged

#
# This is called from the top level makefile to clean all of the built files.
#
pidgin-sipe-clean:
	rm -f $(pidgin-sipe_BUILD_DIR)/.built
	-$(MAKE) -C $(pidgin-sipe_BUILD_DIR) clean

#
# This is called from the top level makefile to clean all dynamically created
# directories.
#
pidgin-sipe-dirclean:
	rm -rf $(BUILD_DIR)/$(pidgin-sipe_DIR) $(pidgin-sipe_BUILD_DIR)
