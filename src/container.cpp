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

void GPUGC::enqueue(SDL_GPUDevice* device, SDL_GPUBuffer* buffer) {
  if (!buffer) return;
  std::lock_guard lock(mutex);
  resources.push_back({ Resource::BUFFER, device, buffer });
}

void GPUGC::enqueue(SDL_GPUDevice* device, SDL_GPUTexture* texture) {
  if (!texture) return;
  std::lock_guard lock(mutex);
  resources.push_back({ Resource::TEXTURE, device, texture });
}

void GPUGC::enqueue(SDL_GPUDevice* device, SDL_GPUSampler* sampler) {
  if (!sampler) return;
  std::lock_guard lock(mutex);
  resources.push_back({ Resource::SAMPLER, device, sampler });
}

void GPUGC::enqueue(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline) {
  if (!pipeline) return;
  std::lock_guard lock(mutex);
  resources.push_back({ Resource::PIPELINE, device, pipeline });
}

void GPUGC::enqueue(SDL_GPUDevice* device, SDL_GPUShader* shader) {
  if (!shader) return;
  std::lock_guard lock(mutex);
  resources.push_back({ Resource::SHADER, device, shader });
}

void GPUGC::enqueue(SDL_GPUDevice* device, SDL_GPUTransferBuffer* transferBuffer) {
  if (!transferBuffer) return;
  std::lock_guard lock(mutex);
  resources.push_back({ Resource::TRANSFER_BUFFER, device, transferBuffer });
}

void GPUGC::collect() {
  std::vector<Resource> toDelete;
  {
    std::lock_guard lock(mutex);
    if (resources.empty()) return;
    toDelete.swap(resources);
  }

  for (const auto& res : toDelete) {
    switch (res.type) {
      case Resource::BUFFER:          SDL_ReleaseGPUBuffer(res.device, static_cast<SDL_GPUBuffer*>(res.handle)); break;
      case Resource::TEXTURE:         SDL_ReleaseGPUTexture(res.device, static_cast<SDL_GPUTexture*>(res.handle)); break;
      case Resource::SAMPLER:         SDL_ReleaseGPUSampler(res.device, static_cast<SDL_GPUSampler*>(res.handle)); break;
      case Resource::PIPELINE:        SDL_ReleaseGPUGraphicsPipeline(res.device, static_cast<SDL_GPUGraphicsPipeline*>(res.handle)); break;
      case Resource::SHADER:          SDL_ReleaseGPUShader(res.device, static_cast<SDL_GPUShader*>(res.handle)); break;
      case Resource::TRANSFER_BUFFER: SDL_ReleaseGPUTransferBuffer(res.device, static_cast<SDL_GPUTransferBuffer*>(res.handle)); break;
    }
  }
}

void GPUGC::collectAll() {
  collect();
}

Container::Container(SDL_Window* window, SDL_Renderer* renderer)
  : window(window), renderer(renderer)
{
  GPUGC::makeCurrent(&gc);
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

  rootPtr.store(nullptr, std::memory_order_release);
  nextPtr.store(nullptr, std::memory_order_release);
  roots.clear();
  assetManager.reset();

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
  if (gameBuffer) SDL_DestroyTexture(gameBuffer);

  Assets::GPUShader::cleanupTexturePool();
  gc.collectAll();
  GPUGC::makeCurrent(nullptr);
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
  if (!renderer) return;

#if defined(__APPLE__) && defined(__MACH__)
  std::lock_guard lock(rootMutex);
  if (const auto root = rootPtr)
#else
  if (const auto root = rootPtr.load(std::memory_order_acquire))
#endif
    root->iRender(renderer, time, root->activeRS.rect.fpos(), nullptr, {0, 0});
}

void Container::dispatchEvent(const SDL_Event& e) {
  if (!window || !renderer) return;

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
      if (lWidth == 0 || lHeight == 0) break;

      const int scaleX = width  / lWidth;
      const int scaleY = height / lHeight;
      const int scale = std::min(scaleX, scaleY);

      if (scale == 0) break;

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
#ifdef HIC_USE_IMGUI
  ImGui::SetCurrentContext(imguiContext);
#endif
  while (isInLoop.load(std::memory_order_acquire)) {
    gc.collect();

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

    int w, h;
    SDL_GetRenderOutputSize(renderer, &w, &h);
    if (w <= 0 || h <= 0) {
      SDL_Delay(10);
      continue;
    }

#ifdef HIC_USE_IMGUI
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
#endif

    if (logicalResDirty.exchange(false, std::memory_order_acq_rel) || !gameBuffer) {
      if (gameBuffer) SDL_DestroyTexture(gameBuffer);
      if (lWidth > 0 && lHeight > 0) {
        gameBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, lWidth, lHeight);
        if (gameBuffer) SDL_SetTextureScaleMode(gameBuffer, SDL_SCALEMODE_NEAREST);
      }

      SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    }

    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderViewport(renderer, nullptr);
    SDL_SetRenderClipRect(renderer, nullptr);

    if (gameBuffer) {
      SDL_SetRenderTarget(renderer, gameBuffer);
      SDL_SetRenderViewport(renderer, nullptr);
      SDL_SetRenderClipRect(renderer, nullptr);
    }
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

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
    } else {
      render(SDL_GetTicks());
    }

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderViewport(renderer, nullptr);
    SDL_SetRenderClipRect(renderer, nullptr);
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (gameBuffer && lWidth > 0 && lHeight > 0) {
      const float scaleX = static_cast<float>(w) / lWidth;
      const float scaleY = static_cast<float>(h) / lHeight;
      const float scale = std::min(scaleX, scaleY);
      
      const float gw = lWidth * scale;
      const float gh = lHeight * scale;
      const SDL_FRect dstRect = { (w - gw) / 2.0f, (h - gh) / 2.0f, gw, gh };
      
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_RenderTexture(renderer, gameBuffer, nullptr, &dstRect);
    }

#ifdef HIC_USE_IMGUI
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
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
