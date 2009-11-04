libpurple-adapterext_VERSION=1.2.0
libpurple-adapterext_DIR=libpurple-adapterext-$(libpurple-adapterext_VERSION)

#libpurple-adapterext_CPPFLAGS=
#libpurple-adapterext_LDFLAGS=

#
# libpurple-adapterext_BUILD_DIR is the directory in which the build is done.
# libpurple-adapterext_SOURCE_DIR is the directory which holds all the
# patches and ipkg control files.
#
# You should not change any of these variables.
#
libpurple-adapterext_BUILD_DIR=$(BUILD_DIR)/libpurple-adapterext-$(libpurple-adapterext_VERSION)
libpurple-adapterext_SOURCE_DIR=$(SOURCE_DIR)/libpurple-adapterext-$(libpurple-adapterext_VERSION)

.PHONY: libpurple-adapterext-source libpurple-adapterext-unpack libpurple-adapterext libpurple-adapterext-stage libpurple-adapterext-clean libpurple-adapterext-dirclean libpurple-adapterext-check


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
$(libpurple-adapterext_BUILD_DIR)/.configured: $(DL_DIR)/$(libpurple-adapterext_SOURCE) $(libpurple-adapterext_PATCHES) make/libpurple-adapterext.mk
	touch $@

libpurple-adapterext-unpack: $(libpurple-adapterext_BUILD_DIR)/.configured

#
# This builds the actual binary.
#
$(libpurple-adapterext_BUILD_DIR)/.built: $(libpurple-adapterext_BUILD_DIR)/.configured
	rm -f $@
	$(MAKE) -C $(@D)
	touch $@

#
# This is the build convenience target.
#
libpurple-adapterext: $(libpurple-adapterext_BUILD_DIR)/.built

#
# If you are building a library, then you need to stage it too.
#
$(libpurple-adapterext_BUILD_DIR)/.staged: $(libpurple-adapterext_BUILD_DIR)/.built
	rm -f $@
	$(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
	touch $@

libpurple-adapterext-stage: $(libpurple-adapterext_BUILD_DIR)/.staged

#
# This is called from the top level makefile to clean all of the built files.
#
libpurple-adapterext-clean:
	rm -f $(libpurple-adapterext_BUILD_DIR)/.built
	-$(MAKE) -C $(libpurple-adapterext_BUILD_DIR) clean

#
# This is called from the top level makefile to clean all dynamically created
# directories.
#
libpurple-adapterext-dirclean:
	rm -rf $(BUILD_DIR)/$(libpurple-adapterext_DIR) $(libpurple-adapterext_BUILD_DIR)