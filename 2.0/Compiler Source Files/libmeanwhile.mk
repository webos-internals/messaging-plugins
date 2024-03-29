libmeanwhile_VERSION=1.0.2
libmeanwhile_DIR=libmeanwhile-$(libmeanwhile_VERSION)

#libmeanwhile_CPPFLAGS=
#libmeanwhile_LDFLAGS=

#
# libmeanwhile_BUILD_DIR is the directory in which the build is done.
# libmeanwhile_SOURCE_DIR is the directory which holds all the
# patches and ipkg control files.
#
# You should not change any of these variables.
#
libmeanwhile_BUILD_DIR=$(BUILD_DIR)/libmeanwhile-$(libmeanwhile_VERSION)
libmeanwhile_SOURCE_DIR=$(SOURCE_DIR)/libmeanwhile-$(libmeanwhile_VERSION)

.PHONY: libmeanwhile-source libmeanwhile-unpack libmeanwhile libmeanwhile-stage libmeanwhile-clean libmeanwhile-dirclean libmeanwhile-check


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
$(libmeanwhile_BUILD_DIR)/.configured: $(DL_DIR)/$(libmeanwhile_SOURCE) $(libmeanwhile_PATCHES) make/libmeanwhile.mk
	#$(MAKE) <bar>-stage <baz>-stage
	(cd $(@D); \
		$(TARGET_CONFIGURE_OPTS) \
		CPPFLAGS="$(STAGING_CPPFLAGS) $(libmeanwhile_CPPFLAGS)" \
		LDFLAGS="$(STAGING_LDFLAGS) $(libmeanwhile_LDFLAGS)" \
		./configure \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--target=$(GNU_TARGET_NAME) \
		--prefix=/opt \
		--disable-nls \
		--disable-static \
		--prefix=/usr --disable-doxygen\
	)
	$(PATCH_LIBTOOL) $(@D)/libtool
	touch $@

libmeanwhile-unpack: $(libmeanwhile_BUILD_DIR)/.configured

#
# This builds the actual binary.
#
$(libmeanwhile_BUILD_DIR)/.built: $(libmeanwhile_BUILD_DIR)/.configured
	rm -f $@
	$(MAKE) -C $(@D)
	touch $@

#
# This is the build convenience target.
#
libmeanwhile: $(libmeanwhile_BUILD_DIR)/.built

#
# If you are building a library, then you need to stage it too.
#
$(libmeanwhile_BUILD_DIR)/.staged: $(libmeanwhile_BUILD_DIR)/.built
	rm -f $@
	$(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
	touch $@

libmeanwhile-stage: $(libmeanwhile_BUILD_DIR)/.staged

#
# This is called from the top level makefile to clean all of the built files.
#
libmeanwhile-clean:
	rm -f $(libmeanwhile_BUILD_DIR)/.built
	-$(MAKE) -C $(libmeanwhile_BUILD_DIR) clean

#
# This is called from the top level makefile to clean all dynamically created
# directories.
#
libmeanwhile-dirclean:
	rm -rf $(BUILD_DIR)/$(libmeanwhile_DIR) $(libmeanwhile_BUILD_DIR)