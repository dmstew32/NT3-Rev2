
set ARM_TOOLS=C:\SiliconLabs\SimplicityStudio\v5\developer\toolchains\gnu_arm\12.2.rel1_2023.7\bin
set ELF=..\cmake_gcc\build\base\ota_dongle.out
set ADDRESS1=0x8043dab
set ADDRESS2=0x801c42b
set ADDRESS3=0x8019a25
set ADDRESS4=0x8021913
set ADDRESS5=0x8020581

%ARM_TOOLS%\arm-none-eabi-addr2line -e %ELF% %ADDRESS1%
%ARM_TOOLS%\arm-none-eabi-addr2line -e %ELF% %ADDRESS2%
%ARM_TOOLS%\arm-none-eabi-addr2line -e %ELF% %ADDRESS3%
%ARM_TOOLS%\arm-none-eabi-addr2line -e %ELF% %ADDRESS4%
%ARM_TOOLS%\arm-none-eabi-addr2line -e %ELF% %ADDRESS5%


pause
