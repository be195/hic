#include "shader.hpp"
#include "../container.hpp"
#include "../utils/logging.hpp"

namespace hic::Assets {

GPUShader::GPUShader(std::string vertexFile, std::string fragmentFile, Config config, bool useGlobal)
  : vertexFileName(std::move(vertexFile))
  , fragmentFileName(std::move(fragmentFile))
  , config(std::move(config))
  , useGlobalTexture(useGlobal) {}

std::shared_ptr<Base> GPUShader::createInstance() {
  // instances always get their own bridge texture (useGlobal=false) so that
  // each consumer renders into an independent SDL texture. sharing the global
  // bridge texture across multiple begin()/end() cycles would cause the earlier
  // renders to be clobbered before the SDL renderer batch is flushed.
  auto instance = std::make_shared<GPUShader>(vertexFileName, fragmentFileName, config, false);
  instance->parent = std::static_pointer_cast<GPUShader>(shared_from_this());
  instance->loaded = true;
  return instance;
}

GPUShader::~GPUShader() {
  HICL("GPUShader").debug("destroying shader:", vertexFileName, fragmentFileName, "is child:", !!parent);

  if (vertexData) SDL_free(vertexData);
  if (fragmentData) SDL_free(fragmentData);

  if (bridgeTextureInfo)
    releaseBridgeTexture(bridgeTextureInfo);

  // GPU resources are owned by the original (non-instance) shader; enqueuing them in GC
  // ensures they are not released while still in use by the render thread.
  if (!parent && device) {
    if (auto gc = GPUGC::get()) {
      if (defaultSampler) gc->enqueue(device, defaultSampler);
      if (pipeline)       gc->enqueue(device, pipeline);
      if (vertexBuffer)   gc->enqueue(device, vertexBuffer);
      if (indexBuffer)    gc->enqueue(device, indexBuffer);
    } else {
      if (defaultSampler) SDL_ReleaseGPUSampler(device, defaultSampler);
      if (pipeline)       SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
      if (vertexBuffer)   SDL_ReleaseGPUBuffer(device, vertexBuffer);
      if (indexBuffer)    SDL_ReleaseGPUBuffer(device, indexBuffer);
    }
  }
}

void GPUShader::createBuffers(const std::vector<float>& vertices, const std::vector<uint16_t>& indices) {
  if (!vertices.empty()) {
    SDL_GPUBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = vertices.size() * sizeof(float);

    vertexBuffer = SDL_CreateGPUBuffer(device, &bufferInfo);

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = bufferInfo.size;

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    void* mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    SDL_memcpy(mapped, vertices.data(), bufferInfo.size);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);

    SDL_GPUTransferBufferLocation src{};
    src.transfer_buffer = transferBuffer;
    src.offset = 0;

    SDL_GPUBufferRegion dst{};
    dst.buffer = vertexBuffer;
    dst.offset = 0;
    dst.size = bufferInfo.size;

    SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmd);
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
  }

  if (!indices.empty()) {
    SDL_GPUBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bufferInfo.size = indices.size() * sizeof(uint16_t);

    indexBuffer = SDL_CreateGPUBuffer(device, &bufferInfo);

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = bufferInfo.size;

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    void* mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    SDL_memcpy(mapped, indices.data(), bufferInfo.size);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);

    SDL_GPUTransferBufferLocation src{};
    src.transfer_buffer = transferBuffer;
    src.offset = 0;

    SDL_GPUBufferRegion dst{};
    dst.buffer = indexBuffer;
    dst.offset = 0;
    dst.size = bufferInfo.size;

    SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmd);
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
  }
}

void GPUShader::preload() {
  size_t vSize, fSize;

  vertexData = SDL_LoadFile(("shaders/vt_" + vertexFileName + HIC_GPUSHADER_EXT).c_str(), &vSize);
  if (!vertexData) {
    HICL("GPUShader").error("failed to load vertex shader:", vertexFileName);
    return;
  }
  vertexDataSize = vSize;

  fragmentData = SDL_LoadFile(("shaders/fr_" + fragmentFileName + HIC_GPUSHADER_EXT).c_str(), &fSize);
  if (!fragmentData) {
    HICL("GPUShader").error("failed to load fragment shader:", fragmentFileName);
    SDL_free(vertexData);
    vertexData = nullptr;
    return;
  }
  fragmentDataSize = fSize;

  loaded = true;
}

void GPUShader::use(SDL_Renderer* renderer) {
  // instances (clones) borrow all GPU resources from their parent; skip
  // pipeline creation entirely. the pipeline pointer will be valid once the
  // parent shader has been processed by the asset manager.
  if (parent) return;
  if (!loaded || pipeline) return;

  device = SDL_GetGPURendererDevice(renderer);
  if (!device) {
    HICL("GPUShader").error("Failed to get GPU device");
    return;
  }

  loaded = false;
  createPipeline();
  createDefaultSampler();
  createBuffers(config.vertexData, config.indexData);
}

void GPUShader::initBridge(SDL_Renderer *r) {
  if (texturesReady) return;

  const auto textureInfo = acquireBridgeTexture(r);
  if (!textureInfo) {
    HICL("GPUShader").error("Failed to acquire bridge texture");
    return;
  }

  bridgeTextureInfo = textureInfo;
  texturesReady = true;
}

bool GPUShader::createPipeline() {
  SDL_GPUShaderCreateInfo vertInfo{};
  vertInfo.code_size = vertexDataSize;
  vertInfo.code = static_cast<const Uint8*>(vertexData);
  vertInfo.entrypoint = HIC_SHADER_ENTRYPOINT;
  vertInfo.format = HIC_GPUSHADER_FORMAT;
  vertInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  vertInfo.num_uniform_buffers = config.vertexUniformBuffers;
  vertInfo.num_samplers = config.vertexSamplers;

  SDL_GPUShader* vertShader = SDL_CreateGPUShader(device, &vertInfo);
  if (!vertShader) {
    HICL("GPUShader").error("failed to create vertex shader");
    HICL("GPUShader").error(SDL_GetError());
    return false;
  }

  SDL_GPUShaderCreateInfo fragInfo{};
  fragInfo.code_size = fragmentDataSize;
  fragInfo.code = static_cast<const Uint8*>(fragmentData);
  fragInfo.entrypoint = HIC_SHADER_ENTRYPOINT;
  fragInfo.format = HIC_GPUSHADER_FORMAT;
  fragInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  fragInfo.num_uniform_buffers = config.fragmentUniformBuffers;
  fragInfo.num_samplers = config.fragmentSamplers;

  SDL_GPUShader* fragShader = SDL_CreateGPUShader(device, &fragInfo);
  if (!fragShader) {
    HICL("GPUShader").error("failed to create fragment shader");
    HICL("GPUShader").error(SDL_GetError());
    SDL_ReleaseGPUShader(device, vertShader);
    return false;
  }

  SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};

  SDL_GPUVertexInputState vertexInput{};
  if (config.vertexBuffers.empty()) {
    vertexInput.vertex_buffer_descriptions = nullptr;
    vertexInput.num_vertex_buffers = 0;
  } else {
    vertexInput.vertex_buffer_descriptions = config.vertexBuffers.data();
    vertexInput.num_vertex_buffers = config.vertexBuffers.size();
  }

  if (config.vertexAttributes.empty()) {
    vertexInput.vertex_attributes = nullptr;
    vertexInput.num_vertex_attributes = 0;
  } else {
    vertexInput.vertex_attributes = config.vertexAttributes.data();
    vertexInput.num_vertex_attributes = config.vertexAttributes.size();
  }
  pipelineInfo.vertex_input_state = vertexInput;

  pipelineInfo.primitive_type = config.primitiveType;
  pipelineInfo.vertex_shader = vertShader;
  pipelineInfo.fragment_shader = fragShader;

  SDL_GPURasterizerState rasterizer{};
  rasterizer.fill_mode = config.fillMode;
  rasterizer.cull_mode = config.cullMode;
  rasterizer.front_face = config.frontFace;
  rasterizer.enable_depth_clip = true;
  pipelineInfo.rasterizer_state = rasterizer;

  SDL_GPUMultisampleState multisample{};
  multisample.sample_count = SDL_GPU_SAMPLECOUNT_1;
  multisample.sample_mask = 0;
  pipelineInfo.multisample_state = multisample;

  SDL_GPUDepthStencilState depthStencil{};
  depthStencil.enable_depth_test = config.depthTestEnable;
  depthStencil.enable_depth_write = config.depthWriteEnable;
  depthStencil.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
  depthStencil.enable_stencil_test = false;
  pipelineInfo.depth_stencil_state = depthStencil;

  SDL_GPUColorTargetBlendState blendState{};
  blendState.enable_blend = config.blendEnable;
  blendState.src_color_blendfactor = config.srcColorBlend;
  blendState.dst_color_blendfactor = config.dstColorBlend;
  blendState.color_blend_op = SDL_GPU_BLENDOP_ADD;
  blendState.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  blendState.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blendState.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  blendState.color_write_mask = 0xF;

  SDL_GPUColorTargetDescription colorTargetDesc{};
  colorTargetDesc.format = SDL_GetGPUTextureFormatFromPixelFormat(HIC_SHADER_BRIDGE_PIXEL_FORMAT);

  colorTargetDesc.blend_state.enable_blend = config.blendEnable;
  colorTargetDesc.blend_state.src_color_blendfactor = config.srcColorBlend;
  colorTargetDesc.blend_state.dst_color_blendfactor = config.dstColorBlend;
  colorTargetDesc.blend_state.color_blend_op = config.colorBlendOp;
  colorTargetDesc.blend_state.src_alpha_blendfactor = config.srcAlphaBlend;
  colorTargetDesc.blend_state.dst_alpha_blendfactor = config.dstAlphaBlend;
  colorTargetDesc.blend_state.alpha_blend_op = config.alphaBlendOp;
  colorTargetDesc.blend_state.color_write_mask = 0xF; // RGBA

  SDL_GPUGraphicsPipelineTargetInfo targetInfo{};
  targetInfo.color_target_descriptions = &colorTargetDesc;
  targetInfo.num_color_targets = 1;

  if (config.depthTestEnable) {
    targetInfo.has_depth_stencil_target = true;
    targetInfo.depth_stencil_format = config.depthStencilFormat;
  } else {
    targetInfo.has_depth_stencil_target = false;
    targetInfo.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
  }

  pipelineInfo.target_info = targetInfo;

  HICL("GPUShader").info("=== PIPELINE INFO ===");
  HICL("GPUShader").info("vertex uniform buffers:", config.vertexUniformBuffers);
  HICL("GPUShader").info("fragment uniform buffers:", config.fragmentUniformBuffers);
  HICL("GPUShader").info("fragment samplers:", config.fragmentSamplers);
  HICL("GPUShader").info("num vertex buffers:", config.vertexBuffers.size());
  HICL("GPUShader").info("num vertex attributes:", config.vertexAttributes.size());
  HICL("GPUShader").info("color target format:", static_cast<int>(config.colorTargetFormat));
  HICL("GPUShader").info("blend enabled:", config.blendEnable);
  HICL("GPUShader").info("depth test enabled:", config.depthTestEnable);
  HICL("GPUShader").info("has depth target:", targetInfo.has_depth_stencil_target);
  HICL("GPUShader").info("depth format:", static_cast<int>(targetInfo.depth_stencil_format));

  pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

  SDL_ReleaseGPUShader(device, vertShader);
  SDL_ReleaseGPUShader(device, fragShader);
  SDL_free(vertexData);
  SDL_free(fragmentData);
  vertexData = fragmentData = nullptr;

  if (!pipeline) {
    HICL("GPUShader").error("failed to create graphics pipeline");
    HICL("GPUShader").error(SDL_GetError());
  }

  return pipeline != nullptr;
}

void GPUShader::bindBuffers() const {
  const GPUShader* effectiveShader = parent ? parent.get() : this;
  if (effectiveShader->vertexBuffer)
    bindVertexBuffer(effectiveShader->vertexBuffer, 0, 0);
  if (effectiveShader->indexBuffer)
    bindIndexBuffer(effectiveShader->indexBuffer, SDL_GPU_INDEXELEMENTSIZE_16BIT);
}

SDL_Mutex* GPUShader::texturePoolMutex = nullptr;
static std::once_flag texturePoolMutexFlag;
std::vector<GPUShader::TextureInfo*> GPUShader::texturePool;
GPUShader::TextureInfo* GPUShader::acquireBridgeTexture(SDL_Renderer* r) {
  std::call_once(texturePoolMutexFlag, []() {
    texturePoolMutex = SDL_CreateMutex();
  });

  if (!texturePoolMutex) return nullptr;

  SDL_LockMutex(texturePoolMutex);
  if (!texturePool.empty()) {
    TextureInfo* info = texturePool.back();
    texturePool.pop_back();
    SDL_UnlockMutex(texturePoolMutex);
    return info;
  }
  SDL_UnlockMutex(texturePoolMutex);

  // no available textures in the pool; create a new one
  SDL_Texture* tex = SDL_CreateTexture(r,
    HIC_SHADER_BRIDGE_PIXEL_FORMAT,
    SDL_TEXTUREACCESS_TARGET,
    HIC_SHADER_ATLAS_SIZE, HIC_SHADER_ATLAS_SIZE
  );

  if (!tex) {
    HICL("static GPUShader").error("Failed to create bridge texture:", SDL_GetError());
    return nullptr;
  }

  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  SDL_SetRenderTarget(r, tex);
  SDL_RenderClear(r);
  SDL_SetRenderTarget(r, nullptr);

  SDL_FlushRenderer(r);

  SDL_GPUTexture* gpuTex = static_cast<SDL_GPUTexture*>(SDL_GetPointerProperty(
    SDL_GetTextureProperties(tex),
    SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER,
    nullptr
  ));

  if (!gpuTex) {
    HICL("static GPUShader").error("Failed to cast bridge texture to GPU texture");
    SDL_DestroyTexture(tex);
    return nullptr;
  }

  TextureInfo* newInfo = new TextureInfo{ tex, gpuTex };
  return newInfo;
}

void GPUShader::releaseBridgeTexture(TextureInfo* info) {
  if (!info) return;
  if (!texturePoolMutex) {
    delete info;
    return;
  }
  SDL_LockMutex(texturePoolMutex);
  texturePool.push_back(info);
  SDL_UnlockMutex(texturePoolMutex);
}

void GPUShader::cleanupTexturePool() {
  if (!texturePoolMutex) return;

  SDL_LockMutex(texturePoolMutex);

  for (const auto& info : texturePool)
    delete info;

  texturePool.clear();
  SDL_UnlockMutex(texturePoolMutex);
  SDL_DestroyMutex(texturePoolMutex);
  texturePoolMutex = nullptr;
}

void GPUShader::createDefaultSampler() {
  if (!device) return;

  SDL_GPUSamplerCreateInfo samplerInfo{};
  samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
  samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
  samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  defaultSampler = SDL_CreateGPUSampler(device, &samplerInfo);
}

void GPUShader::begin(SDL_Renderer* renderer, const int width, const int height) {
  if (loaded) use(renderer);
  auto* const effectivePipeline = parent ? parent->pipeline : pipeline;
  auto* const effectiveDevice   = parent ? parent->device   : device;

  if (!effectivePipeline || !effectiveDevice) return;
  if (!bridgeTextureInfo) initBridge(renderer);

  if (!bridgeTextureInfo) {
    HICL("GPUShader").error("failed to initialize bridge texture");
    return;
  }

  activeRenderer = renderer;
  SDL_FlushRenderer(renderer);

  commandBuffer = SDL_AcquireGPUCommandBuffer(effectiveDevice);
  if (!commandBuffer) {
    HICL("GPUShader").error("failed to acquire GPU command buffer");
    activeRenderer = nullptr;
    return;
  }

  SDL_GPUColorTargetInfo colorTarget{};
  colorTarget.texture = bridgeTextureInfo->gpuHandle;
  colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
  colorTarget.clear_color = { 0, 0, 0, 0 };
  colorTarget.store_op = SDL_GPU_STOREOP_STORE;

  renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
  if (!renderPass) {
    HICL("GPUShader").error("failed to begin GPU render pass");
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    commandBuffer = nullptr;
    activeRenderer = nullptr;
    return;
  }

  SDL_BindGPUGraphicsPipeline(renderPass, effectivePipeline);
}

void GPUShader::end() {
  if (renderPass) {
    SDL_EndGPURenderPass(renderPass);
    renderPass = nullptr;
  }

  bool submitted = false;
  if (commandBuffer) {
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    commandBuffer = nullptr;
    submitted = true;
  }

  if (submitted && bridgeTextureInfo && activeRenderer)
    SDL_RenderTexture(activeRenderer, bridgeTextureInfo->bridgeTexture, nullptr, nullptr);

  activeRenderer = nullptr;
}

void GPUShader::bindFragmentTexture(const uint32_t slot, SDL_GPUTexture* texture) const {
  if (!renderPass || !texture) return;

  SDL_GPUTextureSamplerBinding binding{};
  binding.texture = texture;
  binding.sampler = parent ? parent->defaultSampler : defaultSampler;

  if (!binding.sampler) return;

  SDL_BindGPUFragmentSamplers(renderPass, slot, &binding, 1);
}

void GPUShader::bindVertexTexture(const uint32_t slot, SDL_GPUTexture* texture) const {
  if (!renderPass || !texture) return;

  SDL_GPUTextureSamplerBinding binding{};
  binding.texture = texture;
  binding.sampler = parent ? parent->defaultSampler : defaultSampler;

  if (!binding.sampler) return;

  SDL_BindGPUVertexSamplers(renderPass, slot, &binding, 1);
}

void GPUShader::bindVertexBuffer(SDL_GPUBuffer* buffer, const uint32_t slot, const uint32_t offset) const {
  if (!renderPass || !buffer) return;

  SDL_GPUBufferBinding binding{};
  binding.buffer = buffer;
  binding.offset = offset;

  SDL_BindGPUVertexBuffers(renderPass, slot, &binding, 1);
}

void GPUShader::bindIndexBuffer(SDL_GPUBuffer* buffer, const SDL_GPUIndexElementSize elementSize) const {
  if (!renderPass || !buffer) return;

  SDL_GPUBufferBinding binding{};
  binding.buffer = buffer;
  binding.offset = 0;

  SDL_BindGPUIndexBuffer(renderPass, &binding, elementSize);
}

void GPUShader::draw(const uint32_t vertexCount, const uint32_t instanceCount, const uint32_t firstVertex) const {
  if (renderPass)
    SDL_DrawGPUPrimitives(renderPass, vertexCount, instanceCount, firstVertex, 0);
}

void GPUShader::drawIndexed(const uint32_t indexCount, const uint32_t instanceCount, const uint32_t firstIndex) const {
  if (renderPass)
    SDL_DrawGPUIndexedPrimitives(renderPass, indexCount, instanceCount, firstIndex, 0, 0);
}

}