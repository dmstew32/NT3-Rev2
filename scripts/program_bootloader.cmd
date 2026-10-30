set COMMANDER=C:\SiliconLabs\SimplicityStudio\v5\developer\adapter_packs\commander\commander.exe
set BOOT_S37_IMAGE=..\bootloader\bootloader-storage-internal-single-1MB.s37
set KEYS_DIR=..\keys\
set USB_ADDR=440305960
set DEVICE=EFR32MG21A020F1024IM32

echo "Writing keys to the device"
%COMMANDER% flash --device %DEVICE% --tokengroup znet --tokenfile %KEYS_DIR%encryption-key --tokenfile %KEYS_DIR%signing-keys-tokens.txt

echo "Programming the bootloader"
%COMMANDER% flash %BOOT_S37_IMAGE% --device %DEVICE%


pause
