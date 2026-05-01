#include "stream.hpp"
#include "const.hpp"
#include <algorithm>

namespace hic::Audio {

PCMStream::PCMStream(Assets::Audio* audio) : audio(audio) {}

void PCMStream::getSamples(float* samples, const int frames) {
  if (finished.load(std::memory_order_acquire)) return;

  const auto& buffer = audio->getDecodedBuffer();
  if (buffer.empty()) {
    finished.store(true, std::memory_order_release);
    return;
  }

  const int samplesNeeded = frames * 2;
  int samplesWritten = 0;

  while (samplesWritten < samplesNeeded) {
    if (position >= buffer.size()) {
      if (loop.load(std::memory_order_acquire))
        position = 0;
      else {
        finished.store(true, std::memory_order_release);
        break;
      }
    }

    const size_t samplesAvailable = buffer.size() - position;
    const size_t toCopy = std::min(static_cast<size_t>(samplesNeeded - samplesWritten), samplesAvailable);

    for (size_t i = 0; i < toCopy; ++i)
      samples[samplesWritten + i] += static_cast<float>(buffer[position + i]) / 32768.f;

    position += toCopy;
    samplesWritten += static_cast<int>(toCopy);
  }
}

OpusStream::OpusStream(Assets::Audio* audio): audio(audio) {
  opusFile = audio->createHandle();
}

OpusStream::~OpusStream() {
  if (opusFile && audio)
    audio->freeHandle(opusFile);
}

double OpusStream::getDuration() const {
  if (!opusFile) return 0.0;
  return op_pcm_total(opusFile, -1) / SAMPLE_RATE;
}

double OpusStream::getPosition() const {
  if (!opusFile) return 0.0;
  return op_pcm_tell(opusFile) / SAMPLE_RATE;
}

bool OpusStream::isFinished() const {
  return finished.load(std::memory_order_acquire);
}

bool OpusStream::isLooping() const {
  return loop.load(std::memory_order_acquire);
}

void OpusStream::setLooping(const bool looping) {
  loop.store(looping, std::memory_order_release);
}

void OpusStream::seek(const double seconds) {
  if (!opusFile) return;

  const ogg_int64_t sample = static_cast<ogg_int64_t>(seconds * SAMPLE_RATE);
  op_pcm_seek(opusFile, sample);

  bufferReadPos = 0;
  bufferValidSamples = 0;
  finished.store(false, std::memory_order_release);
  decodeChunk();
}

void OpusStream::reset() {
  return seek(0);
}

int OpusStream::decodeChunk() {
  if (!opusFile) return 0;

  int samplesRead = op_read_float_stereo(opusFile, decodeBuffer, DECODE_BUFFER_SIZE);

  if (samplesRead < 0)
    return 0;

  if (samplesRead == 0) {
    if (loop.load(std::memory_order_acquire)) {
      op_pcm_seek(opusFile, 0);
      samplesRead = op_read_float_stereo(opusFile, decodeBuffer, DECODE_BUFFER_SIZE);
    } else {
      finished.store(true, std::memory_order_release);
      return 0;
    }
  }

  bufferReadPos = 0;
  bufferValidSamples = samplesRead * 2;
  return bufferValidSamples;
}

void OpusStream::getSamples(float* samples, const int frames) {
  if (finished.load(std::memory_order_acquire)) return;

  const int samplesNeeded = frames * 2;
  int samplesWritten = 0;

  while (samplesWritten < samplesNeeded) {
    if (bufferReadPos >= bufferValidSamples) {
      if (decodeChunk() <= 0) return;
    }

    const int samplesAvailable = bufferValidSamples - bufferReadPos;
    const int samplesToCopy = std::min(samplesNeeded - samplesWritten, samplesAvailable);

    for (int i = 0; i < samplesToCopy; ++i)
      samples[samplesWritten + i] += decodeBuffer[bufferReadPos + i];

    bufferReadPos += samplesToCopy;
    samplesWritten += samplesToCopy;
  }
}

}
