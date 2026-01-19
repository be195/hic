#pragma once

#include <string>
#include <SDL3/SDL.h>

namespace hic::Assets {

struct Base {
public:
  virtual ~Base() = default;

  // called in a separate thread
  virtual void preload() {}

  // called in a main thread
  virtual void use(SDL_Renderer* renderer) {}

  virtual std::string getCacheKey() const { return ""; }
};

} // namespace hic::Assets
