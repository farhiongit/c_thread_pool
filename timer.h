// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#ifndef __TIMERS_H__
#  define __TIMERS_H__
#  include <time.h>

struct timespec delay_to_abs_timespec (double seconds);
void *timer_set (struct timespec timeout, int (*callback) (void *arg), void *arg);      // Complexity log n, where n is the number of timers previously set. 
void timer_unset (void *);      // Complexity n log n (quite slow)
#endif
