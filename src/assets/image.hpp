#pragma once

#include "base.hpp"
#include <SDL3/SDL.h>

namespace hic::Assets {

class Image : public Base {
public:
  const char* fileName;
  int w, h;

  explicit Image(const char* fileName) : fileName(fileName), w(0), h(0) {}
  ~Image() override;

  void use(SDL_Renderer* renderer) override;
  void draw(SDL_Renderer* renderer, float x, float y, float gw = -1, float gh = -1) const;

  std::string getCacheKey() const override { return fileName; }
private:
  SDL_Texture* texture = nullptr;
};

}
