#include <opusfile.h>
#include "audio.hpp"
#include <algorithm>
#include <ranges>

#include "../utils/util.hpp"

namespace hic::Assets {

Audio::Audio(const char *fileName): fileName(fileName) {
  handlesMutex = SDL_CreateMutex();
  bufferMutex = SDL_CreateMutex();
}

Audio::~Audio() {
  SDL_Log("audio destructor start, handles: %zu", handles.size());

  std::vector<OggOpusFile*> toClose;

  SDL_LockMutex(handlesMutex);
  toClose.reserve(handles.size());

  for (auto& [handle, stream] : handles) {
    SDL_Log("orphaning stream %p for handle %p", stream, handle);
    stream->owner = nullptr;
    toClose.push_back(handle);
  }

  handles.clear();
  SDL_UnlockMutex(handlesMutex);

  SDL_Log("freeing %zu handles", toClose.size());
  for (const auto &handle: toClose) {
    op_free(handle);
    SDL_Log("freeing handle %p", handle);
  }

  SDL_Log("destroying mutex");

  SDL_LockMutex(bufferMutex);
  buffer.clear();
  SDL_UnlockMutex(bufferMutex);

  SDL_DestroyMutex(bufferMutex);
  SDL_DestroyMutex(handlesMutex);
  SDL_Log("ok");
}

int Audio::_closeCallback(void *stream) {
  const auto cast = static_cast<OpusMemoryStream*>(stream);
  delete cast;
  return 0;
}

int Audio::_readCallback(void *stream, unsigned char *ptr, const int n) {
  const auto cast = static_cast<OpusMemoryStream*>(stream);

  if (!cast->owner) return 0;

  SDL_LockMutex(cast->owner->bufferMutex);

  if (!cast->owner || cast->owner->buffer.empty()) {
    SDL_UnlockMutex(cast->owner->bufferMutex);
    return 0;
  }

  const size_t available = cast->size - cast->pos;
  const size_t toRead = min(static_cast<size_t>(n), available);

  if (toRead > 0) {
    std::memcpy(ptr, cast->data + cast->pos, toRead);
    cast->pos += toRead;
  }

  SDL_UnlockMutex(cast->owner->bufferMutex);
  return static_cast<int>(toRead);
}

int Audio::_seekCallback(void *stream, const opus_int64 offset, const int whence) {
  const auto cast = static_cast<OpusMemoryStream*>(stream);

  if (!cast->owner) return -1;

  SDL_LockMutex(cast->owner->bufferMutex);

  if (!cast->owner || cast->owner->buffer.empty()) {
    SDL_UnlockMutex(cast->owner->bufferMutex);
    return -1;
  }

  opus_int64 newPos = 0;
  switch (whence) {
    case SEEK_SET:
      newPos = offset; break;
    case SEEK_CUR:
      newPos = cast->pos + offset; break;
    case SEEK_END:
      newPos = cast->size + offset; break;
    default:
      SDL_UnlockMutex(cast->owner->bufferMutex);
      return -1;
  }

  if (newPos < 0 || newPos > static_cast<opus_int64>(cast->size)) {
    SDL_UnlockMutex(cast->owner->bufferMutex);
    return -1;
  }

  cast->pos = static_cast<size_t>(newPos);
  SDL_UnlockMutex(cast->owner->bufferMutex);
  return 0;
}

opus_int64 Audio::_tellCallback(void *stream) {
  const auto cast = static_cast<OpusMemoryStream*>(stream);
  if (!cast->owner) return -1;

  SDL_LockMutex(cast->owner->bufferMutex);
  const auto res = static_cast<opus_int64>(cast->pos);
  SDL_UnlockMutex(cast->owner->bufferMutex);

  return res;
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
    this
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

  SDL_LockMutex(handlesMutex);
  handles[handle] = memStream;
  SDL_UnlockMutex(handlesMutex);

  return handle;
}

void Audio::freeHandle(OggOpusFile* handle) {
  if (!handle) return;

  SDL_LockMutex(handlesMutex);
  handles.erase(handle);
  SDL_UnlockMutex(handlesMutex);

  op_free(handle);
}

}