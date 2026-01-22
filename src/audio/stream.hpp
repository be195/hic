#pragma once

#include <opusfile.h>
#include "../assets/audio.hpp"
#include "../utils/hicapi.hpp"

namespace hic::Audio {

class HIC_API OpusStream {
public:
  explicit OpusStream(Assets::Audio* audio);
  ~OpusStream();

  void getSamples(float* samples, int frames);
  void reset();
  void seek(double seconds);
  void setLooping(bool looping);
  bool isLooping() const;
  bool isFinished() const;
  double getPosition() const;
  double getDuration() const;

private:
  OggOpusFile* opusFile;
  Assets::Audio* audio;

  std::atomic<bool> loop{false};
  std::atomic<bool> finished{false};

  static constexpr int DECODE_BUFFER_SIZE = 8192;
  float decodeBuffer[DECODE_BUFFER_SIZE * 2] = {};
  int bufferReadPos = 0;
  int bufferValidSamples = 0;

  int decodeChunk();
};

}