#pragma once

#include "base.hpp"
#include "../utils/hicapi.hpp"
#include <SDL3/SDL.h>
#include "../utils/logging.hpp"

namespace hic::Assets {

class HIC_API Shader : public Base {
public:
  std::string fileName;

  explicit Shader(std::string fileName) : fileName(std::move(fileName)) {}
  ~Shader() override;

  void preload() override;
  void use(SDL_Renderer* renderer) override;
  void push(SDL_Renderer* renderer) const;
  static void pop(SDL_Renderer* renderer);

  template<typename Options>
  void push(SDL_Renderer* renderer, const Options& options) const {
    if (sizeof(Options) % 16 != 0) {
      HICL("Shader").error("push Options struct must be 16-byte aligned");
      return;
    }

    if (renderState) {
      SDL_SetGPURenderState(renderer, renderState);
      SDL_PushGPUFragmentUniformData(renderer, 0, &options, sizeof(options));
    }
  };

  std::string getCacheKey() const override { return "sh#" + fileName; }
private:
  SDL_GPURenderState* renderState = nullptr;

  void* data = nullptr;
  size_t dataSize = 0;
  bool read = false;
};

}
