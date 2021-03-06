#ifndef UTILS_HXX_
#define UTILS_HXX_

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <iostream>
#define LOG(x) std::cout
#else
// 3rd party
#include "aixlog.hpp"
#endif

#define NULL_STR(x) (x == nullptr ? "" : x)

#endif