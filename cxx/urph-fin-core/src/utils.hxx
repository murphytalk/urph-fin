#ifndef UTILS_HXX_
#define UTILS_HXX_

//#if defined(GTEST_INCLUDE_GTEST_GTEST_H_) || defined(__ANDROID__) || defined(NO_LOG)
#if defined (AWS)

#include <aws/core/utils/logging/LogMacros.h>
#define LINFO(tag, stream_expr) AWS_LOGSTREAM_INFO(tag, stream_expr)
#define LWARN(tag, stream_expr) AWS_LOGSTREAM_WARN(tag, stream_expr)
#define LERROR(tag, stream_expr) AWS_LOGSTREAM_ERROR(tag, stream_expr)
#define LDEBUG(tag, stream_expr) AWS_LOGSTREAM_DEBUG(tag, stream_expr)

#elif defined (AIX_LOG)

#include <aixlog.hpp>

#define LINFO(tag, stream_expr)  LOG(INFO)  << stream_expr << "\n";
#define LWARN(tag, stream_expr)  LOG(WARN)  << stream_expr << "\n";
#define LERROR(tag, stream_expr) LOG(ERROR) << stream_expr << "\n";
#define LDEBUG(tag, stream_expr) LOG(DEBUG) << stream_expr << "\n";

#else

#define LINFO(tag, stream_expr)
#define LERROR(tag, stream_expr)
#define LDEBUG(tag, stream_expr)


#endif



#define NULL_STR(x) (x == nullptr ? "" : x)

#endif