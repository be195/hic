#pragma once

#include "utils/animation.hpp"
#include "utils/logging.hpp"
#include "utils/geometry.hpp"
#include "utils/hicapi.hpp"
#include <any>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_render.h>

namespace hic {

enum class Cursor {
  DEFAULT,
  POINTER,
  TEXT,
  CROSSHAIR,
  WAIT,
  INHERIT
};

class HIC_API Container;

struct HIC_API RenderState {
  Rectangle rect;
  bool active = true;
  bool clip = true;
  float scale = 1.0f;
  int8_t fps = -1;
  bool needsRender = false;
  bool dirtyRenderTarget = false;
  std::any renderData;
};

class HIC_API BaseComponent : public AnimationMixin {
public:
  explicit BaseComponent(const std::string& loggerTag = "BaseComponent")
    : logger(loggerTag) {}
  virtual ~BaseComponent();

  virtual void preload() {}
  virtual void premounted() {}
  virtual void mounted() {}
  virtual void destroy() {}
  virtual void postTaskDrain() {}

  virtual void update(float deltaTime, float time) {}
  virtual std::any buildRenderData() { return {}; }
  virtual void draw(SDL_Renderer* r, float time, const std::any& data) {}
  virtual void postDraw(SDL_Renderer* r, float time, const std::any& data) {}

  virtual Cursor handleMouseEvent(const SDL_Event& e, float x, float y) {
    return Cursor::INHERIT;
  }
  virtual bool handleKeyboardEvent(const SDL_Event& e) { return false; }
  virtual void mouseEnter() {}
  virtual void mouseLeave() {}

  void addChild(const std::shared_ptr<BaseComponent>& child);
  void prependChild(const std::shared_ptr<BaseComponent>& child);
  void removeChild(const std::shared_ptr<BaseComponent>& child);

  void iPreMount(Container* cont, BaseComponent* par = nullptr);
  void iPostMount();
  void iUpdate(float deltaTime, float time);
  void iSwapRenderState();
  void iRender(
    SDL_Renderer* renderer,
    float time,
    Position absPos,
    BaseComponent* lastClipParent,
    Position lastClipAbsPos
  );
  void iDestroy();

  Cursor iHandleMouseEvent(const SDL_Event& e, float x, float y);
  bool iHandleKeyboardEvent(const SDL_Event& e);

  [[nodiscard]] bool collidingWith(const BaseComponent& other, bool absolute = false) const;
  Position getAbsolutePosition() const;
  [[nodiscard]] Position getCenter() const { return boundingRect.center(); }

  void markRenderTarget();

  std::atomic<bool> active{true};
  bool clip = true;
  float scale = 1.0f;
  float timeScale = 1.0f;
  int8_t fps = -1;
  Rectangle boundingRect;

  RenderState activeRS;

protected:
  Logger logger;
  Container* container = nullptr;
  BaseComponent* parent = nullptr;
  std::vector<std::shared_ptr<BaseComponent>> children;
  mutable std::recursive_mutex childrenMutex;

  Position* amIOverlappingWithMouse(const BaseComponent* component);
  void checkMouse();
  float accumulatedTime = 0.0f;

  mutable Position cachedAbsolutePos;
  mutable bool absolutePosDirty = true;
  bool isLastMousePosInvalid() const;
  Position lastMousePos = {-1, -1};
  bool mouseInside = false;

  void requestRender() {
    pendingNeedsRender = true;
  }

  void postTask(std::function<void()> func);

private:
  uint8_t mountedStage = 0;
  bool destroyed = false;

  bool pendingNeedsRender = false;
  bool pendingDirtyRenderTarget = false;

  RenderState pendingRS;
  std::mutex rsSwapMutex;

  SDL_Texture* renderTarget = nullptr;

  void triggerMouseEnter();
  void triggerMouseLeave();
  void markAbsolutePosDirty() const;
  bool useRenderTarget(const RenderState& rs) const;
  void initChild(const std::shared_ptr<BaseComponent>& child);

  std::mutex pendingTasksMutex;
  std::vector<std::function<void()>> pendingTasks;
  void drainPendingTasks();

  Position overlappingRes = {-1, -1};
};

} // namespace hic
