#ifndef _APP_VERSION_H_
#define _APP_VERSION_H_

#define STR_CONVERTER(s) #s
#define _STR(s_)  STR_CONVERTER(s_)
#define _CHAR(i_) ('0' + i_)

/// These version fields are used in the Zigbee Basic attributes
//  and other areas in the firmware.
#define APP_VERSION_MAJOR 8
#define APP_VERSION_MINOR 1
#define APP_VERSION_BUILD 4


// The APP_VERSION_STR_LEN must be properly maintained to represent the length
// of the APP_VERSION_STR, which is <MAJOR>.<MINOR>.<BUILD>.  If any of the
// version fields exceeds 1 character, this should be increased.
#define APP_VERSION_STR_LEN  5

#define APP_VERSION_STR  _STR(APP_VERSION_MAJOR) "." _STR(APP_VERSION_MINOR) "." _STR(APP_VERSION_BUILD)
#define APP_VERSION_CHARS  _CHAR(APP_VERSION_MAJOR),'.',_CHAR(APP_VERSION_MINOR),'.',_CHAR(APP_VERSION_BUILD)
#define APP_VERSION_U32  (((uint32_t)APP_VERSION_MAJOR << 16) + ((uint32_t)APP_VERSION_MINOR << 8) + ((uint32_t)APP_VERSION_BUILD))

#endif  // #ifndef _APP_VERSION_H_
