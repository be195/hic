#pragma once

#include "basecomponent.hpp"
#include <atomic>
#include <unordered_map>
#include <SDL3/SDL.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "assets/audio.hpp"
#include "assets/manager.hpp"
#include "audio/manager.hpp"
#include "utils/hicapi.hpp"

#ifdef HIC_USE_IMGUI
#include <imgui.h>
#endif

namespace hic {

class HIC_API Container {
public:
  Container(SDL_Window* window, SDL_Renderer* renderer);
  virtual ~Container();

#ifdef HIC_USE_IMGUI
  ImGuiIO* imguiIo;
  ImGuiContext* imguiContext;
#endif

  void setRoot(const std::shared_ptr<BaseComponent> &newRoot);
  void setRoot(const std::string& name);

  SDL_Window* getWindow() const { return window; }
  SDL_Renderer* getRenderer() const { return renderer; }
  void startLoop();
  void haltLoop();

  int getWidth() const { return width; }
  int getHeight() const { return height; }
  int getLogicalWidth() const { return lWidth; }
  int getLogicalHeight() const { return lHeight; }
  int setLogicalWidth(int newWidth);
  int setLogicalHeight(int newHeight);

  virtual void renderLoadingScreen(const int pending, const int ready) {};
  virtual void updateLoadingScreen(float deltaTime, float time) {};

  void define(const std::string& name, const std::shared_ptr<BaseComponent> &newRoot);

  Assets::Manager* getAssetManager() const { return assetManager.get(); }
  Audio::Manager* getAudioManager() const { return audioManager.get(); }
  std::atomic<bool> loading = {false};

  static int ctrThreadFunc(void* data);
  void ctrThreadLoop();
  SDL_Thread* ctrThread = nullptr;

  virtual void update(float deltaTime, float time) const;
  virtual void render(float time) const;

  void dispatchEvent(const SDL_Event& e);

  SDL_Window* window;
  SDL_Renderer* renderer;
  std::unordered_map<std::string, std::shared_ptr<BaseComponent>> roots;
#if defined(__APPLE__) && defined(__MACH__)
  std::shared_ptr<BaseComponent> rootPtr;
  std::shared_ptr<BaseComponent> nextPtr;
  mutable std::mutex rootMutex;
  mutable std::mutex nextMutex;
#else
  std::atomic<std::shared_ptr<BaseComponent>> rootPtr;
  std::atomic<std::shared_ptr<BaseComponent>> nextPtr;
#endif
  std::unique_ptr<Audio::Manager> audioManager;
  std::unique_ptr<Assets::Manager> assetManager;
  SDL_Cursor* currentSDLCursor = nullptr;

  int width{}, height{}, lWidth{}, lHeight{};
  Uint64 lastCounterTime = 0;

  std::atomic<bool> isInLoop = {false};
  std::atomic<bool> logicalResDirty = {false};

  Cursor currentCursor = Cursor::DEFAULT;
  void updateCursor(Cursor cursor);
};

} // namespace hic
