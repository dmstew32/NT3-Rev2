#include "af.h"
#include "app_logger.h"
#include "uptime.h"

static void _print_timestamp(void);

static char m_print_buffer[100];

void app_logger(const char *p_format_str, ...)
{
    _print_timestamp();

    va_list ap;
    va_start(ap, p_format_str);
    vsnprintf(m_print_buffer, sizeof(m_print_buffer), p_format_str, ap);
    va_end(ap);

    // now print the formatted string
    sl_zigbee_af_core_println("%s", m_print_buffer);
}

void app_logger_nocrlf(bool timestamp, const char *p_format_str, ...)
{
    if(timestamp == true)
    {
        _print_timestamp();
    }
    va_list ap;
    va_start(ap, p_format_str);
    vsnprintf(m_print_buffer, sizeof(m_print_buffer), p_format_str, ap);
    va_end(ap);

    // now print the formatted string
    sl_zigbee_af_core_print("%s", m_print_buffer);
}

void app_logger_buffer(char *p_title, uint8_t *p_buffer, uint8_t len)
{
    if(p_title != NULL)
    {
        sl_zigbee_af_core_print("%s", p_title);
    }
    for(uint8_t i=0; i<len; i++)
    {
        sl_zigbee_af_core_print("%02x ", p_buffer[i]);
    }
    sl_zigbee_af_core_print("\r\n");
}


static void _print_timestamp(void)
{
    uint32_t uptime_sec = uptime_get_seconds();
    uint32_t ms = (uptime_get_ms32() % 1000);

    uint32_t mins = (uptime_sec / 60);
    uint32_t secs = (uptime_sec % 60);
    sl_zigbee_af_core_print("[%d:%02d.%03d] ", mins, secs, ms);
}
