imaccountvalidator_VERSION=1.0
imaccountvalidator_DIR=imaccountvalidator-$(imaccountvalidator_VERSION)

#
# imaccountvalidator_BUILD_DIR is the directory in which the build is done.
# imaccountvalidator_SOURCE_DIR is the directory which holds all the
# patches and ipkg control files.
#
# You should not change any of these variables.
#
imaccountvalidator_BUILD_DIR=$(BUILD_DIR)/imaccountvalidator-$(imaccountvalidator_VERSION)
imaccountvalidator_SOURCE_DIR=$(SOURCE_DIR)/imaccountvalidator-$(imaccountvalidator_VERSION)

.PHONY: imaccountvalidator-source imaccountvalidator-unpack imaccountvalidator imaccountvalidator-stage imaccountvalidator-clean imaccountvalidator-dirclean imaccountvalidator-check


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
$(imaccountvalidator_BUILD_DIR)/.configured: $(DL_DIR)/$(imaccountvalidator_SOURCE) $(imaccountvalidator_PATCHES) make/imaccountvalidator.mk
	touch $@

imaccountvalidator-unpack: $(imaccountvalidator_BUILD_DIR)/.configured

#
# This builds the actual binary.
#
$(imaccountvalidator_BUILD_DIR)/.built: $(imaccountvalidator_BUILD_DIR)/.configured
	rm -f $@
	$(MAKE) -C $(@D)
	touch $@

#
# This is the build convenience target.
#
imaccountvalidator: $(imaccountvalidator_BUILD_DIR)/.built

#
# If you are building a library, then you need to stage it too.
#
$(imaccountvalidator_BUILD_DIR)/.staged: $(imaccountvalidator_BUILD_DIR)/.built
	rm -f $@
	$(MAKE) -C $(@D) DESTDIR=$(STAGING_DIR) install
	touch $@

imaccountvalidator-stage: $(imaccountvalidator_BUILD_DIR)/.staged

#
# This is called from the top level makefile to clean all of the built files.
#
imaccountvalidator-clean:
	rm -f $(imaccountvalidator_BUILD_DIR)/.built
	-$(MAKE) -C $(imaccountvalidator_BUILD_DIR) clean

#
# This is called from the top level makefile to clean all dynamically created
# directories.
#
imaccountvalidator-dirclean:
	rm -rf $(BUILD_DIR)/$(imaccountvalidator_DIR) $(imaccountvalidator_BUILD_DIR)