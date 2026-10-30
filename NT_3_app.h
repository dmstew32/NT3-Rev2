#ifndef _NT_3_APP_H_
#define _NT_3_APP_H_

/****************       FIRMWARE VERSION       ****************/
// NOTE: Check Debug_PRINT below
#define APP_FIRMWARE_VERSION_U32 0x00010104 /* OTA Version */
#define APP_FIRMWARE_VERSION "010104"       /* Attribute version */
#define APP_DATE_VERSION "26.13.06"


#define LIGHT_ENDPOINT  (1)
#define OCC_SENSOR_ENDPOINT  (2)
#define LIGHT_SENSOR_ENDPOINT  (3)

#define ADAPTIV_ZIGBEE_MFG_ID   (0x1574)
#define DAINTREE_ZIGBEE_MFG_ID  (0x103A)

#define ZCL_ADAPTIV_AUTOCOMMISSIONING_CLUSTER_ID     (0xFCAC)
#define ZCL_ADAPTIV_COMMISH_CMD_ANNOUNCE_GROUP       (0x01)
#define ZCL_ADAPTIV_COMMISH_CMD_SET_MANUAL_ADD_MODE  (0x03)
#define ZCL_ADAPTIV_COMMISH_CMD_ID_FIRMWARE_VER      (0x10)

#define ZCL_CONFIGURATION_CLUSTER_ID  (0xFC01)
#define ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID (0xFCAD)
// Adaptive commands
#define ZCL_ADAPTIV_DEBUG_CLUSTER_ID          (0xFCDB)
#define ZCL_ADAPTIV_DEBUG_COMMAND_ID          (0xDB)
#define ZCL_SUSPEND_LIGHT_SENSOR_COMMAND_ID   (0xDC)
#define ZCL_WRITE_LIGHT_SENSOR_COMMAND_ID     (0xDD)
#define ZCL_WRITE_OCCUPANCY_SENSOR_COMMAND_ID (0xDE)

// Attributes in zap-id.h
#define MAX_ALLOWABLE_LIGHT_LEVEL  (254)       /* 100% */
#define DEFAULT_MAX_LIGHT_LEVEL    (204)       /* 80% */
#define DEFAULT_MID_LIGHT_LEVEL    (51)        /* 20% */
#define DEFAULT_MIN_LIGHT_LEVEL    (0)
#define DEFAULT_MIN_LIGHT_LEVEL_DT (25)        /* 10% per Kimin */
#define MIN_ALLOWABLE_LIGHT_LEVEL  (25)        /* 10% per Kimin */
#define DEFAULT_LIGHT_TEMPERATURE_K (4000)     /* New, just joined */


#define DEFAULT_OCC_SENSOR_DELAY        (30*60)     /* 30 minutes */
#define DEFAULT_OCC_SENSOR_DELAY_TO_OFF (5*60)      /* 5 minutes */
#define DEFAULT_OCC_SENSOR_DELAY_TO_OCC (0)         /* 0 seconds unocc -> occ */
#define DEFAULT_OCC_SENSOR_XXL_DELAY    (2UL*60UL*60UL) /* 2 hours */
#define DEFAULT_OCC_SENSOR_XXXL_DELAY   (3UL*60UL*60UL) /* 3 hours occ sensor*/
#define DEFAULT_OCC_SENSOR_THRESHOLD    (80)        /* Trip point */
#define DEFAULT_OCC_SENSOR_THRESHOLD_LOW (60)

#define DEFAULT_SENSOR_LUX_TARGET  (0)         /* DHS Off */

#define DEFAULT_ALLOW_LIGHT_OFF    (1)

#define DEFAULT_RADIO_PWR (3)

/*** Dsecs, in english, for delays ***/
#define DSECS_0_SECS      (0)
#define DSECS_HALF_SECS   (5)
#define DSECS_1_SECS      (10)
#define DSECS_2_SECS      (20)
#define DSECS_3_SECS      (30)
#define DSECS_4_SECS      (40)
#define DSECS_5_SECS      (50)

/*** Bits for TOKEN_APP_FLAGS ***/
#define B_50_PERCENT_MODE       (1UL << 0UL)
#define B_MANUAL_ON_MODE        (1UL << 1UL)
#define B_DISABLE_OCC_SENSOR    (1UL << 2UL)
#define B_DISABLE_LIGHT_SENSOR  (1UL << 3UL)

#define B_SWITCH_NWK            (1UL << 4UL)
#define B_MANUAL_ADD_MODE       (1UL << 5UL)
#define B_OCC_XXL_DELAY_ENABLED (1UL << 6UL)
#define B_EM_MODE               (1UL << 7UL)

/* DHS on boundary */
#define B_DHS_MASTER_LIGHT      (1UL << 8UL)
#define B_DHS_CALIBRATING       (1UL << 9UL)
#define B_DHS_CALIBRATED        (1UL << 10UL)
#define B_DHS_HIGH_LUX          (1UL << 11UL)

#define B_OKELLEY_FOLLY_1       (1UL << 12UL)

// Bit test for byte, after S_DHS_MODE_INFO
#define SH_DHS_MODE_INFO        (8)
#define BS_DHS_MASTER_LIGHT     (1 << 0)
#define BS_DHS_CALIBRATING      (1 << 1)
#define BS_DHS_CALIBRATED       (1 << 2)
#define BS_DHS_HIGH_LUX         (1 << 3)

//#define TC_LINK_KEY_FIX
#define NO_LEAVE_ON_TC_LINK_FAILURE

#define MODIFY_JOIN_PROCESS           /* Skip some steps */

#define N_SENSOR_NEIGHBOR_TABLE_SIZE  (16)

#define DISCO_POLL_TIME            (5000UL)
#define DISCO_POLL_TIME_DAINTREE   (30000UL)

/*** MACROS **/
// Little endian
#define GET_8_FROM_ARRAY(array, i) ((uint8_t)(array[(i)]))

#define GET_16_FROM_ARRAY(array, i) (((uint16_t)array[(i)+1] << 8) |  (uint16_t)(array[(i)]))

#define GET_32_FROM_ARRAY(array, i) (((uint32_t)array[(i)+3] << 24) |  ((uint32_t)(array[(i)+2]) << 16 ) | \
                                  ((uint32_t)array[(i)+1] << 8) |  (uint32_t)(array[(i)]))

#define GET_LSB_FROM_16(var16) (uint8_t)(0x00FF & var16)
#define GET_MSB_FROM_16(var16) (uint8_t)(0x00FF & (var16 >> 8))

#define BYTE0_FROM_32(var32)   (uint8_t)(0x00FFUL & var32)
#define BYTE1_FROM_32(var32)   (uint8_t)(0x00FFUL & (var32 >> 8))
#define BYTE2_FROM_32(var32)   (uint8_t)(0x00FFUL & (var32 >> 16))
#define BYTE3_FROM_32(var32)   (uint8_t)(0x00FFUL & (var32 >> 24))

#define ABS(x) ((x) < 0 ? -(x) : (x))

#define SIGNUM(x) ((0 < (x)) - ((x) < 0))

/***  DEBUG  ***/
//#define DHS_SINK_DEVICE               /* Make one node sink DHS info */

// Op-codes for sink/source
#define SINK_SOURCE_DISABLE  (0x10)
#define SINK_SOURCE_ENABLE   (0x11)
#define SINK_SHORT_ADDR      (0xAA)
#define DHS_DATA             (0xD1)      /* Daylight harvester data */
#define DHS_CAL_DATA         (0xDC)
#define DHS_MST_DATA         (0xDE)

//#define DISABLE_LIGHT_SENSOR_CONTROL     /* Report lux but no adjustment of lights */
#define ALLOW_DISPLAY_VERSION
#define NEW_DHS

// These should be commented out for releases
// and always non-zero levels, 1, 2,..
//#define BUTTON_PRINT         (1)
//#define OCC_PRINT            (1)
//#define LEVEL_PRINT          (1)
//#define LIGHT_SENSOR_PRINT   (1)
//#define HAL_LIGHT_SENSOR_PRINT (1)

typedef enum
{
  eNO_NWK = 0,   // Not joined
  eADAPTIV_NWK,
  eDAINTREE_NWK,
  eUNKNOWN_NWK,  // Joined but don't know who
  eSTAND_ALONE   // Not(Never?) joined for x time
} which_nwk_t;

typedef enum {
  OTA_IMAGE_TYPE_ID_LIGHT = 0x0002,

  OTA_IMAGE_TYPE_ID_NONE = 0xFFFF
} adaptiv_af_ota_image_id_t;

#endif  // _NT_3_APP_H_
