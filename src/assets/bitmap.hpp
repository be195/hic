#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "base.hpp"
#include "../utils/bmfont.hpp"

namespace hic::Assets {

class HIC_API BitmapFont : public Base {
public:
  std::string folderName;
  bool ok = false;
  std::unique_ptr<BMFont> font;
  std::unordered_map<std::string, SDL_Surface*> surfaces;
  std::unordered_map<std::string, SDL_Texture*> textures;

  explicit BitmapFont(std::string folderName);
  ~BitmapFont() override;

  void preload(Manager* manager) override;
  void use(SDL_Renderer *renderer) override;
  void setScaleMode(SDL_ScaleMode scaleMode);

  int16_t getKerning(uint32_t prevCode, uint32_t nextCode) const;
  void renderText(SDL_Renderer* renderer, float x, float y, const std::string& text, const std::vector<SDL_Color>& colors, float scale = 1.0f);
  void renderText(SDL_Renderer* renderer, float x, float y, const std::string& text, SDL_Color color, float scale = 1.0f);

  float getLineHeight(float scale = 1.0f) const;
  float measureText(const std::string& text, float scale = 1.0f) const;

  std::string getCacheKey() const override { return "bmf#" + folderName; }
private:
  SDL_Mutex* mutex = nullptr;

  void cleanup();
};

}
