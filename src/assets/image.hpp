#pragma once

#include <memory>
#include "base.hpp"
#include "../utils/hicapi.hpp"
#include <SDL3/SDL.h>

namespace hic::Assets {

class HIC_API Image : public Base {
public:
  std::string fileName;
  SDL_Texture* texture = nullptr;
  int w, h;

  explicit Image(std::string fileName) : fileName(std::move(fileName)), w(0), h(0) {}
  ~Image() override;

  void preload(Manager* manager) override;
  void use(SDL_Renderer* renderer) override;
  void render(SDL_Renderer* renderer, float x, float y, float gw = -1, float gh = -1) const;
  void setScaleMode(SDL_ScaleMode scaleMode) const;
  SDL_ScaleMode getScaleMode() const;

  std::string getCacheKey() const override { return "i#" + fileName; }
private:
  SDL_Surface* surface = nullptr;
};

}
