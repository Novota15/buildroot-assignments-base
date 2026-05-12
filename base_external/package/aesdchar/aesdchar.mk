
##############################################################
#
# AESD_CHAR
#
##############################################################

# Use locally vendored copy of the aesd-assignments tree (shared with the
# aesd-assignments package).  This carries the assignment 9 llseek/ioctl
# additions to the kernel module and the aesdsocket userspace counterpart.
AESD_CHAR_VERSION = local
AESD_CHAR_SITE = $(BR2_EXTERNAL_project_base_PATH)/package/aesd-assignments/src
AESD_CHAR_SITE_METHOD = local

AESD_CHAR_MODULE_SUBDIRS = aesd-char-driver

define AESD_CHAR_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 $(@D)/aesd-char-driver/aesdchar_load $(TARGET_DIR)/bin
	$(INSTALL) -m 0755 $(@D)/aesd-char-driver/aesdchar_unload $(TARGET_DIR)/bin
	$(INSTALL) -m 0755 $(@D)/aesd-char-driver/aesdchar-start-stop.sh $(TARGET_DIR)/etc/init.d/S97aesdchar
endef

$(eval $(kernel-module))
$(eval $(generic-package))
