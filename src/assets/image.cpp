#include <SDL3/SDL.h>
#include "base.cpp"
#include "image.hpp"
#include "../utils/util.hpp"

namespace hic::Assets {

Image::~Image() {
  if (texture) {
    SDL_DestroyTexture(texture);
    texture = nullptr;
  }
}

void Image::preload() {
  if (texture)
    SDL_DestroyTexture(texture);

  surface = loadSurfaceFromFile(fileName.c_str());
}

void Image::use(SDL_Renderer* renderer) {
  assertNotNull(surface, "surface loading failed in a background thread");

  texture = SDL_CreateTextureFromSurface(renderer, surface);
  assertNotNull(texture, SDL_GetError());

  SDL_DestroySurface(surface);
  surface = nullptr;
}

void Image::draw(SDL_Renderer* renderer, const float x, const float y, const float gw, const float gh) const {
  if (!texture) return;

  SDL_FRect rect;
  rect.x = x;
  rect.y = y;

  if (gw > 0 && gh > 0) {
    rect.w = gw;
    rect.h = gh;
  } else {
    rect.w = w;
    rect.h = h;
  }

  SDL_RenderTexture(renderer, texture, nullptr, &rect);
}

}