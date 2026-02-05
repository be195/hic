#include "shader.hpp"
#include "../utils/logging.hpp"

#define HIC_SHADER_ENTRYPOINT "main"

#if defined(_WIN32) || defined(__CYGWIN__)
#define HIC_GPUSHADER_EXT ".dxil"
#define HIC_GPUSHADER_FORMAT SDL_GPU_SHADERFORMAT_DXIL
#elif defined(__APPLE__)
#define HIC_GPUSHADER_EXIT ".metal"
#define HIC_GPUSHADER_FORMAT SDL_GPU_SHADERFORMAT_MSL
#else
#define HIC_GPUSHADER_EXT ".spv"
#define HIC_GPUSHADER_FORMAT SDL_GPU_SHADERFORMAT_SPIRV
#endif

#if defined(HIC_FORCE_GPUSHADER_FORMAT) && defined(HIC_FORCE_GPUSHADER_EXT)
#define HIC_GPUSHADER_EXT HIC_FORCE_GPUSHADER_EXT
#define HIC_GPUSHADER_FORMAT HIC_FORCE_GPUSHADER_FORMAT
#endif

#define HIC_BRIDGE_PIXEL_FORMAT SDL_PIXELFORMAT_BGRA32
#define HIC_SHADER_ATLAS_SIZE 512

namespace hic::Assets {

GPUShader::GPUShader(std::string vertexFile, std::string fragmentFile, Config config)
  : vertexFileName(std::move(vertexFile))
  , fragmentFileName(std::move(fragmentFile))
  , config(std::move(config)) {}

GPUShader::~GPUShader() {
  if (vertexData) SDL_free(vertexData);
  if (fragmentData) SDL_free(fragmentData);
  if (defaultSampler) SDL_ReleaseGPUSampler(device, defaultSampler);
  if (pipeline) SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
  if (bridgeTexture) SDL_DestroyTexture(bridgeTexture);
  if (vertexBuffer) SDL_ReleaseGPUBuffer(device, vertexBuffer);
  if (indexBuffer) SDL_ReleaseGPUBuffer(device, indexBuffer);
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
  if (!loaded || pipeline) return;

  device = SDL_GetGPURendererDevice(renderer);
  if (!device) {
    HICL("GPUShader").error("Failed to get GPU device");
    return;
  }

  createPipeline();
  createDefaultSampler();
  createBuffers(config.vertexData, config.indexData);
}

void GPUShader::initBridge(SDL_Renderer *r) {
  bridgeTexture = SDL_CreateTexture(r,
    HIC_BRIDGE_PIXEL_FORMAT,
    SDL_TEXTUREACCESS_TARGET,
    HIC_SHADER_ATLAS_SIZE, HIC_SHADER_ATLAS_SIZE
  );

  if (!bridgeTexture) {
    HICL("GPUShader").error("Failed to create bridge texture:", SDL_GetError());
    return;
  }

  SDL_SetTextureScaleMode(bridgeTexture, SDL_SCALEMODE_NEAREST);
  SDL_SetRenderTarget(r, bridgeTexture);
  SDL_RenderClear(r);
  SDL_SetRenderTarget(r, nullptr);

  // explicitly flush to ensure the command buffer reaches the GPU driver logic
  SDL_FlushRenderer(r);

  gpuHandle = static_cast<SDL_GPUTexture *>(SDL_GetPointerProperty(
    SDL_GetTextureProperties(bridgeTexture),
    SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER,
    nullptr
  ));

  if (!gpuHandle)
    HICL("GPUShader").error("Failed to cast bridge texture to GPU texture");
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
  colorTargetDesc.format = SDL_GetGPUTextureFormatFromPixelFormat(HIC_BRIDGE_PIXEL_FORMAT);

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
  if (pipeline != nullptr) {
    SDL_free(vertexData);
    SDL_free(fragmentData);
  } else {
    HICL("GPUShader").error("failed to create graphics pipeline");
    HICL("GPUShader").error(SDL_GetError());
  }
  vertexData = fragmentData = nullptr;

  return pipeline != nullptr;
}

void GPUShader::bindBuffers() const {
  if (vertexBuffer)
    bindVertexBuffer(vertexBuffer, 0, 0);
  if (indexBuffer)
    bindIndexBuffer(indexBuffer, SDL_GPU_INDEXELEMENTSIZE_16BIT);
}

SDL_GPUTexture* GPUShader::gpuHandle = nullptr;
SDL_Texture* GPUShader::bridgeTexture = nullptr;

void GPUShader::createDefaultSampler() {
  SDL_GPUSamplerCreateInfo samplerInfo{};
  samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
  samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
  samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  defaultSampler = SDL_CreateGPUSampler(device, &samplerInfo);
}

void GPUShader::begin(SDL_Renderer* renderer, const int width, const int height) {
  if (!pipeline) return;
  if (!bridgeTexture) initBridge(renderer);

  activeRenderer = renderer;

  if (!gpuHandle) return;
  commandBuffer = SDL_AcquireGPUCommandBuffer(device);

  SDL_GPUColorTargetInfo colorTarget{};
  colorTarget.texture = gpuHandle;
  colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
  colorTarget.clear_color = { 0, 0, 0, 0 };
  colorTarget.store_op = SDL_GPU_STOREOP_STORE;

  renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
  SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
}

void GPUShader::end() {
  if (renderPass) {
    SDL_EndGPURenderPass(renderPass);
    renderPass = nullptr;
  }

  if (commandBuffer) {
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    commandBuffer = nullptr;
  }

  if (bridgeTexture)
    SDL_RenderTexture(activeRenderer, bridgeTexture, nullptr, nullptr);

  activeRenderer = nullptr;
}

void GPUShader::bindFragmentTexture(const uint32_t slot, SDL_GPUTexture* texture) const {
  if (!renderPass) return;

  SDL_GPUTextureSamplerBinding binding{};
  binding.texture = texture;
  binding.sampler = defaultSampler;

  SDL_BindGPUFragmentSamplers(renderPass, slot, &binding, 1);
}

void GPUShader::bindVertexTexture(const uint32_t slot, SDL_GPUTexture* texture) const {
  if (!renderPass) return;

  SDL_GPUTextureSamplerBinding binding{};
  binding.texture = texture;
  binding.sampler = defaultSampler;

  SDL_BindGPUVertexSamplers(renderPass, slot, &binding, 1);
}

void GPUShader::bindVertexBuffer(SDL_GPUBuffer* buffer, const uint32_t slot, const uint32_t offset) const {
  if (!renderPass) return;

  SDL_GPUBufferBinding binding{};
  binding.buffer = buffer;
  binding.offset = offset;

  SDL_BindGPUVertexBuffers(renderPass, slot, &binding, 1);
}

void GPUShader::bindIndexBuffer(SDL_GPUBuffer* buffer, const SDL_GPUIndexElementSize elementSize) const {
  if (!renderPass) return;

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