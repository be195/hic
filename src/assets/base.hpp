#pragma once

#include <string>
#include <SDL3/SDL.h>
#include "../utils/hicapi.hpp"

namespace hic::Assets {

struct HIC_API Base {
  virtual ~Base() = default;

  // called in a separate thread
  virtual void preload() {}

  // called in a main thread
  virtual void use(SDL_Renderer* renderer) {}

  virtual std::string getCacheKey() const { return ""; }

  virtual void tick() {}
};

static SDL_Surface* createPlaceholderSurface();
SDL_Surface* loadSurfaceFromFile(const char* fileName);

} // namespace hic::Assets
