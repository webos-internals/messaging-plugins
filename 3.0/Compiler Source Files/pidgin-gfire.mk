pidgin-gfire_VERSION=0.9.3
pidgin-gfire_DIR=pidgin-gfire-$(pidgin-gfire_VERSION)

#pidgin-gfire_CPPFLAGS=`pkg-config --cflags glib-2.0 purple`
#pidgin-gfire_LDFLAGS=`pkg-config --libs purple`

#
# pidgin-gfire_BUILD_DIR is the directory in which the build is done.
# pidgin-gfire_SOURCE_DIR is the directory which holds all the
# patches and ipkg control files.
#
# You should not change any of these variables.
#
pidgin-gfire_BUILD_DIR=$(BUILD_DIR)/pidgin-gfire-$(pidgin-gfire_VERSION)
pidgin-gfire_SOURCE_DIR=$(SOURCE_DIR)/pidgin-gfire-$(pidgin-gfire_VERSION)

.PHONY: pidgin-gfire-source pidgin-gfire-unpack pidgin-gfire pidgin-gfire-stage pidgin-gfire-clean pidgin-gfire-dirclean pidgin-gfire-check


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
$(pidgin-gfire_BUILD_DIR)/.configured: $(DL_DIR)/$(pidgin-gfire_SOURCE) $(pidgin-gfire_PATCHES) make/pidgin-gfire.mk
	#$(MAKE) <bar>-stage <baz>-stage
	(cd $(@D); \
		$(TARGET_CONFIGURE_OPTS) \
		CPPFLAGS="$(STAGING_CPPFLAGS) $(pidgin-gfire_CPPFLAGS)" \
		LDFLAGS="$(STAGING_LDFLAGS) $(pidgin-gfire_LDFLAGS)" \
		./configure \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--target=$(GNU_TARGET_NAME) \
		--prefix=/usr\
		--disable-gtk\
	)
	$(PATCH_LIBTOOL) $(@D)/libtool
	touch $@

pidgin-gfire-unpack: $(pidgin-gfire_BUILD_DIR)/.configured

#
# This builds the actual binary.
#
$(pidgin-gfire_BUILD_DIR)/.built: $(pidgin-gfire_BUILD_DIR)/.configured
	rm -f $@
	$(MAKE) -C $(@D)
	touch $@

#
# This is the build convenience target.
#
pidgin-gfire: $(pidgin-gfire_BUILD_DIR)/.built

#
# If you are building a library, then you need to stage it too.
#
$(pidgin-gfire_BUILD_DIR)/.staged: $(pidgin-gfire_BUILD_DIR)/.built
	rm -f $@
	$(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
	touch $@

pidgin-gfire-stage: $(pidgin-gfire_BUILD_DIR)/.staged

#
# This is called from the top level makefile to clean all of the built files.
#
pidgin-gfire-clean:
	rm -f $(pidgin-gfire_BUILD_DIR)/.built
	-$(MAKE) -C $(pidgin-gfire_BUILD_DIR) clean

#
# This is called from the top level makefile to clean all dynamically created
# directories.
#
pidgin-gfire-dirclean:
	rm -rf $(BUILD_DIR)/$(pidgin-gfire_DIR) $(pidgin-gfire_BUILD_DIR)
