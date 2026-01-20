#pragma once

#include <functional>
#include "hicapi.hpp"

namespace hic {
using EasingFunction = std::function<float(float)>;

namespace Easing {
  inline float HIC_API linear(float t) { return t; }
  inline float HIC_API easeInQuad(float t) { return t * t; }
  inline float HIC_API easeOutQuad(float t) { return t * (2.0f - t); }
  inline float HIC_API easeInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
  }
} // namespace Easing

} // namespace hic