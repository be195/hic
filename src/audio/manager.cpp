#include "manager.hpp"
#include <algorithm>
#include "const.hpp"
#include "../utils/util.hpp"

namespace hic::Audio {

Manager::Manager() {
  master = createBus();

  constexpr SDL_AudioSpec audioSpec = { SDL_AUDIO_F32, 2, SAMPLE_RATE };
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, _process, this);

  if (stream)
    SDL_ResumeAudioStreamDevice(stream);
}

Manager::~Manager() {
  if (stream) {
    SDL_PauseAudioStreamDevice(stream);
    SDL_UnbindAudioStream(stream);
    SDL_DestroyAudioStream(stream);
  }
}

std::shared_ptr<Bus> Manager::getMaster() const {
  return master;
}

std::shared_ptr<Bus> Manager::createBus() {
  return std::make_shared<Bus>(this);
}

bool Manager::pushCommand(const AudioCommand &command) {
  const auto t = tail.load(std::memory_order_relaxed);
  const auto next = (t + 1) % COMMAND_QUEUE_MAX;

  if (next == head.load(std::memory_order_acquire))
    return false;

  commands[t] = command;
  tail.store(next, std::memory_order_release);
  return true;
}

bool Manager::popCommand(AudioCommand &command) {
  const auto h = head.load(std::memory_order_relaxed);

  if (h == tail.load(std::memory_order_acquire))
    return false;

  command = commands[h];
  head.store((h + 1) % COMMAND_QUEUE_MAX, std::memory_order_release);
  return true;
}

void Manager::_process(void *userdata, SDL_AudioStream *stream, const int additional_amt, int _) {
  const auto manager = static_cast<Manager*>(userdata);

  AudioCommand command;
  while (manager->popCommand(command)) {
    if (!command.parent) continue; // !?

    switch (command.type) {
      case AudioCommand::AudioCommandType::AddChild:
        command.parent->iConnect(command.child);
        break;
      case AudioCommand::AudioCommandType::RemoveChild:
        command.parent->iDisconnect(command.child);
        break;
      case AudioCommand::AudioCommandType::AddEffect:
        command.parent->iAddEffect(command.effect);
        break;
      case AudioCommand::AudioCommandType::RemoveEffect:
        command.parent->iRemoveEffect(command.effect);
        break;
      case AudioCommand::AudioCommandType::SetVolume:
        command.parent->iSetVolume(command.volume);
        break;
    }
  }

  const int samples = additional_amt / sizeof(float);
  const int frameCount = samples / 2;

  if (manager->scratchBuffer.size() < samples)
    manager->scratchBuffer.resize(samples);

  float* mixBuffer = manager->scratchBuffer.data();
  std::fill_n(mixBuffer, samples, 0.0f);

  const auto masterBus = manager->getMaster();
  masterBus->iRead(mixBuffer, frameCount);

  for (int i = 0; i < samples; i++)
    mixBuffer[i] = std::clamp(mixBuffer[i], -1.0f, 1.0f);

  SDL_PutAudioStreamData(stream, mixBuffer, additional_amt);
}

}
