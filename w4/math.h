#pragma once

#include <cmath>

template<typename T>
inline T sqr(T a){ return a*a; }

template<typename T, typename U>
inline float dist_sq(const T &lhs, const U &rhs) { return float(sqr(lhs.x - rhs.x) + sqr(lhs.y - rhs.y)); }

template<typename T, typename U>
inline float dist(const T &lhs, const U &rhs) { return sqrtf(dist_sq(lhs, rhs)); }

template <typename T>
inline int signum(T value) {
  if (value < 0)
    return -1;
  if (value == 0)
    return 0;

  return 1;
}

template <typename T>
inline T L1_dist(T x1, T y1, T x2, T y2) {
  return abs(x1 - x2) + abs(y1 - y2);
}