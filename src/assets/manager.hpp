#pragma once
#include <functional>
#include <memory>
#include <queue>
#include "../utils/hicapi.hpp"

#include "base.hpp"

namespace hic::Assets {

class HIC_API Manager {
private:
  struct LoadTask {
    std::shared_ptr<Base> asset;
    std::function<void()> callback;
  };

  std::queue<LoadTask> loadTasks;
  std::queue<LoadTask> ready;
  SDL_Mutex* loadMutex;
  SDL_Mutex* cacheMutex;
  SDL_Mutex* readyMutex;
  SDL_Condition* cv;
  std::atomic<bool> running{true};
  SDL_Thread* thread;

  std::unordered_map<std::string, std::weak_ptr<Base>> cache;

  static int workerThreadFunc(void* data);

  void threadLoop();
  void addToCache(const std::string& key, const std::shared_ptr<Base> &asset);
  std::shared_ptr<Base> loadCache(const std::string& key);
public:
  Manager();
  ~Manager();

  template<typename T, typename... Args>
  std::shared_ptr<T> load(Args&&... args);

  template<typename T, typename... Args>
  std::shared_ptr<T> loadWithCallback(std::function<void()> callback, Args&&... args);

  template<typename T, typename... Args>
  std::shared_ptr<T> reload(Args&&... args);

  void processReady(SDL_Renderer* renderer);
  void clearCache();
  size_t getCacheSize() const;
  size_t getPendingCount() const;
  size_t getReadyCount() const;
};

}
