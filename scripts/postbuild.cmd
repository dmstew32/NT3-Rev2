set IMB_EXE="image-builder-windows.exe"
set COMMANDER_EXE="C:\SiliconLabs\SimplicityStudio\v5\developer\adapter_packs\commander\commander.exe"

:: Set the configuration values
set APP_VERSION=0x00090104
set MFG_ID=0x1574
set IMAGE_TYPE=0x0002

set APP_S37_IMAGE="..\GNU ARM v12.2.1 - Default\NT-3-Light.s37"
set APP_GBL_IMAGE="..\GNU ARM v12.2.1 - Default\NT-3-Light.gbl"
set APP_GBL_UNENC_IMAGE="..\GNU ARM v12.2.1 - Default\NT-3-Light_unenc.gbl"
set KEYS_DIR=..\keys\

:: Set OTA image
set APP_OTA_IMAGE="..\GNU ARM v12.2.1 - Default\NT-3-Light.ota"
set APP_OTA_UNENC_IMAGE="..\GNU ARM v12.2.1 - Default\NT-3-Light_unenc.ota"


:: Signing the .s37 is required for secure boot, but we are not using that feature.


:: create unencrypted gbl for use with old bootloaders
%COMMANDER_EXE% gbl create %APP_GBL_UNENC_IMAGE% --app %APP_S37_IMAGE%

%COMMANDER_EXE% ota create --upgrade-image %APP_GBL_UNENC_IMAGE% --manufacturer-id %MFG_ID% --image-type %IMAGE_TYPE% --firmware-version %APP_VERSION% --string "rel" -o %APP_OTA_UNENC_IMAGE%

:: create signed and encrypted gbl file
%COMMANDER_EXE% gbl create %APP_GBL_IMAGE% --app %APP_S37_IMAGE% --sign %KEYS_DIR%signing-key --encrypt %KEYS_DIR%encryption-key

:: create ota file from signed & encrypted gbl file
%COMMANDER_EXE% ota create --upgrade-image %APP_GBL_IMAGE% --manufacturer-id %MFG_ID% --image-type %IMAGE_TYPE% --firmware-version %APP_VERSION% --string "rel" -o %APP_OTA_IMAGE%


pause