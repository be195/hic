#include <SDL3/SDL.h>
#include "base.hpp"

namespace hic::Assets {

static SDL_Surface* createPlaceholderSurface() {
  constexpr int size = 64;

  SDL_Surface* surface = SDL_CreateSurface(
    size,
    size,
    SDL_PIXELFORMAT_RGBA32
  );

  if (!surface) return nullptr;

  const auto pixelFmtDetails = SDL_GetPixelFormatDetails(surface->format);
  const Uint32 purple = SDL_MapRGBA(pixelFmtDetails, nullptr, 255, 0, 255, 255);
  const Uint32 black = SDL_MapRGBA(pixelFmtDetails, nullptr, 0, 0, 0, 255);

  const auto pixels = static_cast<Uint32*>(surface->pixels);

  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      constexpr int tile = 8;
      const bool checker =
          ((x / tile) % 2) ^
          ((y / tile) % 2);

      pixels[y * size + x] = checker ? purple : black;
    }
  }

  return surface;
}

SDL_Surface* loadSurfaceFromFile(const char* fileName) {
  const auto surface = SDL_LoadPNG(fileName);
  if (!surface)
    return createPlaceholderSurface();

  return surface;
}

} // namespace hic::Assets
