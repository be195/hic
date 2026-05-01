#include <opusfile.h>
#include "audio.hpp"
#include <algorithm>
#include <fstream>
#include <ranges>
#include "../utils/util.hpp"
#include "../utils/logging.hpp"

namespace hic::Assets {

Audio::Audio(std::string fileName, bool sample): fileName(std::move(fileName)), sample(sample) {
  handlesMutex = SDL_CreateMutex();
  if (!handlesMutex) HICL("Audio").error("failed to create handles mutex");

  bufferMutex = SDL_CreateMutex();
  if (!bufferMutex) HICL("Audio").error("failed to create buffer mutex");
}

Audio::~Audio() {
  HICL("Audio").debug("audio destructor start, handles:", handles.size());

  std::vector<OggOpusFile*> toClose;

  SDL_LockMutex(handlesMutex);
  toClose.reserve(handles.size());

  for (auto& [handle, stream] : handles) {
    HICL("Audio").debug("orphaning stream", stream, "for handle", handle);
    stream->owner = nullptr;
    toClose.push_back(handle);
  }

  handles.clear();
  SDL_UnlockMutex(handlesMutex);

  HICL("Audio").debug("freeing", toClose.size(), "handles");
  for (const auto &handle: toClose) {
    op_free(handle);
    HICL("Audio").debug("freeing handle", handle);
  }

  HICL("Audio").debug("destroying mutex");

  SDL_LockMutex(bufferMutex);
  buffer.clear();
  SDL_UnlockMutex(bufferMutex);

  SDL_DestroyMutex(bufferMutex);
  SDL_DestroyMutex(handlesMutex);
  HICL("Audio").debug("destroyed");
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
  const size_t toRead = std::min(static_cast<size_t>(n), available);

  if (toRead > 0) {
    memcpy(ptr, cast->data + cast->pos, toRead);
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
  std::ifstream file("audio/" + fileName + ".ogg", std::ios::binary | std::ios::ate);
  if (!file) {
    HICL("Audio").error("could not open audio file:", fileName);
    return;
  }

  const auto fileSize = file.tellg();
  if (fileSize <= 0) {
    HICL("Audio").error("could not determine audio file size:", fileName);
    return;
  }

  file.seekg(0, std::ios::beg);

  buffer.resize(fileSize);
  if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
    HICL("Audio").error("failed to read audio file:", fileName);
    buffer.clear();
  }

  if (sample) {
    const auto handle = createHandle();
    if (!handle) {
      HICL("Audio").error("failed to create OggOpusFile handle for sampling:", fileName);
      buffer.clear();
      return;
    }

    const int channels = op_channel_count(handle, -1);
    const int frameSize = 960; // 20ms at 48kHz

    const ogg_int64_t totalSamples = op_pcm_total(handle, -1);
    if (totalSamples > 0)
      decodedBuffer.reserve(static_cast<size_t>(totalSamples * channels));

    std::vector<opus_int16> pcm(frameSize * channels);

    while (true) {
      const int samplesRead = op_read(handle, pcm.data(), frameSize, nullptr);
      if (samplesRead < 0) {
        HICL("Audio").error("error decoding OggOpusFile for sampling:", fileName, "error code:", samplesRead);
        decodedBuffer.clear();
        break;
      }
      if (samplesRead == 0) break;

      decodedBuffer.insert(decodedBuffer.end(), pcm.begin(), pcm.begin() + (samplesRead * channels));
    }

    freeHandle(handle);

    if (!decodedBuffer.empty())
      HICL("Audio").debug("decoded audio sample size for", fileName, ":", decodedBuffer.size() * sizeof(opus_int16), "bytes");
  }
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