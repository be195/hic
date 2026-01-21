#pragma once

#include <memory>

#include "base.hpp"
#include "../utils/hicapi.hpp"
#include <SDL3/SDL.h>

namespace hic::Assets {

class HIC_API Image : public Base {
public:
  std::string fileName;
  int w, h;

  explicit Image(std::string fileName) : fileName(std::move(fileName)), w(0), h(0) {}
  ~Image() override;

  void preload() override;
  void use(SDL_Renderer* renderer) override;
  void draw(SDL_Renderer* renderer, float x, float y, float gw = -1, float gh = -1) const;

  std::string getCacheKey() const override { return fileName; }
private:
  SDL_Texture* texture = nullptr;
  SDL_Surface* surface = nullptr;
};

}
