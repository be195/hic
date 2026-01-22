#pragma once

#include "bus.hpp"
#include "const.hpp"
#include "../utils/hicapi.hpp"
#include "SDL3/SDL_audio.h"

namespace hic::Audio {

struct AudioCommand {
  enum class AudioCommandType { AddChild, RemoveChild, AddEffect, RemoveEffect, SetVolume };
  AudioCommandType type;
  std::shared_ptr<Bus> parent;
  std::shared_ptr<Bus> child;
  std::shared_ptr<BaseEffect> effect;
  float volume = 0.0f;
};

class HIC_API Manager {
public:
  Manager();
  ~Manager();

  std::shared_ptr<Bus> getMaster() const;
  bool pushCommand(const AudioCommand &command);
  std::shared_ptr<Bus> createBus();
private:
  SDL_AudioStream* stream;
  std::shared_ptr<Bus> master;

  AudioCommand commands[COMMAND_QUEUE_MAX];
  std::atomic<uint32_t> head;
  std::atomic<uint32_t> tail;

  static void _process(void* userdata, SDL_AudioStream* stream, int additional_amt, int _);
protected:
  std::vector<float> scratchBuffer;
  bool popCommand(AudioCommand &command);
};

}
