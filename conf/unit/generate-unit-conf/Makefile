.PHONY: all

MAIN = afm-unit

FILES = $(MAIN) $(shell ls *.inc) Makefile

DIR = ..

D = -DAGL_DEVEL

all: $(DIR)/afm-unit.conf.in $(DIR)/afm-unit-debug.conf.in

$(DIR)/afm-unit.conf.in: $(FILES)
	@echo generating $@
	@m4 $< | sed '/^[ \t]*\\*[ \t]*$$/d' > $@

$(DIR)/afm-unit-debug.conf.in: $(FILES)
	@echo generating $@
	@m4 $D $< | sed '/^[ \t]*\\*[ \t]*$$/d' > $@

