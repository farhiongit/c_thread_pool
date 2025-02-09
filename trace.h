// # DEFINITION
#ifndef __TRACE__
#  define __TRACE__
#  include <stdio.h>
#  include <threads.h>
#  define TRACE(text)               fprintf (stderr, "[%lX:%s] %s <%s:%d>\n", thrd_current(), __func__, (text), __FILE__, __LINE__)
#  define TRACE_EXPRESSION(expr)    (TRACE(#expr), (expr))
#  define TRACE_FORMAT(...)         do { fprintf (stderr, "[%lX:%s] ", thrd_current(), __func__) ; fprintf (stderr, __VA_ARGS__) ; fprintf (stderr, " <%s:%d>\n", __FILE__, __LINE__); } while (0)

/* # USAGE
 If my_function is a function, write:
 #define my_function(...) TRACE_EXPRESSION(my_function (__VA_ARGS__))
 to trace all calls to my_function.
 If my_function is a user-defined function, this should be written just after the definition of the function.
 For instance, the following program:

    #include <unistd.h>
    #include "trace.h"
    #define sleep(...) TRACE_EXPRESSION(sleep (__VA_ARGS__))

    static
    void f1 (unsigned int a)
    {
      TRACE_FORMAT ("Enter with %d", a);
      sleep (a);
      fprintf (stdout, "%d\n", a);
      TRACE_FORMAT ("Exit with %d", a);
    }
    #define f1(...) TRACE_EXPRESSION(f1 (__VA_ARGS__))

    static
    void f2 (void)
    {
      TRACE_FORMAT ("Enter");
      f1 (2);
      TRACE_FORMAT ("Exit");
    }
    #define f2(...) TRACE_EXPRESSION(f2 (__VA_ARGS__))

    int main (void)
    {
      unsigned int a, b;
      (a=1,
    #define a TRACE_EXPRESSION(a)
      f1 (a));
      f2 ();
      b = a;
    }

will yield:

    [79DEB50B8740:main] f1 ((fprintf (stderr, "[%lX:%s] %s <%s:%d>\n", thrd_current(), __func__, ("a"), "test_trace.c", 29), (a))) <test_trace.c:29>
    [79DEB50B8740:main] a <test_trace.c:29>
    [79DEB50B8740:f1] Enter with 1 <test_trace.c:8>
    [79DEB50B8740:f1] sleep (a) <test_trace.c:9>
    1
    [79DEB50B8740:f1] Exit with 1 <test_trace.c:11>
    [79DEB50B8740:main] f2 () <test_trace.c:30>
    [79DEB50B8740:f2] Enter <test_trace.c:18>
    [79DEB50B8740:f2] f1 (2) <test_trace.c:19>
    [79DEB50B8740:f1] Enter with 2 <test_trace.c:8>
    [79DEB50B8740:f1] sleep (a) <test_trace.c:9>
    2
    [79DEB50B8740:f1] Exit with 2 <test_trace.c:11>
    [79DEB50B8740:f2] Exit <test_trace.c:20>
    [79DEB50B8740:main] a <test_trace.c:31>

 */
#endif
