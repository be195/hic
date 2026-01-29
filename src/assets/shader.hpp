#pragma once

#include "base.hpp"
#include "../utils/hicapi.hpp"
#include <SDL3/SDL.h>

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
  void push(SDL_Renderer* renderer, const Options& options) const;

  std::string getCacheKey() const override { return "sh#" + fileName; }
private:
  SDL_GPURenderState* renderState = nullptr;

  void* data = nullptr;
  size_t dataSize = 0;
  bool read = false;
};

}
