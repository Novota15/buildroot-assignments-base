##############################################################
#
# AESD_CHAR
#
##############################################################

#TODO: Fill up the contents below in order to reference your assignment 8 git contents
AESD_CHAR_VERSION = a392d42b73371bc32d45c3bd27b7beb3ace82f72
# Note: Be sure to reference the *ssh* repository URL here (not https) to work properly
# with ssh keys and the automated build/test system.
# Your site should start with git@github.com:
AESD_CHAR_SITE = 'git@github.com:Novota15/aesd-assignments.git'
AESD_CHAR_SITE_METHOD = git
AESD_CHAR_GIT_SUBMODULES = YES

AESD_CHAR_MODULE_SUBDIRS = aesd-char-driver

define AESD_CHAR_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 $(@D)/aesd-char-driver/aesdchar_load $(TARGET_DIR)/bin
	$(INSTALL) -m 0755 $(@D)/aesd-char-driver/aesdchar_unload $(TARGET_DIR)/bin
	$(INSTALL) -m 0755 $(@D)/aesd-char-driver/aesdchar-start-stop.sh $(TARGET_DIR)/etc/init.d/S97aesdchar
endef

$(eval $(kernel-module))
$(eval $(generic-package))