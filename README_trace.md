# `trace.h` permits to instrument the code (without changing it) to trace all calls to a function.



| Include |
| - |
| <stdio.h> |


| Include |
| - |
| <threads.h> |


| Define | Value |
| - | - |
| \_\_TRACE\_\_(text) | fprintf (stderr, "[%lX:%s] %s <%s:%d>\\n", thrd\_current(), \_\_func\_\_, (text), \_\_FILE\_\_, \_\_LINE\_\_) |

## DEFINITIONS
Use `#define function(...) TRACE_EXPRESSION(function (__VA_ARGS__))` to trace all calls to `function` to the standard error stream.



| Define | Value |
| - | - |
| TRACE\_EXPRESSION(expr) | (\_\_TRACE\_\_(#expr), (expr)) |

Use `TRACE_FORMAT (fmt, args);` to log a message to the standard error stream.



| Define | Value |
| - | - |
| TRACE\_FORMAT(...) | do { fprintf (stderr, "[%lX:%s] ", thrd\_current(), \_\_func\_\_) ; fprintf (stderr, \_\_VA\_ARGS\_\_) ; fprintf (stderr, " <%s:%d>\\n", \_\_FILE\_\_, \_\_LINE\_\_); } while (0) |


## USAGE
If `function` is a function (user-defined or external), write:

	   #define function(...) TRACE_EXPRESSION(function (__VA_ARGS__))

to trace all calls to `function` to the standard error stream.


> If `function` is a user-defined function, this should be written **just after** its definition.



## EXAMPLE
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

	

-----

*This page was generated automatically from `trace.h` by `h2md`.*

-----

