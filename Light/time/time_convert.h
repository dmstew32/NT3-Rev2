#ifndef _TIME_CONVERT_H_
#define _TIME_CONVERT_H_

#define MINUTES_TO_MS(mins_) (SECONDS_TO_MS((mins_) * 60))
#define MINUTES_TO_SEC(mins_) (mins_ * 60)
#define SECONDS_TO_MS(sec_)  ((sec_) * 1000)
#define SECONDS_TO_TENTHS_SEC(sec_)  ((sec_) * 10)

#define MS_TO_SECONDS(ms_)   ((ms_) / 1000)

#endif // #ifndef _TIME_CONVERT_H_
