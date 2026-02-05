#pragma once

#include "base.hpp"
#include "../utils/hicapi.hpp"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <string>

namespace hic::Assets {

class HIC_API GPUShader : public Base {
public:
  struct Config {
    std::vector<SDL_GPUVertexBufferDescription> vertexBuffers;
    std::vector<SDL_GPUVertexAttribute> vertexAttributes;

    SDL_GPUFillMode fillMode = SDL_GPU_FILLMODE_FILL;
    SDL_GPUCullMode cullMode = SDL_GPU_CULLMODE_NONE;
    SDL_GPUFrontFace frontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    bool blendEnable = true;
    SDL_GPUBlendFactor srcColorBlend = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    SDL_GPUBlendFactor dstColorBlend = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    SDL_GPUBlendOp colorBlendOp = SDL_GPU_BLENDOP_ADD;
    SDL_GPUBlendFactor srcAlphaBlend = SDL_GPU_BLENDFACTOR_ONE;
    SDL_GPUBlendFactor dstAlphaBlend = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    SDL_GPUBlendOp alphaBlendOp = SDL_GPU_BLENDOP_ADD;

    bool depthTestEnable = false;
    bool depthWriteEnable = false;
    SDL_GPUCompareOp depthCompareOp = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    SDL_GPUPrimitiveType primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    SDL_GPUTextureFormat colorTargetFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    SDL_GPUTextureFormat depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

    uint32_t vertexUniformBuffers = 0;
    uint32_t fragmentUniformBuffers = 0;
    uint32_t vertexSamplers = 0;
    uint32_t fragmentSamplers = 0;

    std::vector<float> vertexData;
    std::vector<uint16_t> indexData;
    bool useIndexBuffer = false;
  };

  GPUShader(std::string vertexFile, std::string fragmentFile, Config config);
  ~GPUShader() override;

  void preload() override;
  void use(SDL_Renderer* renderer) override;
  void begin(SDL_Renderer* renderer, int width, int height);
  void end();

  template<typename T>
  void setVertexUniform(const uint32_t slot, const T& data) {
    if (commandBuffer)
      SDL_PushGPUVertexUniformData(commandBuffer, slot, &data, sizeof(T));
  }

  template<typename T>
  void setFragmentUniform(const uint32_t slot, const T& data) {
    if (commandBuffer)
      SDL_PushGPUFragmentUniformData(commandBuffer, slot, &data, sizeof(T));
  }

  void bindFragmentTexture(uint32_t slot, SDL_GPUTexture* texture) const;
  void bindVertexTexture(uint32_t slot, SDL_GPUTexture* texture) const;

  void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0) const;
  void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0) const;

  void bindVertexBuffer(SDL_GPUBuffer* buffer, uint32_t slot = 0, uint32_t offset = 0) const;
  void bindIndexBuffer(SDL_GPUBuffer* buffer, SDL_GPUIndexElementSize elementSize = SDL_GPU_INDEXELEMENTSIZE_16BIT) const;

  void bindBuffers() const;

  std::string getCacheKey() const override {
    return "sh#" + vertexFileName + "#" + fragmentFileName;
  }

private:
  std::string vertexFileName;
  std::string fragmentFileName;
  Config config;

  SDL_GPUGraphicsPipeline* pipeline = nullptr;
  SDL_GPUDevice* device = nullptr;
  SDL_Renderer* activeRenderer = nullptr;

  void* vertexData = nullptr;
  void* fragmentData = nullptr;
  size_t vertexDataSize = 0;
  size_t fragmentDataSize = 0;
  bool loaded = false;

  SDL_GPUCommandBuffer* commandBuffer = nullptr;
  SDL_GPURenderPass* renderPass = nullptr;
  SDL_GPUSampler* defaultSampler = nullptr;
  static SDL_Texture* bridgeTexture;
  static SDL_GPUTexture* gpuHandle;

  // TODO: bool option for instance texture and gputexture, otherwise
  // we would have to call SDL_FlushRenderer
  static void initBridge(SDL_Renderer *r);
  bool createPipeline();
  void createDefaultSampler();

  SDL_GPUBuffer* vertexBuffer = nullptr;
  SDL_GPUBuffer* indexBuffer = nullptr;
  void createBuffers(const std::vector<float>& vertices, const std::vector<uint16_t>& indices);
};

namespace ShaderPresets {
  inline GPUShader::Config sprite2D() {
    GPUShader::Config cfg{};

    cfg.vertexAttributes = {
      {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0},
      {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 12}
    };

    cfg.vertexBuffers = {
      {0, 20, SDL_GPU_VERTEXINPUTRATE_VERTEX, 0}
    };

    cfg.blendEnable = true;
    cfg.srcColorBlend = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    cfg.dstColorBlend = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    cfg.colorBlendOp = SDL_GPU_BLENDOP_ADD;
    cfg.srcAlphaBlend = SDL_GPU_BLENDFACTOR_ONE;
    cfg.dstAlphaBlend = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    cfg.alphaBlendOp = SDL_GPU_BLENDOP_ADD;

    cfg.depthTestEnable = false;
    cfg.depthWriteEnable = false;

    cfg.fillMode = SDL_GPU_FILLMODE_FILL;
    cfg.cullMode = SDL_GPU_CULLMODE_NONE;
    cfg.frontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    cfg.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    cfg.colorTargetFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    cfg.depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

    cfg.vertexUniformBuffers = 1;
    cfg.fragmentUniformBuffers = 1;
    cfg.fragmentSamplers = 1;

    return cfg;
  }

  inline GPUShader::Config fullscreenQuad() {
    GPUShader::Config cfg{};

    cfg.vertexAttributes = {
      {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0}
    };
    cfg.vertexBuffers = {
      {0, 8, SDL_GPU_VERTEXINPUTRATE_VERTEX, 0}
    };

    cfg.blendEnable = false;
    cfg.fragmentUniformBuffers = 1;
    cfg.fragmentSamplers = 1;

    return cfg;
  }
}

}