#include "bitmap.hpp"
#include <algorithm>
#include <ranges>
#include "../utils/logging.hpp"
#include "../utils/util.hpp"
#include "../utils/utf8.hpp"
#include "base.hpp"
#include "manager.hpp"

namespace hic::Assets {

void BitmapFont::cleanup() {
  HICL("BitmapFont").debug("cleanup()");

  for (const auto texture: textures | std::views::values) {
    HICL("BitmapFont").debug("destroying texture", texture);
    if (texture) SDL_DestroyTexture(texture);
  }

  for (const auto surface: surfaces | std::views::values) {
    HICL("BitmapFont").debug("destroying surface", surface);
    if (surface) SDL_DestroySurface(surface);
  }

  textures.clear();
  surfaces.clear();
  ok = false;
}

BitmapFont::BitmapFont(std::string folderName) : folderName(std::move(folderName)) {
  mutex = SDL_CreateMutex();
}

BitmapFont::~BitmapFont() {
  if (mutex) {
    SDL_LockMutex(mutex);
    cleanup();
    SDL_UnlockMutex(mutex);
    SDL_DestroyMutex(mutex);
  }
}

void BitmapFont::preload(Manager* manager) {
  SDL_LockMutex(mutex);
  cleanup();

  font = std::make_unique<BMFont>();
  std::string fntPath = manager->resolve("fonts/" + folderName + "/" + folderName + ".fnt");
  ok = font->tryLoad(fntPath.c_str());
  if (!ok) {
    HICL("BitmapFont").error("failed to load font binary data:", fntPath);
    SDL_UnlockMutex(mutex);
    return;
  }

  for (const auto& page : font->pages) {
    std::string pagePath = manager->resolve("fonts/" + folderName + "/" + page);
    HICL("BitmapFont").debug("attempting to load page", pagePath);

    const auto surface = loadSurfaceFromFile(manager, pagePath.c_str());
    if (surface)
      surfaces.insert({page, surface});
    else {
      HICL("BitmapFont").warn("failed to load font page", page, folderName);
      cleanup();
      SDL_UnlockMutex(mutex);
      return;
    }
  }

  ok = !surfaces.empty();
  SDL_UnlockMutex(mutex);
}

void BitmapFont::use(SDL_Renderer *renderer) {
  SDL_LockMutex(mutex);
  if (!ok) {
    HICL("BitmapFont").error("cannot use font:", folderName);
    SDL_UnlockMutex(mutex);
    return;
  }

  for (const auto& [page, surface] : surfaces) {
    const auto texture = SDL_CreateTextureFromSurface(renderer, surface);

    if (!texture) {
      HICL("BitmapFont").error("failed to create texture:", SDL_GetError());

      for (const auto tex : textures | std::views::values)
        SDL_DestroyTexture(tex);
      textures.clear();

      ok = false;
      SDL_UnlockMutex(mutex);
      return;
    }

    textures.insert({page, texture});
  }

  for (const auto texture: textures | std::views::values)
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

  SDL_UnlockMutex(mutex);
}

void BitmapFont::setScaleMode(const SDL_ScaleMode scaleMode) {
  SDL_LockMutex(mutex);

  if (!ok) {
    SDL_UnlockMutex(mutex);
    return;
  }

  for (const auto texture: textures | std::views::values)
    SDL_SetTextureScaleMode(texture, scaleMode);
  SDL_UnlockMutex(mutex);
}

int16_t BitmapFont::getKerning(const uint32_t prevCode, const uint32_t nextCode) const {
  const uint64_t key = (static_cast<uint64_t>(prevCode) << 32) | nextCode;
  const auto kernelIt = font->kernels.find(key);
  if (kernelIt == font->kernels.end()) return 0;
  return kernelIt->second;
}

float BitmapFont::getLineHeight(const float scale) const {
  SDL_LockMutex(mutex);
  const float height = ok ? font->common.lineHeight * scale : 0;
  SDL_UnlockMutex(mutex);

  return height;
}

float BitmapFont::measureText(const std::string& text, const float scale) const {
  SDL_LockMutex(mutex);

  if (!ok || text.empty()) {
    SDL_UnlockMutex(mutex);
    return 0;
  }

  std::vector<float> widths;
  float width = 0;
  uint32_t prevChar = 0;

  const auto codepoints = UTF8::to_codepoints(text);
  for (const auto c : codepoints) {
    if (c == '\n') {
      prevChar = 0;
      widths.push_back(width);
      width = 0;
      continue;
    }

    auto charIt = font->chars.find(c);
    if (charIt == font->chars.end()) {
      prevChar = 0;
      continue;
    }

    const auto& ch = charIt->second;

    if (prevChar != 0)
      width += getKerning(prevChar, c) * scale;

    width += ch.xAdvance * scale;
    prevChar = c;
  }
  widths.push_back(width);

  SDL_UnlockMutex(mutex);
  return *std::ranges::max_element(widths);
}

void BitmapFont::renderText(SDL_Renderer* renderer, const float x, const float y, const std::string& text, const std::vector<SDL_Color>& colors, const float scale) {
  SDL_LockMutex(mutex);

  if (!ok || text.empty()) {
    SDL_UnlockMutex(mutex);
    return;
  }

  float cursorX = x;
  float cursorY = y;
  uint32_t prevChar = 0;

  const auto codepoints = UTF8::to_codepoints(text);
  for (size_t i = 0; i < codepoints.size(); ++i) {
    uint32_t charCode = codepoints[i];

    if (charCode == '\n') {
      cursorX = x;
      cursorY += font->common.lineHeight * scale;
      prevChar = 0;
      continue;
    }

    auto charIt = font->chars.find(charCode);
    if (charIt == font->chars.end()) {
      prevChar = 0;
      continue;
    }

    SDL_Color color = {255,255,255,255};
    if (i < colors.size())
      color = colors[i];

    const auto& ch = charIt->second;
    if (prevChar != 0)
      cursorX += getKerning(prevChar, charCode) * scale;

    const auto texIt = textures.find(font->pages[ch.page]);
    if (texIt != textures.end()) {
      SDL_Texture* texture = texIt->second;

      SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
      SDL_SetTextureAlphaMod(texture, color.a);

      SDL_FRect srcRect = {
        static_cast<float>(ch.x),
        static_cast<float>(ch.y),
        static_cast<float>(ch.width),
        static_cast<float>(ch.height)
      };

      SDL_FRect dstRect = {
        cursorX + ch.xOffset * scale,
        cursorY + ch.yOffset * scale,
        ch.width * scale,
        ch.height * scale
      };

      SDL_RenderTexture(renderer, texture, &srcRect, &dstRect);

      SDL_SetTextureColorMod(texture, 255, 255, 255);
      SDL_SetTextureAlphaMod(texture, 255);
    }

    cursorX += ch.xAdvance * scale;
    prevChar = charCode;
  }

  SDL_UnlockMutex(mutex);
}

void BitmapFont::renderText(SDL_Renderer* renderer, const float x, const float y, const std::string& text, const SDL_Color color, const float scale) {
  const std::vector colors(UTF8::length(text), color);
  return renderText(renderer, x, y, text, colors, scale);
}

}
