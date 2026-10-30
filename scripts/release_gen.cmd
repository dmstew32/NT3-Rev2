set VERSION=1.1.3
set S37_DIR="..\GNU ARM v12.2.1 - Default\"
set BOOTLOADER_DIR=..\bootloader\
set BOOTLOADER=bootloader-storage-internal-single-1MB.s37
set RELEASE_DIR=..\releases\v%VERSION%


MKDIR %RELEASE_DIR%
COPY %S37_DIR%NT-3-Light.gbl %RELEASE_DIR%\NT-3-Light_v%VERSION%.gbl
COPY %S37_DIR%NT-3-Light.s37 %RELEASE_DIR%\NT-3-Light_v%VERSION%.s37
COPY %S37_DIR%NT-3-Light.ota %RELEASE_DIR%\NT-3-Light_v%VERSION%.ota
COPY %S37_DIR%NT-3-Light.map %RELEASE_DIR%\NT-3-Light_v%VERSION%.map
COPY %BOOTLOADER_DIR%%BOOTLOADER% %RELEASE_DIR%\%BOOTLOADER%

7z a %RELEASE_DIR%\..\NT-3-Light_v%VERSION%.zip %RELEASE_DIR%\*

pause
