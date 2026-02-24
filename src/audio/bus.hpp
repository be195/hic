#pragma once

#include <memory>
#include <vector>

#include "effect.hpp"
#include "../utils/hicapi.hpp"
#include "../assets/audio.hpp"
#include "stream.hpp"

namespace hic::Audio {
class Manager;
class AudioBus;

class HIC_API Bus : public std::enable_shared_from_this<Bus> {
public:
  virtual ~Bus() = default;

  explicit Bus(Manager* manager): manager(manager) {}
  Bus(const Bus&) = delete;

  void iRead(float* samples, int frames);
  virtual void read(float* samples, int frames) {};

  void connect(const std::shared_ptr<Bus> &bus);
  void disconnect(const std::shared_ptr<Bus> &bus);
  void iConnect(const std::shared_ptr<Bus> &bus);
  void iDisconnect(const std::shared_ptr<Bus> &bus);

  void addEffect(const std::shared_ptr<BaseEffect> &effect);
  void removeEffect(const std::shared_ptr<BaseEffect> &effect);
  void iAddEffect(const std::shared_ptr<BaseEffect> &effect);
  void iRemoveEffect(const std::shared_ptr<BaseEffect> &effect);

  void setVolume(float newVolume);
  float getVolume() const;
  void iSetVolume(float newVolume);

  virtual bool isFinished() const { return false; }
  virtual bool isDiscardOnFinish() const { return false; }

  std::shared_ptr<AudioBus> createAudioBus(const std::shared_ptr<Assets::Audio> &audio);
private:
  std::vector<std::shared_ptr<Bus>> children;
  std::vector<std::shared_ptr<BaseEffect>> effects;
  Manager* manager;
  std::atomic<float> volume = {1.0f};
};

class HIC_API AudioBus : public Bus {
public:
  explicit AudioBus(Manager* manager, const std::shared_ptr<Assets::Audio> &audio);
  ~AudioBus() override;

  void read(float* samples, int frames) override;

  bool isFinished() const override;
  bool isDiscardOnFinish() const override;
  void setDiscardOnFinish(bool discard);

  void play();
  void stop();
  bool isPlaying() const;

  void setLooping(bool looping);
  bool isLooping() const;

private:
  std::shared_ptr<Assets::Audio> audio;
  OpusStream* stream;
  std::atomic<bool> playing{true};
  std::atomic<bool> discardOnFinish{false};
};

}