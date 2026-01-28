#pragma once

#include "utils/animation.hpp"
#include "utils/logging.hpp"
#include "utils/geometry.hpp"
#include "utils/hicapi.hpp"
#include <memory>
#include <SDL3/SDL.h>

namespace hic {

enum class Cursor {
  DEFAULT,
  POINTER,
  TEXT,
  CROSSHAIR,
  WAIT,
  INHERIT
};

class HIC_API Container; // will be declared later

class HIC_API BaseComponent : public AnimationMixin {
public:
  explicit BaseComponent(const std::string& loggerTag = "BaseComponent") : logger(std::move(loggerTag)) {};
  virtual ~BaseComponent();

  virtual void preload() {}
  virtual void mounted() {}
  virtual void destroy() {}

  virtual void update(float deltaTime, float time) {}
  virtual void render(SDL_Renderer* r, float time) {}
  virtual void postComponentRender(SDL_Renderer* r, float time) {}

  virtual Cursor handleMouseEvent(const SDL_Event& e, float x, float y) {
    return Cursor::INHERIT;
  }
  virtual bool handleKeyboardEvent(const SDL_Event& e) {
    return false;
  }
  virtual void mouseEnter() {}
  virtual void mouseLeave() {}

  void addChild(const std::shared_ptr<BaseComponent>& child);
  void removeChild(const std::shared_ptr<BaseComponent>& child);

  void iMount(Container* cont, BaseComponent* par = nullptr);
  void iUpdate(float deltaTime, float time);
  void iRender(SDL_Renderer* renderer, float time);
  void iDestroy();

  Cursor iHandleMouseEvent(const SDL_Event& e, float x, float y);
  bool iHandleKeyboardEvent(const SDL_Event& e);

  [[nodiscard]] bool collidingWith(const BaseComponent& other, bool absolute = false) const;

  Position getAbsolutePosition() const;
  [[nodiscard]] Position getCenter() const { return boundingRect.center(); }

  bool active = true;
  bool clip = true;
  float scale = 1.0f;
  float timeScale = 1.0f;
  int fps = -1; // unlimited
  Rectangle boundingRect;

protected:
  Logger logger;
  Container* container = nullptr;
  BaseComponent* parent = nullptr;
  std::vector<std::shared_ptr<BaseComponent>> children;

  Position* amIOverlappingWithMouse(const BaseComponent* component);
  void checkMouse();

  float accumulatedTime = 0.0f;

  mutable Position cachedAbsolutePos;
  mutable bool absolutePosDirty = true;

  bool isLastMousePosInvalid() const;
  Position lastMousePos;
  bool mouseInside = false;

  void requestRender() { needsRender = true; }

private:
  bool destroyed = false;
  bool needsRender = true;

  void triggerMouseEnter();
  void triggerMouseLeave();
  void markAbsolutePosDirty() const;

  Position overlappingRes = {-1, -1};

  SDL_Texture* cacheTexture = nullptr;
};

} // namespace hic