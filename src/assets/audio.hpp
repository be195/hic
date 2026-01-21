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
    std::atomic<int> *instanceCount;
  };

  static constexpr int OPUS_SAMPLE_RATE = 48000;
  const char* fileName;

  int getInstanceCount() const;

  OggOpusFile* createHandle();
  void freeHandle(OggOpusFile* handle);

  void preload() override;

  std::string getCacheKey() const override { return fileName; }
private:
  std::vector<unsigned char> buffer;
  std::atomic<int> instanceCount;

  static int _closeCallback(void* stream);
  static int _readCallback(void* stream, unsigned char* ptr, int n);
  static int _seekCallback(void* stream, opus_int64 offset, int whence);
  static opus_int64 _tellCallback(void* stream);
};

}
