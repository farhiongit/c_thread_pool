#ifndef __TRACE__
#  define __TRACE__
#  include <stdio.h>
#  include <threads.h>
#  define TRACE(text) fprintf (stderr, "[%lX:%s] %s <%s:%d>\n", thrd_current(), __func__, (text), __FILE__, __LINE__)
#  define TRACE_EXPRESSION(expr) (TRACE(#expr), (expr))
#  define TRACE_STATEMENT(stmt) do { TRACE(#stmt); stmt;} while (0)
#  define TRACE_HERE(...) do { TRACE_STATEMENT() ; fprintf (stderr, __VA_ARGS__) ; } while (0)
#endif
