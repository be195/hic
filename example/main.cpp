#include <hic.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>

class TestComponent2 : public hic::BaseComponent {
public:
  void mounted() override {
    this->boundingRect.setW(40);
    this->boundingRect.setH(40);
    this->boundingRect.setY(70);
  };

  void render(SDL_Renderer *r, float time) override {
    SDL_SetRenderDrawColor(r, 0, 255, 0, 255);

    const SDL_FRect rect = { 0, 0, this->boundingRect.w(), this->boundingRect.h() };
    SDL_RenderFillRect(r, &rect);
  };

  void update(float deltaTime, float time) override {
    this->boundingRect.setX(100 + sin(time / 1000 * M_PI * 2) * 20);
  };

  hic::Cursor handleMouseEvent(const SDL_Event &e, float x, float y) override {
    return hic::Cursor::WAIT;
  };
};

class TestComponent : public hic::BaseComponent {
  public:
    void mounted() override {
      this->boundingRect.setW(200);
      this->boundingRect.setH(200);

      const std::shared_ptr<BaseComponent> a = std::make_shared<TestComponent2>();
      this->addChild(a);
    };

    void render(SDL_Renderer *r, float time) override {
      SDL_SetRenderDrawColor(r, 255, 0, 0, 255);

      const SDL_FRect rect = { 0, 0, this->boundingRect.w(), this->boundingRect.h() };
      SDL_RenderFillRect(r, &rect);
    };

    hic::Cursor handleMouseEvent(const SDL_Event &e, float x, float y) override {
      return hic::Cursor::INHERIT;
    };
};

int main(const int argc, char* argv[]) {
  if (hic::watchdog(argc, argv))
    return 0;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "HIC Test", 800, 600, 0
  );

  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

  const std::shared_ptr<hic::BaseComponent> test = std::make_shared<TestComponent>();
  test.get()->clip = false;

  auto container = hic::Container(window, renderer);
  container.setRoot(test);
  container.startLoop();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}