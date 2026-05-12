
##############################################################
#
# AESD-ASSIGNMENTS
#
##############################################################

# Use locally vendored copy of the aesd-assignments tree.  The source is kept
# inside this external buildroot tree under package/aesd-assignments/src so we
# can carry the assignment 9 driver/socket changes alongside the buildroot
# configuration in this repository.
AESD_ASSIGNMENTS_VERSION = local
AESD_ASSIGNMENTS_SITE = $(BR2_EXTERNAL_project_base_PATH)/package/aesd-assignments/src
AESD_ASSIGNMENTS_SITE_METHOD = local

define AESD_ASSIGNMENTS_BUILD_CMDS
	$(INSTALL) -d 0755 $(@D)/conf/ $(TARGET_DIR)/root/conf/
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)/finder-app all
	$(MAKE) $(TARGET_CONFIGURE_OPTS) USE_AESD_CHAR_DEVICE=1 -C $(@D)/server clean all
endef

# TODO add your writer, finder and finder-test utilities/scripts to the installation steps below
define AESD_ASSIGNMENTS_INSTALL_TARGET_CMDS
	$(INSTALL) -d 0755 $(@D)/conf/ $(TARGET_DIR)/etc/finder-app/conf/
	$(INSTALL) -m 0755 $(@D)/conf/* $(TARGET_DIR)/etc/finder-app/conf/
	$(INSTALL) -m 0755 $(@D)/conf/* $(TARGET_DIR)/root/conf/
	$(INSTALL) -m 0755 $(@D)/assignment-autotest/test/assignment4/* $(TARGET_DIR)/usr/bin
	$(INSTALL) -m 0755 $(@D)/server/aesdsocket $(TARGET_DIR)/usr/bin
	$(INSTALL) -m 0755 $(@D)/server/aesdsocket-start-stop $(TARGET_DIR)/etc/init.d/S99aesdsocket
endef

$(eval $(generic-package))
