#pragma once

#include <memory>
#include <vector>

#include "effect.hpp"
#include "../utils/hicapi.hpp"

namespace hic::Audio {

class HIC_API Bus {
public:
  virtual ~Bus() = default;

  Bus() = default;
  Bus(const Bus&) = delete;

  void iRead(float* samples, int frames);
  virtual void read(float* samples, int frames);

  void connect(std::shared_ptr<Bus> bus);
  void disconnect(std::shared_ptr<Bus> bus);

  void addEffect(std::shared_ptr<BaseEffect> effect);
  void removeEffect(std::shared_ptr<BaseEffect> effect);
private:
  std::vector<std::shared_ptr<Bus>> children;
  std::vector<std::shared_ptr<BaseEffect>> effects;
};

class HIC_API AudioBus : public Bus {
public:
  static constexpr int CHUNK_SIZE = 128;

  void read(float* samples, int frames) override;
private:
  std::vector<float**> chunks;
};

}