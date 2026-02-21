#pragma once

#include <atomic>
#include <memory>
#include <opusfile.h>
#include <unordered_map>
#include <vector>
#include "base.hpp"
#include "../utils/hicapi.hpp"

namespace hic::Assets {

class HIC_API Audio : public Base {
public:
  struct OpusMemoryStream {
    const unsigned char* data;
    size_t size;
    size_t pos;
    const Audio* owner;
  };

  explicit Audio(std::string fileName);
  ~Audio() override;

  static constexpr int OPUS_SAMPLE_RATE = 48000;
  std::string fileName;

  OggOpusFile* createHandle();
  void freeHandle(OggOpusFile* handle);

  void preload() override;

  std::string getCacheKey() const override { return "aud#" + fileName; }
private:
  std::vector<unsigned char> buffer;

  SDL_Mutex* bufferMutex = nullptr;
  SDL_Mutex* handlesMutex = nullptr;
  std::unordered_map<OggOpusFile*, OpusMemoryStream*> handles;

  static int _closeCallback(void* stream);
  static int _readCallback(void* stream, unsigned char* ptr, int n);
  static int _seekCallback(void* stream, opus_int64 offset, int whence);
  static opus_int64 _tellCallback(void* stream);
};

}
