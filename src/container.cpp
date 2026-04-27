#include "basecomponent.hpp"
#include "container.hpp"
#include "assets/shader.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <utility>

#ifdef HIC_USE_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#endif

namespace hic {

Container::Container(SDL_Window* window, SDL_Renderer* renderer)
  : window(window), renderer(renderer)
{
  SDL_GetWindowSize(window, &width, &height);
  assetManager = std::make_unique<Assets::Manager>();
  audioManager = std::make_unique<Audio::Manager>();
#ifdef HIC_USE_IMGUI
  HICL("Container").debug("initializing ImGui, to turn off use HIC_USE_IMGUI option");
  IMGUI_CHECKVERSION();
  imguiContext = ImGui::CreateContext();
  imguiIo = &ImGui::GetIO();
  imguiIo->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  imguiIo->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
#endif
}

Container::~Container() {
  if (ctrThread) {
    haltLoop();
    SDL_WaitThread(ctrThread, nullptr);
  }
#ifdef HIC_USE_IMGUI
  ImGui::DestroyContext(imguiContext);
#endif
#if defined(__APPLE__) && defined(__MACH__)
  {
    std::lock_guard lock(rootMutex);
    if (rootPtr) rootPtr->iDestroy();
  }
#else
  if (const auto root = rootPtr.load(std::memory_order_acquire))
    root->iDestroy();
#endif
  if (currentSDLCursor) SDL_DestroyCursor(currentSDLCursor);
  Assets::GPUShader::cleanupTexturePool();
}

void Container::setRoot(const std::shared_ptr<BaseComponent>& newRoot) {
#if defined(__APPLE__) && defined(__MACH__)
  std::lock_guard lock(nextMutex);
  nextPtr = newRoot;
#else
  nextPtr.store(newRoot, std::memory_order_release);
#endif
}

void Container::setRoot(const std::string& name) {
  if (const auto it = roots.find(name); it != roots.end())
    setRoot(it->second);
}

void Container::define(const std::string& name, const std::shared_ptr<BaseComponent>& newRoot) {
  if (!newRoot) return;
  roots.insert({name, newRoot});
}

int Container::setLogicalWidth(const int newWidth) {
  lWidth = newWidth;
  logicalResDirty = true;
  return newWidth;
}

int Container::setLogicalHeight(const int newHeight) {
  lHeight = newHeight;
  logicalResDirty = true;
  return newHeight;
}

void Container::update(const float deltaTime, const float time) const {
#if defined(__APPLE__) && defined(__MACH__)
  std::lock_guard lock(rootMutex);
  if (const auto root = rootPtr)
#else
  if (const auto root = rootPtr.load(std::memory_order_acquire))
#endif
    root->iUpdate(deltaTime, time);
}

void Container::render(const float time) const {
#if defined(__APPLE__) && defined(__MACH__)
  std::lock_guard lock(rootMutex);
  if (const auto root = rootPtr)
#else
  if (const auto root = rootPtr.load(std::memory_order_acquire))
#endif
    root->iRender(renderer, time, root->activeRS.rect.pos(), nullptr, {0, 0});
}

void Container::dispatchEvent(const SDL_Event& e) {
#ifdef HIC_USE_IMGUI
  {
    std::lock_guard lock(imguiEventMutex);
    imguiEventQueue.push_back(e);
  }
#endif

#if defined(__APPLE__) && defined(__MACH__)
  std::lock_guard lock(rootMutex);
  auto root = rootPtr;
#else
  auto root = rootPtr.load(std::memory_order_acquire);
#endif
  if (!root) return;

  switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL: {
      const int scaleX = width  / lWidth;
      const int scaleY = height / lHeight;
      const int scale = std::min(scaleX, scaleY);

      const float offsetX = (width  - lWidth  * scale) / 2.0f;
      const float offsetY = (height - lHeight * scale) / 2.0f;

      const float adjustedX = (e.motion.x - offsetX) / scale;
      const float adjustedY = (e.motion.y - offsetY) / scale;

      Cursor cursor = root->iHandleMouseEvent(e, adjustedX, adjustedY);
      if (cursor == Cursor::INHERIT) cursor = Cursor::DEFAULT;
      updateCursor(cursor);
    } break;

    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      root->iHandleKeyboardEvent(e);
      break;

    default: break;
  }
}

int Container::ctrThreadFunc(void* data) {
  static_cast<Container*>(data)->ctrThreadLoop();
  return 0;
}

void Container::ctrThreadLoop() {
  while (isInLoop.load(std::memory_order_acquire)) {

#ifdef HIC_USE_IMGUI
    {
      std::vector<SDL_Event> imguiEvents;
      {
        std::lock_guard lock(imguiEventMutex);
        imguiEvents.swap(imguiEventQueue);
      }
      for (const auto& e : imguiEvents)
        ImGui_ImplSDL3_ProcessEvent(&e);
    }
#endif

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

#ifdef HIC_USE_IMGUI
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
#endif

    if (logicalResDirty.exchange(false, std::memory_order_acq_rel) && lWidth != 0 && lHeight != 0)
      SDL_SetRenderLogicalPresentation(renderer, lWidth, lHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    if (loading.load(std::memory_order_acquire)) {
      assetManager->processReady(renderer);

      const int pending = assetManager->getPendingCount();
      const int readyCount = assetManager->getReadyCount();
      const bool stillLoading = assetManager->isLoading();

      renderLoadingScreen(pending, readyCount);

      if (pending == 0 && readyCount == 0 && !stillLoading) {
        loading.store(false, std::memory_order_release);
#if defined(__APPLE__) && defined(__MACH__)
        std::lock_guard lock(rootMutex);
        if (rootPtr) rootPtr->iPostMount();
#else
        if (const auto root = rootPtr.load(std::memory_order_acquire))
          root->iPostMount();
#endif
      }
    } else
      render(SDL_GetTicks());

#ifdef HIC_USE_IMGUI
    ImGui::Render();
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    // FUCK!
    SDL_SetRenderLogicalPresentation(renderer, w, h, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_SetRenderLogicalPresentation(renderer, lWidth, lHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
#endif

    SDL_RenderPresent(renderer);
  }
}

void Container::startLoop() {
  bool expected = false;
  if (!isInLoop.compare_exchange_strong(expected, true))
    return;

  ctrThread = SDL_CreateThread(ctrThreadFunc, "hic::RenderThread", this);
  if (!ctrThread) {
    isInLoop.store(false);
    return;
  }

  Uint64 lastCounter = SDL_GetPerformanceCounter();

  while (isInLoop.load()) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_WINDOW_RESIZED)
        SDL_GetWindowSize(window, &width, &height);

      if (e.type == SDL_EVENT_QUIT) {
        haltLoop();
        SDL_WaitThread(ctrThread, nullptr);
        return;
      }

      dispatchEvent(e);
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    const float deltaTime = static_cast<float>(
      (now - lastCounter) * 1000.0 / SDL_GetPerformanceFrequency()
    );
    lastCounter = now;
    const float time = static_cast<float>(SDL_GetTicks());

#if defined(__APPLE__) && defined(__MACH__)
    {
      std::lock_guard lock(nextMutex);
      if (nextPtr) {
        std::lock_guard rlock(rootMutex);
        if (rootPtr) rootPtr->iDestroy();
        nextPtr->iPreMount(this);
        rootPtr = nextPtr;
        nextPtr = nullptr;
        loading.store(true, std::memory_order_release);
      }
    }
#else
    if (auto next = nextPtr.exchange(nullptr, std::memory_order_acq_rel)) {
      if (const auto root = rootPtr.load(std::memory_order_acquire))
        root->iDestroy();
      next->iPreMount(this);
      rootPtr.store(next, std::memory_order_release);
      loading.store(true, std::memory_order_release);
    }
#endif

    if (loading.load(std::memory_order_acquire))
      updateLoadingScreen(deltaTime, time);
    else {
      update(deltaTime, time);

#if defined(__APPLE__) && defined(__MACH__)
      {
        std::lock_guard lock(rootMutex);
        if (rootPtr) rootPtr->iSwapRenderState();
      }
#else
      if (const auto root = rootPtr.load(std::memory_order_acquire))
        root->iSwapRenderState();
#endif
    }

    SDL_Delay(1);
  }
}

void Container::haltLoop() {
  isInLoop.store(false, std::memory_order_release);
}

void Container::updateCursor(const Cursor cursor) {
  if (cursor == currentCursor || cursor == Cursor::INHERIT) return;
  currentCursor = cursor;

  SDL_SystemCursor sdlCursor;
  switch (cursor) {
    case Cursor::POINTER: sdlCursor = SDL_SYSTEM_CURSOR_POINTER; break;
    case Cursor::TEXT: sdlCursor = SDL_SYSTEM_CURSOR_TEXT; break;
    case Cursor::CROSSHAIR: sdlCursor = SDL_SYSTEM_CURSOR_CROSSHAIR; break;
    case Cursor::WAIT: sdlCursor = SDL_SYSTEM_CURSOR_WAIT; break;
    default: sdlCursor = SDL_SYSTEM_CURSOR_DEFAULT; break;
  }

  if (currentSDLCursor) SDL_DestroyCursor(currentSDLCursor);
  currentSDLCursor = SDL_CreateSystemCursor(sdlCursor);
  SDL_SetCursor(currentSDLCursor);
}

} // namespace hic
