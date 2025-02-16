// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#ifndef __TIMERS_H__
#  define __TIMERS_H__
#  include <time.h>

struct timespec delay_to_abs_timespec (double seconds);
// is a helper function to convert a delay in seconds relative to the current time on the timer's clock at the time of the call into an absolute time.

void *timer_set (struct timespec timeout, int (*callback) (void *arg), void *arg);
// creates and starts a timer. When the absolute time `timeout` is reached, the callback function `callback` is called with `arg` passed as argument.
// - Returns a timer id that can be passed to `timer_unset` to cancel a timer.
// - Complexity log n, where n is the number of timers previously set.

void timer_unset (void *);
// cancels a previously set timer.
// - Complexity n log n (quite slow)
#endif
