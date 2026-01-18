#pragma once

#include "basecomponent.hpp"
#include <SDL3/SDL.h>
#include <memory>

namespace hic {

class Container {
public:
  Container(SDL_Window* window, SDL_Renderer* renderer);
  ~Container();

  void update(float deltaTime, float time) const;
  void render(float time) const;
  void handleEvent(const SDL_Event& e);

  void setRoot(std::shared_ptr<BaseComponent> newRoot);

  SDL_Window* getWindow() const { return window; }
  SDL_Renderer* getRenderer() const { return renderer; }
  void startLoop();
  void haltLoop();

  int getWidth() const { return width; }
  int getHeight() const { return height; }

private:
  SDL_Window* window;
  SDL_Renderer* renderer;
  std::shared_ptr<BaseComponent> root;

  int width{}, height{};
  Uint8 lastCounterTime = 0;

  bool is_in_loop = false;

  Cursor currentCursor = Cursor::DEFAULT;
  void updateCursor(Cursor cursor);
};

} // namespace hic