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
  mutable SDL_Mutex* loadMutex;
  mutable SDL_Mutex* cacheMutex;
  mutable SDL_Mutex* readyMutex;
  SDL_Condition* cv;
  std::atomic<bool> running{true};
  std::atomic<bool> loading{false};
  SDL_Thread* thread;

  std::unordered_map<std::string, std::weak_ptr<Base>> cache;

  static int workerThreadFunc(void* data);

  void threadLoop();
  void addToCache(const std::string& key, const std::shared_ptr<Base> &asset);
  std::shared_ptr<Base> loadCache(const std::string& key);

  // If the cached asset supports per-instance cloning, returns a fresh instance
  // that shares immutable GPU resources with the cached original.
  // Returns nullptr when the asset does not implement createInstance().
  static std::shared_ptr<Base> tryInstance(const std::shared_ptr<Base>& cached) {
    return cached ? cached->createInstance() : nullptr;
  }
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
  bool isLoading() const;
  size_t getCacheSize() const;
  size_t getPendingCount() const;
  size_t getReadyCount() const;
};

template<typename T, typename... Args>
std::shared_ptr<T> Manager::load(Args&&... args) {
  static_assert(std::is_base_of_v<Base, T>, "T must inherit from Assets::Base");

  // FIXME: redundant initialization here and funcs below
  const auto tempAsset = std::make_shared<T>(std::forward<Args>(args)...);
  std::string cacheKey = tempAsset->getCacheKey();

  if (const auto cached = loadCache(cacheKey)) {
    if (auto instance = tryInstance(cached))
      return std::static_pointer_cast<T>(instance);
    return std::static_pointer_cast<T>(cached);
  }

  LoadTask task{tempAsset, nullptr};

  SDL_LockMutex(loadMutex);
  loadTasks.push(std::move(task));
  SDL_SignalCondition(cv);
  SDL_UnlockMutex(loadMutex);

  addToCache(cacheKey, tempAsset);

  return tempAsset;
}

template<typename T, typename... Args>
std::shared_ptr<T> Manager::loadWithCallback(std::function<void()> callback, Args&&... args) {
  static_assert(std::is_base_of_v<Base, T>, "T must inherit from Assets::Base");

  const auto tempAsset = std::make_shared<T>(std::forward<Args>(args)...);
  std::string cacheKey = tempAsset->getCacheKey();

  if (const auto cached = loadCache(cacheKey)) {
    if (auto instance = tryInstance(cached)) {
      if (callback) callback();
      return std::static_pointer_cast<T>(instance);
    }
    if (callback) callback();
    return std::static_pointer_cast<T>(cached);
  }

  LoadTask task{tempAsset, std::move(callback)};

  SDL_LockMutex(loadMutex);
  loadTasks.push(std::move(task));
  SDL_SignalCondition(cv);
  SDL_UnlockMutex(loadMutex);

  addToCache(cacheKey, tempAsset);

  return tempAsset;
}

template<typename T, typename... Args>
std::shared_ptr<T> Manager::reload(Args&&... args) {
  static_assert(std::is_base_of_v<Base, T>, "T must inherit from Assets::Base");

  auto asset = std::make_shared<T>(std::forward<Args>(args)...);
  std::string cacheKey = asset->getCacheKey();

  LoadTask task{asset, nullptr};

  SDL_LockMutex(loadMutex);
  loadTasks.push(std::move(task));
  SDL_SignalCondition(cv);
  SDL_UnlockMutex(loadMutex);

  addToCache(cacheKey, asset);

  return asset;
}

}