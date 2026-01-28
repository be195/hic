#pragma once

#include "basecomponent.hpp"
#include <SDL3/SDL.h>
#include <memory>

#include "assets/audio.hpp"
#include "assets/manager.hpp"
#include "audio/manager.hpp"
#include "utils/hicapi.hpp"

namespace hic {

class HIC_API Container {
public:
  Container(SDL_Window* window, SDL_Renderer* renderer);
  virtual ~Container();

  void update(float deltaTime, float time) const;
  void render(float time) const;
  void handleEvent(const SDL_Event& e);

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

  Assets::Manager* getAssetManager() const { return assetManager; }
  Audio::Manager* getAudioManager() const { return audioManager; }
private:
  bool loading = false;

  SDL_Window* window;
  SDL_Renderer* renderer;
  std::unordered_map<std::string, std::shared_ptr<BaseComponent>> roots;
  std::shared_ptr<BaseComponent> root;
  std::shared_ptr<BaseComponent> next;
  Audio::Manager* audioManager;
  Assets::Manager* assetManager;
  SDL_Cursor* currentSDLCursor = nullptr;

  int width{}, height{}, lWidth{}, lHeight{};
  Uint64 lastCounterTime = 0;

  bool is_in_loop = false;
  bool logical_res_dirty = false;

  Cursor currentCursor = Cursor::DEFAULT;
  void updateCursor(Cursor cursor);
};

} // namespace hic