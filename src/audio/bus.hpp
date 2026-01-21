#pragma once

#include <memory>
#include <vector>

#include "effect.hpp"
#include "../utils/hicapi.hpp"
#include "../assets/audio.hpp"
#include "stream.hpp"

namespace hic::Audio {

class HIC_API Bus {
public:
  virtual ~Bus() = default;

  // TODO: manager
  Bus() = default;
  Bus(const Bus&) = delete;

  void iRead(float* samples, int frames);
  virtual void read(float* samples, int frames);

  void connect(std::shared_ptr<Bus> bus);
  void disconnect(std::shared_ptr<Bus> bus);
  void iConnect(std::shared_ptr<Bus> bus);
  void iDisconnect(std::shared_ptr<Bus> bus);

  void addEffect(std::shared_ptr<BaseEffect> effect);
  void removeEffect(std::shared_ptr<BaseEffect> effect);
  void iAddEffect(std::shared_ptr<BaseEffect> effect);
  void iRemoveEffect(std::shared_ptr<BaseEffect> effect);
private:
  std::vector<std::shared_ptr<Bus>> children;
  std::vector<std::shared_ptr<BaseEffect>> effects;
  // TODO: declare manager here
};

class HIC_API AudioBus : public Bus {
public:
  explicit AudioBus(Assets::Audio* audio);
  ~AudioBus() override;

  void read(float* samples, int frames) override;
private:
  Assets::Audio* audio;
  OpusStream* stream;
};

}