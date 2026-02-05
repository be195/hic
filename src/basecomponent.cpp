#include "basecomponent.hpp"
#include "container.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <ranges>

namespace hic {

BaseComponent::~BaseComponent() {
  if (!destroyed) iDestroy();
}

void BaseComponent::addChild(const std::shared_ptr<BaseComponent>& child) {
  child->parent = this;
  children.push_back(child);
  child->markAbsolutePosDirty();

  if (container) {
    // we can't predict when it will load the assets that it requires,
    // so all the resposibility goes to the parent component
    if (mountedStage >= 1)
      child->iPreMount(container, this);
    if (mountedStage >= 2)
      child->iPostMount();
  }
}

void BaseComponent::removeChild(const std::shared_ptr<BaseComponent>& child) {
  if (const auto it = std::ranges::find(children, child); it != children.end()) {
    (*it)->iDestroy();
    children.erase(it);
  }
}

void BaseComponent::markRenderTarget() {
  if (!useRenderTarget()) return;
  dirtyRenderTarget = true;
}

bool BaseComponent::useRenderTarget() const {
  return fps >= 0 && renderTarget;
}

void BaseComponent::iPreMount(Container* cont, BaseComponent* par) {
  const bool firstMount = mountedStage == 0;
  container = cont;
  parent = par;
  destroyed = false;

  logger.info("mounting component (stage 1)");

  if (firstMount) {
    boundingRect.change += [this](const char* prop, float old, float nw) {
      this->markRenderTarget();
      this->markAbsolutePosDirty();
      this->checkMouse();
    };
  }

  mountedStage = 1;
  if (firstMount) preload();
  premounted();

  for (const auto& child : children)
    child->iPreMount(container, this);
}

void BaseComponent::iPostMount() {
  logger.info("mounting component (stage 2)");
  for (const auto& child : children)
    child->iPostMount();

  mountedStage = 2;
  if (fps >= 0) dirtyRenderTarget = true;
  mounted();
  requestRender();
}

void BaseComponent::iUpdate(float deltaTime, const float time) {
  if (!active) return;

  deltaTime *= timeScale;

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
  } else
    update(deltaTime, time);

  for (const auto& child : children) {
    try {
      child->iUpdate(deltaTime, time);
    } catch (const std::exception& e) {
      logger.error("Child threw an update error: ", e.what());
      child->active = false;
    }
  }
}

void BaseComponent::checkMouse() {
  if (mouseInside && !isLastMousePosInvalid()) {
    if (!boundingRect.contains(lastMousePos.x, lastMousePos.y))
      triggerMouseLeave();
  } else if (parent != nullptr) {
    // ReSharper disable once CppTooWideScopeInitStatement
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
  // ReSharper disable once CppTooWideScopeInitStatement
  const float relY = lastMousePos.y - boundingRect.y();

  if (component->boundingRect.contains(relX, relY)) {
    overlappingRes.x = relX;
    overlappingRes.y = relY;
    return &overlappingRes;
  }

  return nullptr;
}

void BaseComponent::iRender(SDL_Renderer* renderer, const float time) {
  if (!active) return;

  SDL_Rect viewport;
  SDL_Rect* clipRect = nullptr;
  SDL_GetRenderViewport(renderer, &viewport);

  if (clip)
    SDL_GetRenderClipRect(renderer, clipRect);

  const int x = static_cast<int>(boundingRect.x());
  const int y = static_cast<int>(boundingRect.y());
  const int w = static_cast<int>(boundingRect.w());
  const int h = static_cast<int>(boundingRect.h());

  const SDL_Rect newViewport = {x, y, w, h};
  SDL_SetRenderViewport(renderer, &newViewport);

  if (clip) {
    const SDL_Rect newClipRect = {0, 0, w, h};
    SDL_SetRenderClipRect(renderer, &newClipRect);
  }

  if (dirtyRenderTarget) {
    if (renderTarget) SDL_DestroyTexture(renderTarget);

    renderTarget = SDL_CreateTexture(renderer,
      SDL_PIXELFORMAT_RGBA32,
      SDL_TEXTUREACCESS_TARGET,
      boundingRect.w(), boundingRect.h()
    );
    if (!renderTarget)
      logger.error("failed to create render target texture", SDL_GetError());
    else
      SDL_SetTextureScaleMode(renderTarget, SDL_SCALEMODE_NEAREST);
    dirtyRenderTarget = false;
    needsRender = true;
  }

  const auto useRenderTargetB = useRenderTarget();
  if (needsRender || fps == -1) {
    needsRender = false;

    if (useRenderTargetB) {
      SDL_SetRenderTarget(renderer, renderTarget);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
      SDL_RenderClear(renderer);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    }
    render(renderer, time);

    for (const auto& child : children) {
      try {
        // ReSharper disable once CppTooWideScopeInitStatement
        Rectangle c = child->boundingRect;
        if (!clip ||
          (c.x() + c.w() >= 0 &&
          c.y() + c.h() >= 0 &&
          c.x() < w && c.y() < h)) {
          child->iRender(renderer, time);
        }
      } catch (const std::exception& e) {
        logger.error("Child threw a render error: ", e.what());
        child->active = false;
      }
    }

    postComponentRender(renderer, time);
    if (useRenderTargetB)
      SDL_SetRenderTarget(renderer, nullptr);
  }

  if (useRenderTargetB)
    SDL_RenderTexture(renderer, renderTarget, nullptr, nullptr);

  SDL_SetRenderViewport(renderer, &viewport);
  if (clip)
    SDL_SetRenderClipRect(renderer, clipRect);
}

void BaseComponent::iDestroy() {
  if (destroyed) return;
  logger.info("Destroying component");

  for (const auto& child : children)
    child->iDestroy();

  if (renderTarget)
    SDL_DestroyTexture(renderTarget);

  clearAnimations();
  destroy();

  parent = nullptr;
  container = nullptr;
  destroyed = true;
}

Cursor BaseComponent::iHandleMouseEvent(const SDL_Event& e, float x, float y) {
  if (!active) return Cursor::INHERIT;

  lastMousePos = {x, y};

  x -= boundingRect.x();
  y -= boundingRect.y();
#ifdef HIC_DEBUG_MOUSE
  logger.debug("mouse move:", x, y);
#endif

  triggerMouseEnter();

  auto setCursor = Cursor::INHERIT;

  for (const auto & child : std::ranges::reverse_view(children)) {
    if (Rectangle& c = child->boundingRect; c.contains(x, y)) {
      // ReSharper disable once CppTooWideScopeInitStatement
      const Cursor childCursor = child->iHandleMouseEvent(e, x, y);
      if (childCursor != Cursor::INHERIT && e.type != SDL_EVENT_MOUSE_WHEEL) {
        setCursor = childCursor;
        break;
      }
    } else
      child->triggerMouseLeave();
  }

  if (setCursor == Cursor::INHERIT)
    setCursor = handleMouseEvent(e, x, y);

  return setCursor;
}

bool BaseComponent::iHandleKeyboardEvent(const SDL_Event &e) {
  if (!active) return false;

  for (const auto & child : std::ranges::reverse_view(children))
    if (child->iHandleKeyboardEvent(e))
      return true;

  return handleKeyboardEvent(e);
}

bool BaseComponent::collidingWith(const BaseComponent& other, const bool absolute) const {
  if (!absolute && parent != other.parent)
    throw std::runtime_error("components must be on the same plane for relative collision check");

  const Rectangle r1 = absolute ?
    Rectangle(this->getAbsolutePosition().x,
      this->getAbsolutePosition().y,
      boundingRect.w(), boundingRect.h()) : boundingRect;

  const Rectangle r2 = absolute ?
    Rectangle(other.getAbsolutePosition().x,
      other.getAbsolutePosition().y,
      other.boundingRect.w(), other.boundingRect.h()) : other.boundingRect;

  return r1.overlaps(r2);
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

    cachedAbsolutePos = { absX, absY };
    absolutePosDirty = false;
  }

  return cachedAbsolutePos;
}

void BaseComponent::triggerMouseEnter() {
  if (!mouseInside) {
#ifdef HIC_DEBUG_MOUSE
    logger.debug("triggerMouseEnter()");
#endif
    mouseInside = true;
    mouseEnter();
  }
}

void BaseComponent::triggerMouseLeave() {
  if (mouseInside) {
#ifdef HIC_DEBUG_MOUSE
    logger.debug("triggerMouseLeave()");
#endif
    mouseInside = false;
    mouseLeave();

    for (const auto & child : children)
      child->triggerMouseLeave();
  }
}

void BaseComponent::markAbsolutePosDirty() const {
  absolutePosDirty = true;
  for (const auto& child : children)
    child->markAbsolutePosDirty();
}

} // namespace hic