#pragma once

#include <atomic>
#include <SDL3/SDL.h>
#include <opusfile.h>
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
  };

  static constexpr int OPUS_SAMPLE_RATE = 48000;
  const char* fileName;

  double getDuration();

  OggOpusFile* createHandle();
  void freeHandle(OggOpusFile* handle);

  void preload() override;

  std::string getCacheKey() const override { return fileName; }
private:
  std::vector<unsigned char> buffer;
  std::atomic<int> instanceCount;

  static void _closeCallback(void* stream);
  static void _readCallback(void* stream, unsigned char* ptr, int n);
  static void _seekCallback(void* stream, opus_int64 offset, int whence);
  static void _tellCallback(void* stream);
};

}
