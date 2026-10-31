# Host (PC) test for the button code.
#
# Builds the reference implementation (reference/, the original code) and the
# current code (Light/hal/hal_buttons.c + Light/buttons/) against a simulator,
# runs both through the same randomized button sessions, and compares every
# observable effect.  Run from this folder:   make        (needs gcc + python)
ifeq ($(origin CC),default)
CC        = gcc
endif
PYTHON   ?= python
CFLAGS    = -std=gnu17 -O1 -Wall -Wno-unused-function -include sim.h -Istubs
ROOT      = ../..
BUILD     = build
SEEDS    ?= 300
GESTURES ?= 40

NEW_SRC  = $(ROOT)/Light/hal/hal_buttons.c $(ROOT)/Light/buttons/app_button.c \
           $(ROOT)/Light/buttons/button_actions.c $(ROOT)/Light/buttons/fw_version_display.c
NEW_HDR  = $(ROOT)/Light/hal/hal_buttons.h $(ROOT)/Light/buttons/app_button.h \
           $(ROOT)/Light/buttons/button_actions.h $(ROOT)/Light/buttons/fw_version_display.h
FEATURES = $(ROOT)/Light/nwk_on_off/light_on_off.c:toggle_50_percent_mode,toggle_em_mode \
           $(ROOT)/Light/auto_commission/auto_comm.c:bx_auto_announce_task,start_local_auto_commission

# Some make/shell combinations on Windows start gcc without TMP/TEMP, and gcc then
# tries to write temporary files to C:\WINDOWS.  Give it a folder of its own.
export TMP  = $(BUILD)/tmp
export TEMP = $(BUILD)/tmp

.PHONY: all test clean
all: test

test: $(BUILD)/sim_ref.exe $(BUILD)/sim_new.exe
	$(PYTHON) compare.py $(BUILD)/sim_ref.exe $(BUILD)/sim_new.exe $(SEEDS) $(GESTURES)

# Sources are copied into their own folder so their #include "x.h" finds the
# stubs/ (and the button headers), not the real project headers next to them.
$(BUILD)/sim_ref.exe: sim.c sim.h variant_init.c reference/hal_buttons.c reference/hal_buttons.h
	mkdir -p $(BUILD)/ref $(BUILD)/tmp
	cp reference/hal_buttons.c reference/hal_buttons.h $(BUILD)/ref/
	$(CC) $(CFLAGS) -DSIM_REFERENCE -o $@ sim.c variant_init.c $(BUILD)/ref/hal_buttons.c

$(BUILD)/sim_new.exe: sim.c sim.h variant_init.c extract_funcs.py $(NEW_SRC) $(NEW_HDR) \
                      $(ROOT)/Light/nwk_on_off/light_on_off.c $(ROOT)/Light/auto_commission/auto_comm.c
	mkdir -p $(BUILD)/new $(BUILD)/tmp
	cp $(NEW_SRC) $(NEW_HDR) $(BUILD)/new/
	$(PYTHON) extract_funcs.py $(BUILD)/new/moved_features.c $(FEATURES)
	$(CC) $(CFLAGS) -I$(BUILD)/new -o $@ sim.c variant_init.c $(BUILD)/new/*.c

clean:
	rm -rf $(BUILD)
