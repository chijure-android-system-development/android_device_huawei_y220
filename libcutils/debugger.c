#include <sys/types.h>

/*
 * ICS asks debuggerd for the backtrace of another thread. The GB debuggerd
 * cannot do that, so report failure. MediaTek's libaed only needs the symbol.
 */
int dump_backtrace_to_file(pid_t tid, int fd)
{
    (void) tid;
    (void) fd;
    return -1;
}
