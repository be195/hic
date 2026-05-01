#pragma once

#include "base.hpp"
#include "../utils/hicapi.hpp"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <string>
#include <vector>

#define HIC_SHADER_ENTRYPOINT "main"

#if defined(_WIN32) || defined(__CYGWIN__)
#define HIC_GPUSHADER_EXT ".dxil"
#define HIC_GPUSHADER_FORMAT SDL_GPU_SHADERFORMAT_DXIL
#elif defined(__APPLE__)
#define HIC_SHADER_ENTRYPOINT "main0" // hardcoded behavior in SDL_shadercross
#define HIC_GPUSHADER_EXT ".msl"
#define HIC_GPUSHADER_FORMAT SDL_GPU_SHADERFORMAT_MSL
#else
#define HIC_GPUSHADER_EXT ".spv"
#define HIC_GPUSHADER_FORMAT SDL_GPU_SHADERFORMAT_SPIRV
#endif

#if defined(HIC_FORCE_GPUSHADER_FORMAT) && defined(HIC_FORCE_GPUSHADER_EXT)
#define HIC_GPUSHADER_EXT HIC_FORCE_GPUSHADER_EXT
#define HIC_GPUSHADER_FORMAT HIC_FORCE_GPUSHADER_FORMAT
#endif

#ifndef HIC_SHADER_BRIDGE_PIXEL_FORMAT
#define HIC_SHADER_BRIDGE_PIXEL_FORMAT SDL_PIXELFORMAT_BGRA32
#endif

#ifndef HIC_SHADER_ATLAS_SIZE
#define HIC_SHADER_ATLAS_SIZE 512
#endif

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

  GPUShader(std::string vertexFile, std::string fragmentFile, Config config, bool useGlobal);
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

  std::shared_ptr<Base> createInstance() override;
  static void cleanupTexturePool();

private:
  std::string vertexFileName;
  std::string fragmentFileName;
  Config config;

  struct TextureInfo {
    SDL_Texture* bridgeTexture;
    SDL_GPUTexture* gpuHandle;

    ~TextureInfo() {
      if (bridgeTexture) SDL_DestroyTexture(bridgeTexture);
    }
  };

  int bridgeW = 0, bridgeH = 0;

  static std::vector<TextureInfo*> texturePool;
  static SDL_Mutex* texturePoolMutex;
  static TextureInfo* acquireBridgeTexture(SDL_Renderer* r);
  static void releaseBridgeTexture(TextureInfo* info);

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
  TextureInfo* bridgeTextureInfo = nullptr;
  bool useGlobalTexture = true;
  bool texturesReady = false;

  // non-null when this object is a per-instance clone
  std::shared_ptr<GPUShader> parent;

  void initBridge(SDL_Renderer *r);
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