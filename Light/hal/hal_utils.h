#ifndef HAL_UTILS_H_
#define HAL_UTILS_H_


#define TP_0_PORT   gpioPortD
#define TP_0_PIN    0

#define TP_1_PORT   gpioPortC
#define TP_1_PIN    2

#define TP_2_PORT   gpioPortC
#define TP_2_PIN    3

#define TP_3_PORT   gpioPortD
#define TP_3_PIN    4

void set_TP_0(void);
void clear_TP_0(void);
void toggle_TP_0(void);

void set_TP_1(void);
void clear_TP_1(void);
void toggle_TP_1(void);

void set_TP_2(void);
void clear_TP_2(void);
void toggle_TP_2(void);

void set_TP_3(void);
void clear_TP_3(void);
void toggle_TP_3(void);

void hal_test_pts_init(void);

void hal_timer1_init(void);
uint32_t get_time_ms(void);
bool has_timed_out(uint32_t timeStamp, uint32_t delay);

int8_t set_one_shot_us_timer_isr(void (*func)(void), uint32_t delay_us);
bool disable_one_shot_us_timer_isr(int8_t int_num);

#define ZCL_CMD_ID_AUTO_COMMISSIONING_RESET 0x00

#endif /* HAL_UTILS_H_ */
