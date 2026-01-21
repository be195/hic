#pragma once

#include "../utils/hicapi.hpp"

namespace hic::Audio {

class HIC_API BaseEffect {
public:
  virtual ~BaseEffect() = default;

  virtual void apply(float* samples, int frames);
};

}
