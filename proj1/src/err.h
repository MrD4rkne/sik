#ifndef MIM_ERR_H
#define MIM_ERR_H

#include <stdexcept>
#include <string>

// Print information about a system error and quits.
void syserr(const char* fmt, ...);

// Print information about an error and quits.
void fatal(const char* fmt, ...);

// Print information about an error and return.
void error(const char* fmt, ...);

#endif
