#pragma once

#include "LogUtil.h"

extern CLogUtil* g_dllLog;

#define LOG_DEBUG(...) {if (g_dllLog!=nullptr){g_dllLog->Log(__FILE__,__LINE__, ELogLevel::LOG_LEVEL_DEBUG, __VA_ARGS__);}}
#define LOG_INFO(...) {if (g_dllLog!=nullptr){g_dllLog->Log(__FILE__,__LINE__, ELogLevel::LOG_LEVEL_INFO, __VA_ARGS__);}}
#define LOG_ERROR(...) {if (g_dllLog!=nullptr){g_dllLog->Log(__FILE__,__LINE__, ELogLevel::LOG_LEVEL_ERROR, __VA_ARGS__);}}