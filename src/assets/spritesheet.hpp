#pragma once

#include <unordered_map>
#include <optional>
#include "image.hpp"
#include "../utils/hicapi.hpp"
#include <nlohmann/json.hpp>
#include "../utils/events.hpp"

namespace hic::Assets {

namespace ssjson {
  struct IPoint {
    float x;
    float y;
  };

  struct ISize {
    float w;
    float h;
  };

  struct IRectangle {
    float x;
    float y;
    float w;
    float h;
  };

  struct SpritesheetFrameData {
    IRectangle frame;
    bool rotated;
    bool trimmed;
    IRectangle spriteSourceSize;
    ISize sourceSize;
    IPoint pivot;
  };

  struct SpritesheetMeta {
    std::string app;
    std::string version;
    std::string image;
    std::string format;
    ISize size;
    float scale;
  };

  struct SpritesheetAnimation {
    float fps;
    std::vector<std::string> frames;
  };

  struct SpritesheetData {
    std::unordered_map<std::string, SpritesheetAnimation> animations;
    std::unordered_map<std::string, SpritesheetFrameData> frames;
    SpritesheetMeta meta;
  };

  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IPoint, x, y)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ISize, w, h)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IRectangle, x, y, w, h)

  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
      SpritesheetFrameData,
      frame,
      rotated,
      trimmed,
      spriteSourceSize,
      sourceSize,
      pivot
  )

  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
      SpritesheetMeta,
      app,
      version,
      image,
      format,
      size,
      scale
  )

  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
      SpritesheetAnimation,
      fps,
      frames
  )

  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
      SpritesheetData,
      animations,
      frames,
      meta
  )
}

class Spritesheet;

class HIC_API AnimatedSpritesheetPart {
public:
  Event<void> loop;

  explicit AnimatedSpritesheetPart(Spritesheet* spritesheet, const ssjson::SpritesheetAnimation &animData);

  void render(SDL_Renderer* renderer, float x, float y, bool flipX = false, bool flipY = false);
  void update(float deltaTime);
private:
  struct Match {
    std::string match;
    size_t start;
    size_t end;
    std::vector<std::string> values;
  };

  Spritesheet* spritesheet;
  std::vector<std::string> frames;
  std::vector<std::string>::const_iterator it;
  float frameTime;
  float accumulatedTime = 0.0f;

  void normalizeFrames(const std::vector<std::string> &given);

  static std::vector<std::string> expandBraces(const std::string& pattern);
};

class HIC_API Spritesheet : public Image {
public:
  std::string folderName;

  explicit Spritesheet(std::string folderName);
  ~Spritesheet() override;

  void preload(Manager* manager) override;
  void use(SDL_Renderer *renderer) override;
  void renderFrame(SDL_Renderer* renderer, const std::string &frame, float x, float y, bool flipX = false, bool flipY = false);
  std::shared_ptr<AnimatedSpritesheetPart> animation(const std::string &animation);
  std::shared_ptr<AnimatedSpritesheetPart> createAnimation(const std::string &animation);
  std::optional<ssjson::ISize> getSize(const std::string &frame);
  std::optional<ssjson::ISize> getAnimationSize(const std::string &anim);

  std::string getCacheKey() const override { return "spr#" + folderName; }
private:
  std::optional<ssjson::SpritesheetData> data;
  std::unordered_map<std::string, std::shared_ptr<AnimatedSpritesheetPart>> cache;
  mutable SDL_Mutex* animationMutex = nullptr;
};

}