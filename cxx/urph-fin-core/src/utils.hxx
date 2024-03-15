#ifndef UTILS_HXX_
#define UTILS_HXX_

//#if defined(GTEST_INCLUDE_GTEST_GTEST_H_) || defined(__ANDROID__) || defined(NO_LOG)
#if defined (AWS)

#include <aws/core/utils/logging/LogMacros.h>
#define LINFO(eam_expr) AWS_LOGSTREAM_INFO(tag, stream_expr)
#define LWARN(eam_expr) AWS_LOGSTREAM_WARN(tag, stream_expr)
#define LERROR(eam_expr) AWS_LOGSTREAM_ERROR(tag, stream_expr)
#define LDEBUG(eam_expr) AWS_LOGSTREAM_DEBUG(tag, stream_expr)

#elif defined (AIX_LOG)

#include <aixlog.hpp>
#include <thread>
#include <iostream>

#define LINFO(eam_expr)  LOG(INFO)  << "[" << tag << "] " << "[" << std::hex << std::this_thread::get_id() << "] " << std::dec << stream_expr << "\n"
#define LWARN(eam_expr)  LOG(WARN)  << "[" << tag << "] " << "[" << std::hex << std::this_thread::get_id() << "] " << std::dec << stream_expr << "\n"
#define LERROR(eam_expr) LOG(ERROR) << "[" << tag << "] " << "[" << std::hex << std::this_thread::get_id() << "] " << std::dec << stream_expr << "\n"
#define LDEBUG(eam_expr) LOG(DEBUG) << "[" << tag << "] " << "[" << std::hex << std::this_thread::get_id() << "] " << std::dec << stream_expr << "\n"

#elif defined (P_LOG)

#include <plog/Log.h> 
#include "plog/Initializers/RollingFileInitializer.h"

#define LINFO(stream_expr)  PLOGI  << stream_expr
#define LWARN(stream_expr)  PLOGW  << stream_expr 
#define LERROR(stream_expr) PLOGE  << stream_expr 
#define LDEBUG(stream_expr) PLOGD  << stream_expr 

#else

#define LINFO(eam_expr)
#define LERROR(eam_expr)
#define LDEBUG(eam_expr)


#endif

#define NULL_STR(x) (x == nullptr ? "" : x)


#endif