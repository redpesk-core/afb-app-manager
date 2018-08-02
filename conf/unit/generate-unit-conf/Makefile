.PHONY: all

MAIN = afm-unit

FILES = $(MAIN) binder.inc provided-api.inc provided-binding.inc service.inc Makefile

DIR = ..

#AFM_CONFIG_DIR ?= /etc/afm
AFM_CONFIG_DIR ?= "@afm_confdir@"

C = -DAFM_CONFIG_DIR=$(AFM_CONFIG_DIR)

D = -DAGL_DEVEL

all: $(DIR)/afm-unit.conf.in $(DIR)/afm-unit-debug.conf.in

$(DIR)/afm-unit.conf.in: $(FILES)
	@echo generating $@
	@m4 $C $< | sed '/^[ \t]*\\*[ \t]*$$/d' > $@

$(DIR)/afm-unit-debug.conf.in: $(FILES)
	@echo generating $@
	@m4 $C $D $< | sed '/^[ \t]*\\*[ \t]*$$/d' > $@

