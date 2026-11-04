/**
 * Broadcast Monitor
 * Estimates the current broadcast traffic level from the stack's broadcast table.
 *
 * The broadcast table is not documented by Silicon Labs.  From the stack library (broadcast.o):
 *  * sli_zigbee_broadcast_table_data[] is a circular buffer.  sli_zigbee_broadcast_head is the
 *    index of the newest entry.  A new broadcast is stored at (head + 1) % size, overwriting the
 *    oldest entry, so entries are never cleared - they stay until they are overwritten.
 *  * The entries do not store a time.  Instead, sli_zigbee_broadcast_age_cutoff_indices holds four
 *    8-bit counts of how many entries were added recently.  Adding an entry increments all four
 *    counts.  Every 5 seconds (EMBER_BROADCAST_TABLE_TIMEOUT_QS / 4), emberTick() shifts the
 *    value right 8 bits, dropping the oldest count:
 *      bits 31-24:  entries added in the last ~5 seconds
 *      bits 23-16:  entries added in the last ~10 seconds
 *      bits 15-8:   entries added in the last ~15 seconds
 *      bits 7-0:    entries added in the last ~20 seconds (the table timeout) - the live entries
 *  * The stack only uses the newest (bits 7-0) entries, walking back from the head.  Older entries
 *    have timed out, even though their data is still in the table.
 *  * The stack refuses new broadcasts (EMBER_COUNTER_BROADCAST_TABLE_FULL) when the live entries
 *    fill the table, or when the ~15 second count reaches sli_zigbee_new_broadcast_entry_threshold
 *    (table size - 6).
 * These are internal stack symbols and may change with the SDK version.
 **/
#include <stdint.h>
#include "app_logger.h"
#include "broadcast_monitor.h"
#include "time_convert.h"
#include "sl_zigbee_pro_stack_config.h"
#include "af.h"

// Percentage of the broadcast table holding live (not timed out) entries
#define _TRAFFIC_HEAVY_PERCENT      60
#define _TRAFFIC_MEDIUM_PERCENT     40

#define _UNUSED_ENTRY_SOURCE        0xFFFF      // the stack initializes unused entries to 0xFF

// Number of entries added in the last ~5, 10, 15, and 20 seconds
#define _AGE_CNT_5_SEC(cutoff_)     ((uint8_t)(((cutoff_) >> 24) & 0xFF))
#define _AGE_CNT_10_SEC(cutoff_)    ((uint8_t)(((cutoff_) >> 16) & 0xFF))
#define _AGE_CNT_15_SEC(cutoff_)    ((uint8_t)(((cutoff_) >> 8) & 0xFF))
#define _AGE_CNT_20_SEC(cutoff_)    ((uint8_t)((cutoff_) & 0xFF))

static uint8_t _get_entry_age_sec(uint32_t cutoff, uint8_t newest_order);
static broadcast_traffic_t _refresh_traffic_level(void);

extern sl_zigbee_broadcast_table_entry_t sli_zigbee_broadcast_table_data[];
extern uint8_t sli_zigbee_broadcast_head;
extern uint32_t sli_zigbee_broadcast_age_cutoff_indices;

static broadcast_traffic_t m_last_level = BROADCASTING_UNKNOWN;


broadcast_traffic_t broadcast_monitor_get_traffic(void)
{
    // The stack ages the broadcast table every emberTick(), so the level is always current
    m_last_level = _refresh_traffic_level();
    return m_last_level;
}

void broadcast_monitor_print(void)
{
    app_logger("Broadcast Table");
    app_logger("----------------------");
    app_logger("Idx  Source  Seq  Acks  Neighbor Bitmask");
    app_logger("---  ------  ---  ----  ----------------");
    for(uint8_t i=0; i<SL_ZIGBEE_BROADCAST_TABLE_SIZE; i++)
    {
        const sl_zigbee_broadcast_table_entry_t *entry =
            &sli_zigbee_broadcast_table_data[i];
            app_logger("[%u]  %02X  0x%1X    %u    0x%4X",
                    i, entry->source, entry->sequence, entry->numAcks,
                    (unsigned long)entry->neighborBitmask);
    }

    broadcast_monitor_get_traffic();
    switch(m_last_level)
    {
        case BROADCASTING_HEAVY:  app_logger("Heavy Traffic"); break;
        case BROADCASTING_MEDIUM:  app_logger("Medium Traffic"); break;
        case BROADCASTING_LIGHT:  app_logger("Light Traffic"); break;
        default: app_logger("Unknown Traffic"); break;
    }
}

static uint8_t _get_entry_age_sec(uint32_t cutoff, uint8_t newest_order)
{
    // Returns the maximum age (5, 10, 15, or 20 seconds) of the entry that is `newest_order`
    // entries back from the head (0 = newest).  Returns 0 if the entry has timed out.
    uint8_t age_sec = 0;
    if(newest_order < _AGE_CNT_5_SEC(cutoff))
    {
        age_sec = 5;
    }
    else if(newest_order < _AGE_CNT_10_SEC(cutoff))
    {
        age_sec = 10;
    }
    else if(newest_order < _AGE_CNT_15_SEC(cutoff))
    {
        age_sec = 15;
    }
    else if(newest_order < _AGE_CNT_20_SEC(cutoff))
    {
        age_sec = 20;
    }
    return age_sec;
}

static broadcast_traffic_t _refresh_traffic_level(void)
{
    // Count the broadcast table entries that have not timed out (broadcasts within the last
    // ~20 seconds), newest first, and set the traffic level from the percentage of the table used:
    //  - HEAVY:   at least _TRAFFIC_HEAVY_PERCENT
    //  - MEDIUM:  at least _TRAFFIC_MEDIUM_PERCENT
    //  - LIGHT:   less than _TRAFFIC_MEDIUM_PERCENT
    uint32_t cutoff = sli_zigbee_broadcast_age_cutoff_indices;
    uint8_t head = sli_zigbee_broadcast_head;
    uint8_t num_recent = 0;

    for(uint8_t i=0; i<SL_ZIGBEE_BROADCAST_TABLE_SIZE; i++)
    {
        if(_get_entry_age_sec(cutoff, i) == 0)
        {
            // this and all older entries have timed out
            break;
        }
        uint8_t index = (uint8_t)((head + SL_ZIGBEE_BROADCAST_TABLE_SIZE - i) % SL_ZIGBEE_BROADCAST_TABLE_SIZE);
        const sl_zigbee_broadcast_table_entry_t *p_entry = &sli_zigbee_broadcast_table_data[index];
        if(p_entry->source != _UNUSED_ENTRY_SOURCE)
        {
            num_recent++;
        }
    }

    broadcast_traffic_t traffic_level = BROADCASTING_LIGHT;
    uint16_t percent_x_size = (uint16_t)num_recent * 100;
    if(percent_x_size >= ((uint16_t)_TRAFFIC_HEAVY_PERCENT * SL_ZIGBEE_BROADCAST_TABLE_SIZE))
    {
        traffic_level = BROADCASTING_HEAVY;
    }
    else if(percent_x_size >= ((uint16_t)_TRAFFIC_MEDIUM_PERCENT * SL_ZIGBEE_BROADCAST_TABLE_SIZE))
    {
        traffic_level = BROADCASTING_MEDIUM;
    }
    return traffic_level;
}
