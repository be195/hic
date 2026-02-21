#include "basecomponent.hpp"
#include "container.hpp"
#include "assets/shader.hpp"
#include <SDL3/SDL.h>

#include <utility>

#ifdef HIC_USE_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#endif

namespace hic {

Container::Container(SDL_Window* window, SDL_Renderer* renderer) : window(window), renderer(renderer) {
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
  free(imguiIo);
#endif

  if (const auto root = rootPtr.load(std::memory_order_acquire))
    root->iDestroy();
  if (currentSDLCursor) SDL_DestroyCursor(currentSDLCursor);

  Assets::GPUShader::cleanupTexturePool();
}

void Container::setRoot(const std::shared_ptr<BaseComponent> &newRoot) {
  nextPtr.store(newRoot, std::memory_order_release);
}

void Container::setRoot(const std::string &name) {
  if (const auto it = roots.find(name); it != roots.end())
    setRoot(it->second);
}

void Container::define(const std::string &name, const std::shared_ptr<BaseComponent> &newRoot) {
  if (!newRoot)
    return; // probably throw an error here
  roots.insert({name, newRoot});
}

void Container::update(const float deltaTime, const float time) const {
  if (const auto root = rootPtr.load(std::memory_order_acquire))
    root->iUpdate(deltaTime, time);
}

void Container::render(const float time) const {
  if (const auto root = rootPtr.load(std::memory_order_acquire))
    root->iRender(renderer, time, root->boundingRect.pos(), nullptr, {0, 0});
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

void Container::handleEvent(const SDL_Event& e) {
#ifdef HIC_USE_IMGUI
  ImGui_ImplSDL3_ProcessEvent(&e);
#endif

  auto root = rootPtr.load(std::memory_order_acquire);
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

    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      root->iHandleKeyboardEvent(e);
      break;

    case SDL_EVENT_QUIT:
      if (ctrThread) {
        haltLoop();
        SDL_WaitThread(ctrThread, nullptr);
      }
      break;

    default: break;
  }
}

int Container::ctrThreadFunc(void *data) {
  const auto container = static_cast<Container*>(data);
  container->ctrThreadLoop();
  return 0;
}

void Container::ctrThreadLoop() {
  while (isInLoop.load(std::memory_order_acquire)) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
#ifdef HIC_USE_IMGUI
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
#endif
    if (logicalResDirty.exchange(false, std::memory_order_acq_rel) && lWidth != 0 && lHeight != 0)
      SDL_SetRenderLogicalPresentation(renderer, lWidth, lHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    if (auto next = nextPtr.exchange(nullptr, std::memory_order_acq_rel)) {
      if (const auto root = rootPtr.load(std::memory_order_acquire))
        root->iDestroy();
      if (next)
        next->iPreMount(this);

      rootPtr.store(next, std::memory_order_release);
      loading.store(true, std::memory_order_release);
    }

    const auto nowCounter = SDL_GetPerformanceCounter();
    const double deltaTime = (nowCounter - lastCounterTime) * 1000 / static_cast<float>(SDL_GetPerformanceFrequency());
    const auto time = SDL_GetTicks();

    if (loading.load(std::memory_order_acquire)) {
      assetManager->processReady(renderer);

      const int pending = assetManager->getPendingCount();
      const int ready = assetManager->getReadyCount();
      const bool isLoading = assetManager->isLoading();

      updateLoadingScreen(deltaTime, time);
      renderLoadingScreen(pending, ready);

      if (pending == 0 && ready == 0 && !isLoading) {
        loading.store(false, std::memory_order_release);
        if (const auto root = rootPtr.load(std::memory_order_acquire))
          root->iPostMount();
      }
    } else {
      update(deltaTime, time);
      render(time);
    }

    lastCounterTime = nowCounter;
    SDL_Delay(1);

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

  ctrThread = SDL_CreateThread(ctrThreadFunc, "hic::Container<Container>", this);
  if (!ctrThread) {
    isInLoop.store(false);
    return;
  }

  while (isInLoop.load()) {
    SDL_Event e;
    while (SDL_PollEvent(&e))
      handleEvent(e);
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
