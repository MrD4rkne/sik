#ifndef MIM_ERR_H
#define MIM_ERR_H

#include <stdexcept>
#include <string>

// Print information about a system error and quits.
void syserr(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Print information about an error and quits.
void fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Print information about an error and return.
void error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
