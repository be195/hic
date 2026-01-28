#include <SDL3/SDL.h>
#include "base.hpp"
#include "image.hpp"
#include "../utils/util.hpp"
#include "../utils/logging.hpp"

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
  if (!surface) {
    HICL("Image").warn("surface loading failed in a background thread");
    return;
  }

  texture = SDL_CreateTextureFromSurface(renderer, surface);
  assertNotNull(texture, SDL_GetError());

  w = surface->w; h = surface->h;
  setScaleMode(SDL_SCALEMODE_NEAREST);

  SDL_DestroySurface(surface);
  surface = nullptr;
}

void Image::render(SDL_Renderer* renderer, const float x, const float y, const float gw, const float gh) const {
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

void Image::setScaleMode(const SDL_ScaleMode scaleMode) const {
  if (texture)
    SDL_SetTextureScaleMode(texture, scaleMode);
}

SDL_ScaleMode Image::getScaleMode() const {
  if (texture) {
    SDL_ScaleMode scaleMode = SDL_SCALEMODE_INVALID;
    SDL_GetTextureScaleMode(texture, &scaleMode);
    return scaleMode;
  }

  return SDL_SCALEMODE_INVALID;
}

}
