#include "basecomponent.hpp"
#include "container.hpp"
#include <algorithm>
#include <any>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include "utils/geometry.hpp"

namespace hic {

BaseComponent::~BaseComponent() {
  if (!destroyed) iDestroy();
}

void BaseComponent::addChild(const std::shared_ptr<BaseComponent>& child) {
  {
    std::lock_guard lock(childrenMutex);
    children.push_back(child);
  }
  initChild(child);
}

void BaseComponent::prependChild(const std::shared_ptr<BaseComponent>& child) {
  {
    std::lock_guard lock(childrenMutex);
    children.insert(children.begin(), child);
  }
  initChild(child);
}

void BaseComponent::removeChild(const std::shared_ptr<BaseComponent>& child) {
  std::shared_ptr<BaseComponent> childToDestroy = nullptr;
  {
    std::lock_guard lock(childrenMutex);
    if (const auto it = std::ranges::find(children, child); it != children.end()) {
      childToDestroy = *it;
      children.erase(it);
    }
  }
  if (childToDestroy) childToDestroy->iDestroy();
}

void BaseComponent::initChild(const std::shared_ptr<BaseComponent>& child) {
  child->parent = this;
  child->markAbsolutePosDirty();

  if (container) {
    if (mountedStage >= 1) child->iPreMount(container, this);
    if (mountedStage >= 2) child->iPostMount();
  }
}

void BaseComponent::iPreMount(Container* cont, BaseComponent* par) {
  if (!cont) return;

  const bool firstMount = mountedStage == 0;
  container = cont;
  parent    = par;
  destroyed = false;

  logger.info("mounting component (stage 1)");

  if (firstMount) {
    boundingRect.change += [this](const char*, float, float) {
      markRenderTarget();
      markAbsolutePosDirty();
      checkMouse();
    };
  }

  mountedStage = 1;
  if (firstMount) preload();
  premounted();

  std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
  {
    std::lock_guard lock(childrenMutex);
    childrenCopy = children;
  }
  for (const auto& child : childrenCopy)
    child->iPreMount(container, this);
}

void BaseComponent::iPostMount() {
  if (!container) return;

  logger.info("mounting component (stage 2)");
  
  std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
  {
    std::lock_guard lock(childrenMutex);
    childrenCopy = children;
  }
  for (const auto& child : childrenCopy)
    child->iPostMount();

  mountedStage = 2;
  if (fps >= 0)
    pendingDirtyRenderTarget = true;
  mounted();
  requestRender();
}

void BaseComponent::iDestroy() {
  if (destroyed) return;
  logger.info("Destroying component");

  std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
  {
    std::lock_guard lock(childrenMutex);
    childrenCopy = std::move(children);
    children.clear();
  }

  for (const auto& child : childrenCopy)
    child->iDestroy();

  if (renderTarget) {
    if (container) {
      SDL_GPUDevice* device = SDL_GetGPURendererDevice(container->getRenderer());
      if (device)
        container->getGPUGC()->enqueue(device, renderTarget);
      else
        SDL_DestroyTexture(renderTarget);
    } else {
      SDL_DestroyTexture(renderTarget);
    }
    renderTarget = nullptr;
  }

  clearAnimations();
  destroy();

  parent = nullptr;
  container = nullptr;
  destroyed = true;
}

void BaseComponent::markRenderTarget() {
  pendingDirtyRenderTarget = true;
}

void BaseComponent::iUpdate(float deltaTime, const float time) {
  if (!active.load(std::memory_order_acquire) || destroyed) return;

  deltaTime *= timeScale;

  drainPendingTasks();
  updateAnimations(deltaTime);

  if (fps > 0) {
    accumulatedTime += deltaTime;
    const float frameTime = 1000.0f / fps;
    constexpr int maxFrames = 5;
    int frame = 0;

    while (accumulatedTime >= frameTime && frame++ < maxFrames) {
      accumulatedTime -= frameTime;
      requestRender();
      update(frameTime, time);
    }

    if (accumulatedTime >= frameTime * maxFrames)
      accumulatedTime = 0.0f;
  } else {
    update(deltaTime, time);
  }

  std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
  {
    std::lock_guard lock(childrenMutex);
    childrenCopy = children;
  }

  for (const auto& child : childrenCopy) {
    try {
      if (child) child->iUpdate(deltaTime, time);
    } catch (const std::exception& e) {
      logger.error("Child threw an update error: ", e.what());
      if (child) child->active = false;
    }
  }
}

void BaseComponent::iSwapRenderState() {
  if (destroyed) return;

  pendingRS.rect = boundingRect;
  pendingRS.active = active.load(std::memory_order_relaxed);
  pendingRS.clip = clip;
  pendingRS.scale = scale;
  pendingRS.fps = fps;
  pendingRS.needsRender = pendingNeedsRender;
  pendingRS.dirtyRenderTarget = pendingDirtyRenderTarget;
  pendingRS.renderData = buildRenderData();


  pendingNeedsRender = false;
  pendingDirtyRenderTarget = false;

  {
    std::lock_guard lock(rsSwapMutex);
    std::swap(pendingRS, activeRS);
  }

  std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
  {
    std::lock_guard lock(childrenMutex);
    childrenCopy = children;
  }

  for (const auto& child : childrenCopy)
    if (child) child->iSwapRenderState();
}

bool BaseComponent::useRenderTarget(const RenderState& rs) const {
  return rs.fps >= 0 && renderTarget;
}

void BaseComponent::iRender(
  SDL_Renderer* renderer,
  const float time,
  const Position absPos,
  BaseComponent* lastClipParent,
  const Position lastClipAbsPos
) {
  if (!renderer || destroyed) return;

  RenderState rs;
  {
    std::lock_guard lock(rsSwapMutex);
    rs = activeRS;
  }

  if (!rs.active) return;

  SDL_Rect viewport = {0, 0, 0, 0};
  SDL_GetRenderViewport(renderer, &viewport);

  SDL_Rect parentClip;
  if (rs.clip)
    SDL_GetRenderClipRect(renderer, &parentClip);

  const int x = static_cast<int>(absPos.x);
  const int y = static_cast<int>(absPos.y);
  const int w = static_cast<int>(rs.rect.w());
  const int h = static_cast<int>(rs.rect.h());

  const SDL_Rect newViewport = {x, y, w, h};
  SDL_SetRenderViewport(renderer, &newViewport);

  SDL_Rect currentClipRect = {0, 0, w, h};

  if (rs.clip) {
    if (lastClipParent) {
      const int offsetX = static_cast<int>(absPos.x - lastClipAbsPos.x);
      const int offsetY = static_cast<int>(absPos.y - lastClipAbsPos.y);

      SDL_Rect parentClipLocal = parentClip;
      parentClipLocal.x -= offsetX;
      parentClipLocal.y -= offsetY;

      const SDL_Rect componentBounds = {0, 0, w, h};
      if (!SDL_GetRectIntersection(&componentBounds, &parentClipLocal, &currentClipRect))
        currentClipRect = {0, 0, 0, 0};
    }
    SDL_SetRenderClipRect(renderer, &currentClipRect);
  }

  if (rs.dirtyRenderTarget) {
    if (renderTarget) SDL_DestroyTexture(renderTarget);
    renderTarget = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_RGBA32,
      SDL_TEXTUREACCESS_TARGET,
      w, h
    );
    if (!renderTarget)
      logger.error("failed to create render target texture: ", SDL_GetError());
    else
      SDL_SetTextureScaleMode(renderTarget, SDL_SCALEMODE_NEAREST);
  }

  const bool useRT = useRenderTarget(rs);

  if (rs.needsRender || rs.fps == -1) {
    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer);

    if (useRT) {
      SDL_SetRenderTarget(renderer, renderTarget);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
      SDL_RenderClear(renderer);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    }

    draw(renderer, time, rs.renderData);

    std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
    {
      std::lock_guard lock(childrenMutex);
      childrenCopy = children;
    }

    for (const auto& child : childrenCopy) {
      if (!child) continue;

      try {
        RenderState childRS;
        {
          std::lock_guard lock(child->rsSwapMutex);
          childRS = child->activeRS;
        }

        SDL_Rect childRect = {
          static_cast<int>(childRS.rect.x()),
          static_cast<int>(childRS.rect.y()),
          static_cast<int>(childRS.rect.w()),
          static_cast<int>(childRS.rect.h())
        };

        if (!rs.clip || SDL_HasRectIntersection(&childRect, &currentClipRect)) {
          const Position childAbsPos = {
            absPos.x + childRS.rect.x(),
            absPos.y + childRS.rect.y()
          };
          if (rs.clip)
            child->iRender(renderer, time, childAbsPos, this, absPos);
          else
            child->iRender(renderer, time, childAbsPos, lastClipParent, lastClipAbsPos);
        }
      } catch (const std::exception& e) {
        logger.error("Child threw a render error: ", e.what());
      }
    }

    postDraw(renderer, time, rs.renderData);

    if (useRT)
      SDL_SetRenderTarget(renderer, previousTarget);
  }

  if (useRT)
    SDL_RenderTexture(renderer, renderTarget, nullptr, nullptr);

  SDL_SetRenderViewport(renderer, &viewport);
  if (rs.clip)
    SDL_SetRenderClipRect(renderer, &parentClip);
}

void BaseComponent::postTask(std::function<void()> func) {
  std::lock_guard lock(pendingTasksMutex);
  pendingTasks.push_back(std::move(func));
}

void BaseComponent::drainPendingTasks() {
  std::vector<std::function<void()>> tasksToExecute;
  {
    std::lock_guard lock(pendingTasksMutex);
    tasksToExecute.swap(pendingTasks);
  }
  for (const auto& task : tasksToExecute)
    task();
  postTaskDrain();
}

Cursor BaseComponent::iHandleMouseEvent(const SDL_Event& e, float x, float y) {
  if (!active.load(std::memory_order_acquire)) return Cursor::INHERIT;

  lastMousePos = {x, y};
  x -= boundingRect.x();
  y -= boundingRect.y();

  triggerMouseEnter();

  auto setCursor = Cursor::INHERIT;

  std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
  {
    std::lock_guard lock(childrenMutex);
    childrenCopy = children;
  }

  for (const auto& child : std::ranges::reverse_view(childrenCopy)) {
    if (Rectangle& c = child->boundingRect; c.contains(x, y)) {
      const Cursor childCursor = child->iHandleMouseEvent(e, x, y);
      if (childCursor != Cursor::INHERIT && e.type != SDL_EVENT_MOUSE_WHEEL) {
        setCursor = childCursor;
        break;
      }
    } else {
      child->triggerMouseLeave();
    }
  }

  if (setCursor == Cursor::INHERIT)
    setCursor = handleMouseEvent(e, x, y);

  return setCursor;
}

bool BaseComponent::iHandleKeyboardEvent(const SDL_Event& e) {
  if (!active.load(std::memory_order_acquire)) return false;

  std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
  {
    std::lock_guard lock(childrenMutex);
    childrenCopy = children;
  }

  for (const auto& child : std::ranges::reverse_view(childrenCopy))
    if (child->iHandleKeyboardEvent(e))
      return true;

  return handleKeyboardEvent(e);
}

void BaseComponent::checkMouse() {
  if (mouseInside && !isLastMousePosInvalid()) {
    if (!boundingRect.contains(lastMousePos.x, lastMousePos.y))
      triggerMouseLeave();
  } else if (parent != nullptr) {
    const auto relativePos = parent->amIOverlappingWithMouse(this);
    if (relativePos != nullptr) {
      lastMousePos.x = relativePos->x;
      lastMousePos.y = relativePos->y;
      triggerMouseEnter();
    }
  }
}

bool BaseComponent::isLastMousePosInvalid() const {
  return lastMousePos.x == -1 && lastMousePos.y == -1;
}

Position* BaseComponent::amIOverlappingWithMouse(const BaseComponent* component) {
  const float relX = lastMousePos.x - boundingRect.x();
  const float relY = lastMousePos.y - boundingRect.y();

  if (component->boundingRect.contains(relX, relY)) {
    overlappingRes.x = relX;
    overlappingRes.y = relY;
    return &overlappingRes;
  }
  return nullptr;
}

void BaseComponent::triggerMouseEnter() {
  if (!mouseInside) {
    mouseInside = true;
    mouseEnter();
  }
}

void BaseComponent::triggerMouseLeave() {
  if (mouseInside) {
    mouseInside = false;
    mouseLeave();

    std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
    {
      std::lock_guard lock(childrenMutex);
      childrenCopy = children;
    }
    for (const auto& child : childrenCopy)
      child->triggerMouseLeave();
  }
}

Position BaseComponent::getAbsolutePosition() const {
  if (absolutePosDirty) {
    float absX = boundingRect.x();
    float absY = boundingRect.y();
    const BaseComponent* p = parent;
    while (p) {
      absX += p->boundingRect.x();
      absY += p->boundingRect.y();
      p = p->parent;
    }
    cachedAbsolutePos = {absX, absY};
    absolutePosDirty = false;
  }
  return cachedAbsolutePos;
}

void BaseComponent::markAbsolutePosDirty() const {
  absolutePosDirty = true;
  
  std::vector<std::shared_ptr<BaseComponent>> childrenCopy;
  {
    std::lock_guard lock(childrenMutex);
    childrenCopy = children;
  }
  for (const auto& child : childrenCopy)
    child->markAbsolutePosDirty();
}

bool BaseComponent::collidingWith(const BaseComponent& other, const bool absolute) const {
  if (!absolute && parent != other.parent)
    throw std::runtime_error("components must be on the same plane for relative collision check");

  const Rectangle r1 = absolute
    ? Rectangle(getAbsolutePosition().x, getAbsolutePosition().y, boundingRect.w(), boundingRect.h())
    : boundingRect;

  const Rectangle r2 = absolute
    ? Rectangle(other.getAbsolutePosition().x, other.getAbsolutePosition().y, other.boundingRect.w(), other.boundingRect.h())
    : other.boundingRect;

  return r1.overlaps(r2);
}

} // namespace hic
