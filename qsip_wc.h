//=========================================
// Multi-threaded Quick sort in place
// (c) L. Farhi, 2024
// Language: C
#ifndef __QSIP_H__
#  define __QSIP_H__
#  include <stddef.h>

// Multi-threaded Quick sort in place (does not allocate extra memory to sort array base).
// The qsip() function sorts an array with nmemb elements of size size. The base argument points to the start of the array.
// The contents of the array are sorted in ascending order according to a lesser-than function pointed to by lt:
//   - lt is called with two arguments that point to the objects being compared, and a third argument, a nullable pointer passed to the lesser-than function via arg.
//   - lt must return an integer greater than zero if the first argument is considered to be less than the second, 0 otherwise.
// If two members compare equal, their order in the sorted array is not preserved.
int qsip (void *base, size_t nmemb, size_t size, int (*lt) (const void *, const void *, void *), void *arg);

// For example, to sort strings, the lesser-than function can call strcmp(3) or strcoll(3):
//   static int lts (const void *a, const void *b, void *arg)
//   {
//     return strcmp (*(const char **) a, *(const char **) b) < 0;
//   }
// qsip() returns EXIT_SUCCESS on success, EXIT_FAILURE otherwise (in case of invalid arguments, with errno being set to EINVAL).
#endif
