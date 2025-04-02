#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "err.h"

void syserr(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    std::fprintf(stderr, "\tERROR: ");

    va_start(fmt_args, fmt);
    std::vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    std::fprintf(stderr, " (%d; %s)\n", org_errno, std::strerror(org_errno));
    std::exit(1);
}

void fatal(const char* fmt, ...) {
    va_list fmt_args;

    std::fprintf(stderr, "\tERROR: ");

    va_start(fmt_args, fmt);
    std::vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    std::fprintf(stderr, "\n");
    std::exit(1);
}

void error(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    std::fprintf(stderr, "\tERROR: ");

    va_start(fmt_args, fmt);
    std::vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    if (org_errno != 0) {
        std::fprintf(stderr, " (%d; %s)", org_errno, std::strerror(org_errno));
    }
    std::fprintf(stderr, "\n");
}
