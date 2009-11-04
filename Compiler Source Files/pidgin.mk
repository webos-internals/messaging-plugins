pidgin_VERSION=2.6.2
pidgin_DIR=pidgin-$(pidgin_VERSION)

#pidgin_CPPFLAGS=
#pidgin_LDFLAGS=

#
# pidgin_BUILD_DIR is the directory in which the build is done.
# pidgin_SOURCE_DIR is the directory which holds all the
# patches and ipkg control files.
#
# You should not change any of these variables.
#
pidgin_BUILD_DIR=$(BUILD_DIR)/pidgin-$(pidgin_VERSION)
pidgin_SOURCE_DIR=$(SOURCE_DIR)/pidgin-$(pidgin_VERSION)

.PHONY: pidgin-source pidgin-unpack pidgin pidgin-stage pidgin-clean pidgin-dirclean pidgin-check


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
$(pidgin_BUILD_DIR)/.configured: $(DL_DIR)/$(pidgin_SOURCE) $(pidgin_PATCHES) make/pidgin.mk
	#$(MAKE) <bar>-stage <baz>-stage
	(cd $(@D); \
		$(TARGET_CONFIGURE_OPTS) \
		CPPFLAGS="$(STAGING_CPPFLAGS) $(pidgin_CPPFLAGS)" \
		LDFLAGS="$(STAGING_LDFLAGS) $(pidgin_LDFLAGS)" \
		./configure \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--target=$(GNU_TARGET_NAME) \
		--prefix=/opt \
		--disable-nls \
		--disable-static \
		--prefix=/usr --enable-gnutls=yes --enable-nss=yes --disable-idn --disable-dbus --disable-screensaver --disable-startup-notification --disable-gtkspell --disable-gstreamer --disable-vv --disable-avahi --disable-perl --disable-tcl --disable-gtkui\
	)
	$(PATCH_LIBTOOL) $(@D)/libtool
	touch $@

pidgin-unpack: $(pidgin_BUILD_DIR)/.configured

#
# This builds the actual binary.
#
$(pidgin_BUILD_DIR)/.built: $(pidgin_BUILD_DIR)/.configured
	rm -f $@
	$(MAKE) -C $(@D)
	touch $@

#
# This is the build convenience target.
#
pidgin: $(pidgin_BUILD_DIR)/.built

#
# If you are building a library, then you need to stage it too.
#
$(pidgin_BUILD_DIR)/.staged: $(pidgin_BUILD_DIR)/.built
	rm -f $@
	$(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
	touch $@

pidgin-stage: $(pidgin_BUILD_DIR)/.staged

#
# This is called from the top level makefile to clean all of the built files.
#
pidgin-clean:
	rm -f $(pidgin_BUILD_DIR)/.built
	-$(MAKE) -C $(pidgin_BUILD_DIR) clean

#
# This is called from the top level makefile to clean all dynamically created
# directories.
#
pidgin-dirclean:
	rm -rf $(BUILD_DIR)/$(pidgin_DIR) $(pidgin_BUILD_DIR)