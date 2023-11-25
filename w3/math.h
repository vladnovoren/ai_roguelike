#pragma once

#include <cmath>
#include <random>

template<typename T>
inline T sqr(T a){ return a*a; }

template<typename T, typename U>
inline float dist_sq(const T &lhs, const U &rhs) { return float(sqr(lhs.x - rhs.x) + sqr(lhs.y - rhs.y)); }

template<typename T, typename U>
inline float dist(const T &lhs, const U &rhs) { return sqrtf(dist_sq(lhs, rhs)); }

static std::random_device kRandomDevice;
static std::default_random_engine kRandomEngine(kRandomDevice());

inline float get_random_float(float min, float max) {
  std::uniform_real_distribution<float> range(min, max);
  return range(kRandomEngine);
}