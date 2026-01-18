#include "basecomponent.hpp"
#include "container.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <ranges>

namespace hic {

BaseComponent::BaseComponent() : logger(typeid(*this).name()) {}
BaseComponent::~BaseComponent() = default;

void BaseComponent::addChild(const std::shared_ptr<BaseComponent>& child) {
  child->parent = this;
  children.push_back(child);
  child->markAbsolutePosDirty();

  if (container)
    child->iMount(container, this);
}

void BaseComponent::removeChild(const std::shared_ptr<BaseComponent>& child) {
  auto it = std::ranges::find(children, child);
  if (it != children.end()) {
    (*it)->iDestroy();
    children.erase(it);
  }
}

void BaseComponent::iMount(Container* cont, BaseComponent* par) {
  container = cont;
  parent = par;

  logger.info("Mounting component");
  boundingRect.change += [this](const char* prop, float old, float nw) {
    this->markAbsolutePosDirty();
    this->checkMouse();
  };

  for (const auto& child : children)
    child->iMount(container, this);

  preload();
  mounted();

  logger.info("Mounted component");
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

  if (needsRender || fps == -1) {
    needsRender = false;

    render(renderer, time);

    for (const auto& child : children) {
      try {
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
  }

  SDL_SetRenderViewport(renderer, &viewport);
  if (clip)
    SDL_SetRenderClipRect(renderer, clipRect);
}

void BaseComponent::iDestroy() {
  logger.info("Destroying component");

  for (const auto& child : children)
    child->iDestroy();

  clearAnimations();
  destroy();

  parent = nullptr;
  container = nullptr;
}

Cursor BaseComponent::iHandleMouseEvent(const SDL_Event& e, float x, float y) {
  if (!active) return Cursor::INHERIT;

  lastMousePos = {x, y};

  x -= boundingRect.x();
  y -= boundingRect.y();
  logger.debug("mouse move:", x, y);

  triggerMouseEnter();

  auto setCursor = Cursor::INHERIT;

  for (auto & child : std::ranges::reverse_view(children)) {
    Rectangle& c = child->boundingRect;

    if (c.contains(x, y)) {
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
    Rectangle(const_cast<BaseComponent*>(this)->getAbsolutePosition().x,
      const_cast<BaseComponent*>(this)->getAbsolutePosition().y,
      boundingRect.w(), boundingRect.h()) : boundingRect;

  const Rectangle r2 = absolute ?
    Rectangle(const_cast<BaseComponent*>(&other)->getAbsolutePosition().x,
      const_cast<BaseComponent*>(&other)->getAbsolutePosition().y,
      other.boundingRect.w(), other.boundingRect.h()) : other.boundingRect;

  return r1.overlaps(r2);
}

Position BaseComponent::getAbsolutePosition() {
  if (absolutePosDirty) {
    float absX = boundingRect.x();
    float absY = boundingRect.y();

    BaseComponent* p = parent;
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
    logger.debug("triggerMouseEnter()");
    mouseInside = true;
    mouseEnter();
  }
}

void BaseComponent::triggerMouseLeave() {
  if (mouseInside) {
    logger.debug("triggerMouseLeave()");
    mouseInside = false;
    mouseLeave();

    for (const auto & child : children)
      child->triggerMouseLeave();
  }
}

void BaseComponent::markAbsolutePosDirty() {
  absolutePosDirty = true;
  for (const auto& child : children)
    child->markAbsolutePosDirty();
}

} // namespace hic