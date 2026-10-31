#ifndef _APP_LOGGER_H_
#define _APP_LOGGER_H_

#include <stdint.h>
#include <stdbool.h>

void app_logger(const char *p_format_str, ...);
void app_logger_nocrlf(bool timestamp, const char *p_format_str, ...);
void app_logger_buffer(char *p_title, uint8_t *p_buffer, uint8_t len);

#endif  // #ifndef _APP_LOGGER_H_
