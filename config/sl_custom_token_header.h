/***************************************************************************//**
 * @file sl_custom_token_header.h
 * @brief Custom token header
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

/***************************************************************************//**
 * Custom Tokens Usage Examples
 *
 * User can create custom nvm3 tokens as shown below. Indexed tokens should be handled by application.
 *
 * #define TOKEN_BASICTOKEN1      SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0001), 0)
 * #define TOKEN_BASICTOKEN2      SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0002), 0)
 * #define TOKEN_COUNTERTOKEN1    SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0010), 1)
 * #define TOKEN_COUNTERTOKEN2    SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0020), 1)
 *
 * Refer sl_token_manager_defines.h file for SL_TOKEN_GET_DYNAMIC_TOKEN define
 * and MACRO's for NVM3 Default Instance Key Space Regions.
 *
 * User can create a token size as shown below.
 *
 * typedef uint16_t tokTypeBasicToken1;
 * typedef struct {
 *   uint8_t basicToken2Array[3];
 *   uint16_t basicToken2VarA;
 * } tokTypeBasicToken2;
 *
 * typedef uint32_t tokTypeCounterToken1;
 * typedef uint32_t tokTypeCounterToken2;
 *
 * #define TOKEN_BASICTOKEN1_SIZE       (sizeof(tokTypeBasicToken1))
 * #define TOKEN_BASICTOKEN2_SIZE       (sizeof(tokTypeBasicToken2))
 * #define TOKEN_COUNTERTOKEN1_SIZE     (sizeof(tokTypeCounterToken1))
 * #define TOKEN_COUNTERTOKEN2_SIZE     (sizeof(tokTypeCounterToken2))
 *
 * This code will read the token data:
 * Refer sl_token_manager_api.h file, for sl_token_manager_get_data() API usage
 *     uint8_t data[TOKEN_BASICTOKEN1_SIZE];
 *     sl_token_manager_get_data(TOKEN_BASICTOKEN1, &data, sizeof(data));
 *
 *********************************************************************************
 * For backward compatibility, the legacy token header file is still available, for
 * silicon labs series 2 devices.
 *
 * This format is deprecated and will be removed in the future.
 *
 * Custom Tokens Usage Examples
 *
 * #define BASICTOKEN1_DEFAULT                                            0xA5F0
 * #define BASICTOKEN2_DEFAULT                  { { 0xAA, 0xBB, 0xCC }, 0xDDDD }
 * #define BASICTOKEN3_DEFAULT                                         { { 0 } }
 * #define COUNTERTOKEN1_DEFAULT                                               0
 * #define COUNTERTOKEN2_DEFAULT                                      0xCCCCCCCC
 * #define INDEXEDTOKEN1_DEFAULT                                               0
 * #define INDEXEDTOKEN2_DEFAULT                                      { 0xDDDD }
 * #define INDEXEDTOKEN3_DEFAULT    { 0xAA, 0xBBBB, { 0x00, 0x11, 0x22 }, 0xCC }
 *
 * #ifdef DEFINETYPES
 * typedef uint16_t tokTypeBasicToken1;
 * typedef struct {
 *   uint8_t basicToken2Array[3];
 *   uint16_t basicToken2VarA;
 * } tokTypeBasicToken2;
 *
 * typedef struct {
 *   uint8_t basicToken3Array[254];
 * } tokTypeBasicToken3;
 *
 * typedef uint32_t tokTypeCounterToken1;
 * typedef uint32_t tokTypeCounterToken2;
 *
 * typedef uint8_t  tokTypeIndexedToken1Element;
 * typedef uint16_t tokTypeIndexedToken2Element;
 *
 * typedef struct {
 *   uint8_t indexedToken3VarA;
 *   uint16_t indexedToken3VarB;
 *   uint8_t indexedToken3Array[3];
 *   int8_t indexedToken3VarC;
 * } tokTypeIndexedToken3Element;
 *
 * #endif
 *
 * #ifdef DEFINETOKENS
 * DEFINE_BASIC_TOKEN(BASICTOKEN1,
 *                    tokTypeBasicToken1,
 *                    BASICTOKEN1_DEFAULT)
 * DEFINE_BASIC_TOKEN(BASICTOKEN2,
 *                    tokTypeBasicToken2,
 *                    BASICTOKEN2_DEFAULT)
 * DEFINE_BASIC_TOKEN(BASICTOKEN3,
 *                    tokTypeBasicToken3,
 *                    BASICTOKEN3_DEFAULT)
 * DEFINE_COUNTER_TOKEN(COUNTERTOKEN1,
 *                      tokTypeCounterToken1,
 *                      COUNTERTOKEN1_DEFAULT)
 * DEFINE_COUNTER_TOKEN(COUNTERTOKEN2,
 *                      tokTypeCounterToken2,
 *                      COUNTERTOKEN2_DEFAULT)
 * DEFINE_INDEXED_TOKEN(INDEXEDTOKEN1,
 *                      tokTypeIndexedToken1Element,
 *                      INDEXEDTOKEN1_ELEMENTS,
 *                      INDEXEDTOKEN1_DEFAULT)
 * DEFINE_INDEXED_TOKEN(INDEXEDTOKEN2,
 *                      tokTypeIndexedToken2Element,
 *                      INDEXEDTOKEN2_ELEMENTS,
 *                      INDEXEDTOKEN2_DEFAULT)
 * DEFINE_INDEXED_TOKEN(INDEXEDTOKEN3,
 *                      tokTypeIndexedToken3Element,
 *                      INDEXEDTOKEN3_ELEMENTS,
 *                      INDEXEDTOKEN3_DEFAULT)
 * #endif
 *
 ******************************************************************************/
#include "NT_3_app.h"

#define NVM33_TEST 3
#if (NVM33_TEST == 1)  /* Taken directly from above comment */
#define SL_TOKEN_NVM3_REGION_USER 0

#define TOKEN_BASICTOKEN1      SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0005), 0)

//typedef uint16_t tokTypeBasicToken1;

#define TOKEN_BASICTOKEN1_SIZE       (sizeof(uint16_t))

#elif (NVM33_TEST == 2)  /* Taken directly from above comment */

#define BASICTOKEN1_DEFAULT                                            0xA5F0

#ifdef DEFINETYPES
typedef uint16_t tokTypeBasicToken1;
#endif

#ifdef DEFINETOKENS
DEFINE_BASIC_TOKEN(BASICTOKEN1,
                   tokTypeBasicToken1,
                   BASICTOKEN1_DEFAULT)
#endif


#elif (NVM33_TEST == 3)  /* Working from gecko_sdk_4.4.2 */

#define NVM3KEY_APP_FLAGS                     (NVM3KEY_DOMAIN_USER | 0x0F00)
#define NVM3KEY_APP_OTA_DOWNLOAD_IN_PROGRESS  (NVM3KEY_DOMAIN_USER | 0x0F01)
#define NVM3KEY_APP_ADAPTIV_COMMISH_INFO      (NVM3KEY_DOMAIN_USER | 0x0F07)

#define NVM3KEY_APP_COLOR_TEMP_K              (NVM3KEY_DOMAIN_USER | 0x0F09)
#define NVM3KEY_APP_LIGHT_SENSOR_LUX          (NVM3KEY_DOMAIN_USER | 0x0F0A)

#define NVM3KEY_APP_COMMISSION_CHANNEL_MASK   (NVM3KEY_DOMAIN_USER | 0x0F0C)
#define NVM3KEY_APP_COMMISSION_EXTENDED_PAN_ID (NVM3KEY_DOMAIN_USER | 0x0F0D)
#define NVM3KEY_APP_COMMISSION_PAN_ID         (NVM3KEY_DOMAIN_USER | 0x0F0E)
#define NVM3KEY_APP_COMMISSION_LINK_KEY       (NVM3KEY_DOMAIN_USER | 0x0F0F)
#define NVM3KEY_APP_COMMISSION_STARTUP_CONTROL (NVM3KEY_DOMAIN_USER | 0x0F10)

#define NVM3KEY_APP_LIGHT_LEVEL_MAX           (NVM3KEY_DOMAIN_USER | 0x0F11)
#define NVM3KEY_APP_LIGHT_LEVEL_MID           (NVM3KEY_DOMAIN_USER | 0x0F12)
#define NVM3KEY_APP_LIGHT_LEVEL_MIN           (NVM3KEY_DOMAIN_USER | 0x0F13)
#define NVM3KEY_APP_ALLOW_LIGHT_OFF           (NVM3KEY_DOMAIN_USER | 0x0F14)

#define NVM3KEY_APP_LIGHT_LEVEL_MAX_STASHED   (NVM3KEY_DOMAIN_USER | 0x0F16)
#define NVM3KEY_APP_LIGHT_LEVEL_MID_STASHED   (NVM3KEY_DOMAIN_USER | 0x0F17)
#define NVM3KEY_APP_LIGHT_LEVEL_MIN_STASHED   (NVM3KEY_DOMAIN_USER | 0x0F18)

#define NVM3KEY_APP_WHICH_NETWORK             (NVM3KEY_DOMAIN_USER | 0x0F1A)

#define NVM3KEY_APP_UNOCCUPIED_DELAY          (NVM3KEY_DOMAIN_USER | 0x0F1C)
#define NVM3KEY_APP_UNOCCUPIED_DELAY_TO_OFF   (NVM3KEY_DOMAIN_USER | 0x0F1D)
#define NVM3KEY_APP_OCC_THRESHOLD             (NVM3KEY_DOMAIN_USER | 0x0F1E)

#define NVM3KEY_APP_ON_OFF_CYCLES             (NVM3KEY_DOMAIN_USER | 0x0F20)

#define NVM3KEY_APP_DHS_GAIN                  (NVM3KEY_DOMAIN_USER | 0x0F21)


#ifdef DEFINETYPES
 typedef struct
 {
  uint8_t enable_byte;
  uint8_t options_byte;
  uint8_t src_ep;
  uint16_t src_short_addr;
  uint16_t session_id;
  uint16_t group_id;
  uint32_t auto_time_out;
} adaptiv_commish_info_t;

typedef struct{
  uint8_t epan[8];
} tokExtendedPan_t;

typedef struct{
  uint8_t link_key[16];
} tokLinkKey_t;

#endif

 #ifdef DEFINETOKENS
 DEFINE_BASIC_TOKEN(APP_FLAGS, uint32_t, 0UL)
 DEFINE_BASIC_TOKEN(APP_OTA_DOWNLOAD_IN_PROGRESS, bool, false)
 DEFINE_BASIC_TOKEN(APP_ADAPTIV_COMMISH_INFO, adaptiv_commish_info_t, {0})
 DEFINE_BASIC_TOKEN(APP_COLOR_TEMP_K, uint16_t, DEFAULT_LIGHT_TEMPERATURE_K)   /* Not used */
 DEFINE_BASIC_TOKEN(APP_LIGHT_SENSOR_LUX, uint16_t, DEFAULT_SENSOR_LUX_TARGET)

 DEFINE_BASIC_TOKEN(APP_COMMISSION_CHANNEL_MASK, uint32_t, ((uint32_t)0xFFFF << 11))
 DEFINE_BASIC_TOKEN(APP_COMMISSION_EXTENDED_PAN_ID, tokExtendedPan_t, {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF} )
 DEFINE_BASIC_TOKEN(APP_COMMISSION_PAN_ID, uint16_t, 0xFFFF)
 DEFINE_BASIC_TOKEN(APP_COMMISSION_LINK_KEY, tokLinkKey_t, {0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF} )
 DEFINE_BASIC_TOKEN(APP_COMMISSION_STARTUP_CONTROL, uint8_t, 3)   /* 3 = associate */

 DEFINE_BASIC_TOKEN(APP_LIGHT_LEVEL_MAX, uint8_t, DEFAULT_MAX_LIGHT_LEVEL)
 DEFINE_BASIC_TOKEN(APP_LIGHT_LEVEL_MID, uint8_t, DEFAULT_MID_LIGHT_LEVEL)
 DEFINE_BASIC_TOKEN(APP_LIGHT_LEVEL_MIN, uint8_t, DEFAULT_MIN_LIGHT_LEVEL)
 DEFINE_BASIC_TOKEN(APP_LIGHT_LEVEL_MAX_STASHED, uint8_t, DEFAULT_MAX_LIGHT_LEVEL)
 DEFINE_BASIC_TOKEN(APP_LIGHT_LEVEL_MID_STASHED, uint8_t, DEFAULT_MID_LIGHT_LEVEL)
 DEFINE_BASIC_TOKEN(APP_LIGHT_LEVEL_MIN_STASHED, uint8_t, DEFAULT_MIN_LIGHT_LEVEL)
 DEFINE_BASIC_TOKEN(APP_ALLOW_LIGHT_OFF, bool, DEFAULT_ALLOW_LIGHT_OFF)
 DEFINE_BASIC_TOKEN(APP_WHICH_NETWORK, uint8_t, 0x00)   /* eNO_NWK */

 DEFINE_BASIC_TOKEN(APP_UNOCCUPIED_DELAY, uint16_t, DEFAULT_OCC_SENSOR_DELAY)
 DEFINE_BASIC_TOKEN(APP_UNOCCUPIED_DELAY_TO_OFF, uint16_t, DEFAULT_OCC_SENSOR_DELAY_TO_OFF)
 DEFINE_BASIC_TOKEN(APP_OCC_THRESHOLD, uint16_t, DEFAULT_OCC_SENSOR_THRESHOLD)

 DEFINE_BASIC_TOKEN(APP_ON_OFF_CYCLES, uint8_t, 0x00)
 DEFINE_BASIC_TOKEN(APP_DHS_GAIN, int16_t, 0x01)

 #endif

#endif  // NVM33_TEST


