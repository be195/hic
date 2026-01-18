#pragma once

#include <SDL3/SDL.h>

namespace hic::Assets {

struct Base {
public:
  virtual ~Base() = default;

  virtual void preload() {}
  virtual void use(SDL_Renderer* renderer) {}
};

} // namespace hic::Assets
