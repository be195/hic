#include "shader.hpp"
#include <SDL3/SDL.h>
#include "../utils/logging.hpp"

#define HIC_SHADER_ENTRYPOINT "main"

namespace hic::Assets {

Shader::~Shader() {
  if (data) {
    SDL_free(data);
    data = nullptr;
  }

  if (renderState) {
    SDL_DestroyGPURenderState(renderState);
    renderState = nullptr;
  }
}

void Shader::preload() {
  size_t loadedDataSize;
  // ReSharper disable once CppTooWideScope
  void* loadedData = SDL_LoadFile(("shaders/" + fileName + ".spv").c_str(), &loadedDataSize);

  if (loadedData) {
    read = true;
    data = loadedData;
    dataSize = loadedDataSize;
  }
}

void Shader::use(SDL_Renderer *renderer) {
  if (!read || !data || renderState) return;

  SDL_GPUDevice* device = SDL_GetGPURendererDevice(renderer);

  SDL_GPUShaderCreateInfo shaderInfo = {};
  shaderInfo.code_size = dataSize;
  shaderInfo.code = static_cast<const Uint8*>(data);
  shaderInfo.entrypoint = HIC_SHADER_ENTRYPOINT;
  shaderInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
  shaderInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;

  SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);

  SDL_GPURenderStateCreateInfo stateInfo = {};
  stateInfo.fragment_shader = shader;

  renderState = SDL_CreateGPURenderState(renderer, &stateInfo);

  SDL_free(data);
  data = nullptr;
  SDL_ReleaseGPUShader(device, shader);
}

void Shader::push(SDL_Renderer* renderer) const {
  if (renderState)
    SDL_SetGPURenderState(renderer, renderState);
}

void Shader::pop(SDL_Renderer* renderer) {
  SDL_SetGPURenderState(renderer, nullptr);
}

}
