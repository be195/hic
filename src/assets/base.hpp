#pragma once

#include <memory>
#include <string>
#include <SDL3/SDL.h>
#include "../utils/hicapi.hpp"

namespace hic::Assets {

class HIC_API Manager;

struct HIC_API Base : std::enable_shared_from_this<Base> {
  virtual ~Base() = default;

  // called in a separate thread
  virtual void preload(Manager* manager) {}

  // called in a main thread
  virtual void use(SDL_Renderer* renderer) {}

  virtual std::string getCacheKey() const { return ""; }

  // returns nullptr if the asset does not support instancing (default).
  virtual std::shared_ptr<Base> createInstance() { return nullptr; }

  virtual void tick() {}
};

static SDL_Surface* createPlaceholderSurface();
SDL_Surface* loadSurfaceFromFile(Manager* manager, const char* fileName);

} // namespace hic::Assets
