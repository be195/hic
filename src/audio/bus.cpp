#include "bus.hpp"

#include "manager.hpp"

namespace hic::Audio {

float Bus::getVolume() const {
  return volume.load(std::memory_order_relaxed);
}

void Bus::iSetVolume(const float newVolume) {
  volume.store(newVolume, std::memory_order_relaxed);
}

// TODO: clear fns?
void Bus::iConnect(const std::shared_ptr<Bus> &bus) {
  if (!bus) return;
  children.push_back(bus);
}

void Bus::iDisconnect(const std::shared_ptr<Bus> &bus) {
  if (!bus) return;
  std::erase(children, bus);
}

void Bus::iAddEffect(const std::shared_ptr<BaseEffect> &effect) {
  if (!effect) return;
  effects.push_back(effect);
}

void Bus::iRemoveEffect(const std::shared_ptr<BaseEffect> &effect) {
  if (!effect) return;
  std::erase(effects, effect);
}

void Bus::connect(const std::shared_ptr<Bus> &bus) {
  if (!bus || !manager) return;

  AudioCommand command = { AudioCommand::AudioCommandType::AddChild };
  command.parent = shared_from_this();
  command.child = bus;

  manager->pushCommand(command);
}

void Bus::disconnect(const std::shared_ptr<Bus> &bus) {
  if (!bus || !manager) return;

  AudioCommand command = { AudioCommand::AudioCommandType::RemoveChild };
  command.parent = shared_from_this();
  command.child = bus;

  manager->pushCommand(command);
}

void Bus::addEffect(const std::shared_ptr<BaseEffect> &effect) {
  if (!effect || !manager) return;

  AudioCommand command = { AudioCommand::AudioCommandType::AddEffect };
  command.parent = shared_from_this();
  command.effect = effect;

  manager->pushCommand(command);
}

void Bus::removeEffect(const std::shared_ptr<BaseEffect> &effect) {
  if (!effect || !manager) return;

  AudioCommand command = { AudioCommand::AudioCommandType::RemoveEffect };
  command.parent = shared_from_this();
  command.effect = effect;

  manager->pushCommand(command);
}

void Bus::setVolume(const float newVolume) {
  if (!manager) return;

  AudioCommand command = { AudioCommand::AudioCommandType::SetVolume };
  command.parent = shared_from_this();
  command.volume = newVolume;

  manager->pushCommand(command);
}

void Bus::iRead(float *samples, const int frames) {
  read(samples, frames);

  for (const auto& child : children)
    child->iRead(samples, frames);

  if (const float vol = volume.load(std::memory_order_relaxed); vol != 1.0f || !effects.empty()) {
    for (const auto& effect : effects)
      effect->apply(samples, frames);
    for (int i = 0; i < frames * 2; i++)
      samples[i] *= vol;
  }
}

std::shared_ptr<AudioBus> Bus::createAudioBus(const std::shared_ptr<Assets::Audio> &audio) {
  if (!audio) return nullptr;
  const auto audioBus = std::make_shared<AudioBus>(manager, audio);
  connect(audioBus);
  return audioBus;
}

AudioBus::AudioBus(Manager* manager, const std::shared_ptr<Assets::Audio> &audio) : Bus(manager), audio(audio) {
  stream = new OpusStream(audio.get());
}

AudioBus::~AudioBus() {
  delete stream;
}

void AudioBus::read(float *samples, const int frames) {
  if (stream)
    stream->getSamples(samples, frames);
}

}
