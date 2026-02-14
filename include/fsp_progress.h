#pragma once



static inline double
timespec_diff_sec(struct timespec a, struct timespec b)
{
    return (double)(a.tv_sec - b.tv_sec) +
           (double)(a.tv_nsec - b.tv_nsec) * 1e-9;
}


#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"

#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_MAGENTA "\033[35m"


