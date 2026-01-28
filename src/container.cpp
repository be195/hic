#include "basecomponent.hpp"
#include "container.hpp"
#include <SDL3/SDL.h>

#include <utility>

namespace hic {

Container::Container(SDL_Window* window, SDL_Renderer* renderer) : window(window), renderer(renderer) {
  SDL_GetWindowSize(window, &width, &height);
  assetManager = new Assets::Manager();
  audioManager = new Audio::Manager();
}

Container::~Container() {
  if (root)
    root->iDestroy();
  if (assetManager) delete assetManager;
  if (audioManager) delete audioManager;
  if (currentSDLCursor) SDL_DestroyCursor(currentSDLCursor);
}

void Container::setRoot(const std::shared_ptr<BaseComponent> &newRoot) {
  next = newRoot;
  loading = true;
}

void Container::setRoot(const std::string &name) {
  if (const auto it = roots.find(name); it != roots.end())
    setRoot(it->second);
}

void Container::define(const std::string &name, const std::shared_ptr<BaseComponent> &newRoot) {
  roots.insert({name, newRoot});
}

void Container::update(const float deltaTime, const float time) const {
  if (root)
    root->iUpdate(deltaTime, time);
}

void Container::render(const float time) const {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  if (root)
    root->iRender(renderer, time);

  SDL_RenderPresent(renderer);
}

int Container::setLogicalWidth(const int newWidth) {
  lWidth = newWidth;
  logical_res_dirty = true;
  return newWidth;
}

int Container::setLogicalHeight(const int newHeight) {
  lHeight = newHeight;
  logical_res_dirty = true;
  return newHeight;
}

void Container::handleEvent(const SDL_Event& e) {
  if (!root) return;

  switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL: {
      float x = e.motion.x;
      float y = e.motion.y;

      if (lWidth != 0 && lHeight != 0) {
        // casting here is crucial, but so fucking annoying
        const float factorX = static_cast<float>(lWidth) / static_cast<float>(width);
        const float factorY = static_cast<float>(lHeight) / static_cast<float>(height);
        x *= factorX;
        y *= factorY;
      }

      Cursor cursor = root->iHandleMouseEvent(e, x, y);
      if (cursor == Cursor::INHERIT)
        cursor = Cursor::DEFAULT;
      updateCursor(cursor);
    } break;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      root->iHandleKeyboardEvent(e);
      break;

    case SDL_EVENT_QUIT:
      haltLoop();
      break;

    default: break;
  }
}

void Container::startLoop() {
  if (is_in_loop) return;

  is_in_loop = true;
  while (is_in_loop) {
    SDL_Event e;
    while (SDL_PollEvent(&e))
      handleEvent(e);

    if (logical_res_dirty && lWidth != 0 && lHeight != 0) {
      logical_res_dirty = false;
      SDL_SetRenderLogicalPresentation(renderer, lWidth, lHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    }

    if (next) {
      if (root)
        root->iDestroy();

      root = std::move(next);
      if (root)
        root->iPreMount(this);
    }

    const auto nowCounter = SDL_GetPerformanceCounter();
    const double deltaTime = (nowCounter - lastCounterTime) * 1000 / static_cast<float>(SDL_GetPerformanceFrequency());
    const auto time = SDL_GetTicks();

    if (loading) {
      assetManager->processReady(renderer);

      const int pending = assetManager->getPendingCount();
      const int ready = assetManager->getReadyCount();
      const bool isLoading = assetManager->isLoading();

      updateLoadingScreen(deltaTime, time);
      renderLoadingScreen(pending, ready);

      if (pending == 0 && ready == 0 && !isLoading) {
        loading = false;
        root->iPostMount();
      }
    } else {
      update(deltaTime, time);
      render(time);
    }

    lastCounterTime = nowCounter;
  }
}

void Container::haltLoop() {
  is_in_loop = false;
}

void Container::updateCursor(const Cursor cursor) {
  if (cursor == currentCursor || cursor == Cursor::INHERIT) return;

  currentCursor = cursor;

  SDL_SystemCursor sdlCursor;
  switch (cursor) {
    case Cursor::POINTER:
      sdlCursor = SDL_SYSTEM_CURSOR_POINTER; break;
    case Cursor::TEXT:
      sdlCursor = SDL_SYSTEM_CURSOR_TEXT; break;
    case Cursor::CROSSHAIR:
      sdlCursor = SDL_SYSTEM_CURSOR_CROSSHAIR; break;
    case Cursor::WAIT:
      sdlCursor = SDL_SYSTEM_CURSOR_WAIT; break;
    default:
      sdlCursor = SDL_SYSTEM_CURSOR_DEFAULT; break;
  }

  if (currentSDLCursor) SDL_DestroyCursor(currentSDLCursor);
  SDL_Cursor* c = SDL_CreateSystemCursor(sdlCursor);
  SDL_SetCursor(c);
  currentSDLCursor = c;
}

}
