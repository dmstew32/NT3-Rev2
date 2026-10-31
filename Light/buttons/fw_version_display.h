#ifndef FW_VERSION_DISPLAY_H_
#define FW_VERSION_DISPLAY_H_

// Blinks the firmware version: major on red, middle on green, minor on red.
// The light, occupancy sensor and light sensor are paused while it runs.
#ifdef ALLOW_DISPLAY_VERSION
void start_display_fw_version(void);
#endif

#endif /* FW_VERSION_DISPLAY_H_ */
