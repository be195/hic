#pragma once

#include <functional>

namespace hic {
using EasingFunction = std::function<float(float)>;

namespace Easing {
  inline float linear(float t) { return t; }
  inline float easeInQuad(float t) { return t * t; }
  inline float easeOutQuad(float t) { return t * (2.0f - t); }
  inline float easeInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
  }
} // namespace Easing

} // namespace hic