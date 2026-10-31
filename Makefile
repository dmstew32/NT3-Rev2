# NT-3 Light: command-line build, flash and OTA packaging.
#
#   make build    compile, same as Build in Simplicity Studio
#   make clean    delete the build output (keeps the .gbl / .ota files)
#   make flash    program the firmware (.s37) with Simplicity Commander
#                 more than one adapter connected:  make flash SERIALNO=440322302
#   make ota      create the .gbl and .ota upgrade files (like scripts/postbuild.cmd);
#                 the firmware version is read from NT_3_app.h
#   make test     PC test of the button code (test/buttons; needs gcc and python)
#
# Run it with make.cmd, which uses the make, shell and compiler that come with
# Simplicity Studio:   make build  (cmd)    .\make build  (PowerShell)
#
# "make build" runs the makefile Simplicity Studio generates in the build folder.
# Studio must have built the project once, and again after source files are
# added or removed (Studio regenerates its makefile when it builds).

COMMANDER   ?= C:/SiliconLabs/SimplicityStudio/v5/developer/adapter_packs/commander/commander.exe

DEVICE      ?= EFR32MG21A020F1024IM32
SERIALNO    ?=
JOBS        ?= 8

BUILD_DIR    = GNU ARM v12.2.1 - Default
IMAGE        = $(BUILD_DIR)/NT-3-Light
KEYS_DIR    ?= keys

# OTA image header values (see scripts/postbuild.cmd)
MFG_ID      ?= 0x1574
IMAGE_TYPE  ?= 0x0002
APP_VERSION  = $(shell sed -n 's/^\#define APP_FIRMWARE_VERSION_U32 *\(0x[0-9A-Fa-f]*\).*/\1/p' NT_3_app.h)

.PHONY: build clean flash ota test check-build-dir

build: check-build-dir
	cd "$(BUILD_DIR)" && $(MAKE) -j$(JOBS) all

clean: check-build-dir
	cd "$(BUILD_DIR)" && $(MAKE) clean
	# Studio's clean keeps the images; remove them so a stale one can't be flashed
	rm -f "$(IMAGE).out" "$(IMAGE).hex" "$(IMAGE).bin" "$(IMAGE).s37" "$(IMAGE).map"

flash:
	@test -f "$(IMAGE).s37" || { echo "No $(IMAGE).s37 - run 'make build' first"; exit 1; }
	"$(COMMANDER)" flash "$(IMAGE).s37" --device $(DEVICE) $(if $(SERIALNO),--serialno $(SERIALNO))

ota:
	@test -f "$(IMAGE).s37" || { echo "No $(IMAGE).s37 - run 'make build' first"; exit 1; }
	@test -n "$(APP_VERSION)" || { echo "APP_FIRMWARE_VERSION_U32 not found in NT_3_app.h"; exit 1; }
	@test -f "$(KEYS_DIR)/signing-key" || { echo "No $(KEYS_DIR)/signing-key"; exit 1; }
	@echo "OTA version $(APP_VERSION), manufacturer $(MFG_ID), image type $(IMAGE_TYPE)"
	# unencrypted gbl and ota, for use with old bootloaders
	"$(COMMANDER)" gbl create "$(IMAGE)_unenc.gbl" --app "$(IMAGE).s37"
	"$(COMMANDER)" ota create --upgrade-image "$(IMAGE)_unenc.gbl" --manufacturer-id $(MFG_ID) \
		--image-type $(IMAGE_TYPE) --firmware-version $(APP_VERSION) --string "rel" -o "$(IMAGE)_unenc.ota"
	# signed and encrypted gbl and ota
	"$(COMMANDER)" gbl create "$(IMAGE).gbl" --app "$(IMAGE).s37" \
		--sign "$(KEYS_DIR)/signing-key" --encrypt "$(KEYS_DIR)/encryption-key"
	"$(COMMANDER)" ota create --upgrade-image "$(IMAGE).gbl" --manufacturer-id $(MFG_ID) \
		--image-type $(IMAGE_TYPE) --firmware-version $(APP_VERSION) --string "rel" -o "$(IMAGE).ota"

test:
	$(MAKE) -C test/buttons

check-build-dir:
	@test -f "$(BUILD_DIR)/makefile" || { echo "No '$(BUILD_DIR)/makefile' - build the project once in Simplicity Studio"; exit 1; }
	@command -v arm-none-eabi-gcc > /dev/null || { echo "arm-none-eabi-gcc not found - run this through make.cmd"; exit 1; }
