#pragma once
#include <whb/log.h>
#include <whb/log_udp.h>
#include <cstdio>

#define log_printf(fmt, ...) WHBLogPrintf(fmt, ##__VA_ARGS__)

static inline void log_init()   { WHBLogUdpInit(); }
static inline void log_deinit() { WHBLogUdpDeinit(); }
