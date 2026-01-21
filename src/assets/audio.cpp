#include <opusfile.h>
#include "audio.hpp"
#include <algorithm>

#include "../utils/util.hpp"

namespace hic::Assets {

int Audio::_closeCallback(void *stream) {
  const auto cast = static_cast<OpusMemoryStream*>(stream);
  if (cast->instanceCount)
    cast->instanceCount->fetch_sub(1, std::memory_order_relaxed);
  delete cast;
  return 0;
}

int Audio::_readCallback(void *stream, unsigned char *ptr, const int n) {
  const auto cast = static_cast<OpusMemoryStream*>(stream);

  const size_t available = cast->size - cast->pos;
  const size_t toRead = min(static_cast<size_t>(n), available);

  if (toRead > 0) {
    std::memcpy(ptr, cast->data + cast->pos, toRead);
    cast->pos += toRead;
  }

  return static_cast<int>(toRead);
}

int Audio::_seekCallback(void *stream, const opus_int64 offset, const int whence) {
  const auto cast = static_cast<OpusMemoryStream*>(stream);

  opus_int64 newPos = 0;
  switch (whence) {
    case SEEK_SET:
      newPos = offset; break;
    case SEEK_CUR:
      newPos = cast->pos + offset; break;
    case SEEK_END:
      newPos = cast->size + offset; break;
    default:
      return -1;
  }

  if (newPos < 0 || newPos > static_cast<opus_int64>(cast->size))
    return -1;

  cast->pos = static_cast<size_t>(newPos);
  return 0;
}

opus_int64 Audio::_tellCallback(void *stream) {
  const auto cast = static_cast<OpusMemoryStream*>(stream);
  return static_cast<opus_int64>(cast->pos);
}

void Audio::preload() {
  SDL_IOStream* file = SDL_IOFromFile(fileName, "rb");
  if (!file)
    panic("could not open provided audio file");

  const Sint64 fileSize = SDL_GetIOSize(file);
  if (fileSize <= 0) {
    SDL_CloseIO(file);
    panic("could not determine audio file size");
  }

  buffer.resize(fileSize);
  const size_t bytesRead = SDL_ReadIO(file, buffer.data(), fileSize);
  SDL_CloseIO(file);

  if (bytesRead != fileSize)
    panic("failed to read audio file fully");
}

OggOpusFile* Audio::createHandle() {
  if (buffer.empty()) return nullptr;

  const auto memStream = new OpusMemoryStream(
    buffer.data(),
    buffer.size(),
    0,
    &instanceCount
  );

  OpusFileCallbacks callbacks;
  callbacks.read = _readCallback;
  callbacks.seek = _seekCallback;
  callbacks.tell = _tellCallback;
  callbacks.close = _closeCallback;

  int error = 0;
  OggOpusFile* handle = op_open_callbacks(memStream, &callbacks, nullptr, 0, &error);
  if (!handle || error != 0) {
    delete memStream;
    return nullptr;
  }

  instanceCount.fetch_add(1, std::memory_order_relaxed);
  return handle;
}

void Audio::freeHandle(OggOpusFile* handle) {
  if (!handle) return;
  op_free(handle);
}

int Audio::getInstanceCount() const {
  return instanceCount.load(std::memory_order_relaxed);
}

}