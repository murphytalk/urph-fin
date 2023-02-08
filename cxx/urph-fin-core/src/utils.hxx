#ifndef UTILS_HXX_
#define UTILS_HXX_

#if defined(GTEST_INCLUDE_GTEST_GTEST_H_) || defined(__ANDROID__)

#define LOG_INFO(tag, stream_expr)
#define LOG_ERROR(tag, stream_expr)
#define LOG_DEBUG(tag, stream_expr)

#else

//#include <aws/logs/LogMacros.h>

#include <aws/core/utils/logging/LogMacros.h>

#define LOG_INFO(tag, stream_expr) AWS_LOGSTREAM_INFO(tag, stream_expr)
#define LOG_WARNING(tag, stream_expr) AWS_LOGSTREAM_WARN(tag, stream_expr)
#define LOG_ERROR(tag, stream_expr) AWS_LOGSTREAM_ERROR(tag, stream_expr)
#define LOG_DEBUG(tag, stream_expr) AWS_LOGSTREAM_DEBUG(tag, stream_expr)

#endif

#define NULL_STR(x) (x == nullptr ? "" : x)

#endif