#ifndef TIME_MONITOR_H
#define TIME_MONITOR_H

#include <stdint.h>

typedef struct {
    double current_us;
    double min_us;
    double max_us;
    double avg_us;
    double total_us;
    double last_avg_us;
    int count;
} TimingStats;

void time_monitor_init(void);
void time_monitor_start(void);
void time_monitor_stop(void);
void time_monitor_finalize(void);
TimingStats time_monitor_get_stats(void);
void time_monitor_print_summary(void);
void time_monitor_print_trend_graph(void);

#endif
