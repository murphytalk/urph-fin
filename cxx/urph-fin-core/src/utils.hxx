#ifndef UTILS_HXX_
#define UTILS_HXX_

#if defined(GTEST_INCLUDE_GTEST_GTEST_H_) || defined(URPH_FIN_CLI) || defined(__ANDROID__)
#include <iostream>
#define LOG(x) std::cout
#else
// 3rd party
#include "aixlog.hpp"
#endif

#define NULL_STR(x) (x == nullptr ? "" : x)

#endif