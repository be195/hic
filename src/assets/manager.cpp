#include "manager.hpp"
#include "base.hpp"
#include "../utils/util.hpp"

namespace hic::Assets {

int Manager::workerThreadFunc(void* data) {
  const auto manager = static_cast<Manager*>(data);
  manager->threadLoop();
  return 0;
}

void Manager::threadLoop() {
  while (running) {
    LoadTask task;

    SDL_LockMutex(loadMutex);

    while (loadTasks.empty() && running)
      SDL_WaitCondition(cv, loadMutex);

    if (!running && loadTasks.empty()) {
      SDL_UnlockMutex(loadMutex);
      break;
    }

    if (!loadTasks.empty()) {
      task = std::move(loadTasks.front());
      loadTasks.pop();
    }

    SDL_UnlockMutex(loadMutex);

    if (task.asset) {
      task.asset->preload();

      SDL_LockMutex(readyMutex);
      ready.push(std::move(task));
      SDL_UnlockMutex(readyMutex);
    }
  }
}

void Manager::addToCache(const std::string& key, const std::shared_ptr<Base> &asset) {
  if (key.empty()) return;

  SDL_LockMutex(cacheMutex);
  cache[key] = asset;
  SDL_UnlockMutex(cacheMutex);
}

std::shared_ptr<Base> Manager::loadCache(const std::string& key) {
  if (key.empty()) return nullptr;

  SDL_LockMutex(cacheMutex);

  const auto it = cache.find(key);
  std::shared_ptr<Base> result = nullptr;

  if (it != cache.end()) {
    if (const auto asset = it->second.lock())
      result = asset;
    else
      cache.erase(it);
  }

  SDL_UnlockMutex(cacheMutex);
  return result;
}

Manager::Manager() {
  loadMutex = SDL_CreateMutex();
  cacheMutex = SDL_CreateMutex();
  readyMutex = SDL_CreateMutex();
  cv = SDL_CreateCondition();

  const auto props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void*)workerThreadFunc);
  SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, this);
  SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "hic::Assets::Manager");

  thread = SDL_CreateThreadWithProperties(props);
  SDL_DestroyProperties(props);
}

Manager::~Manager() {
  running = false;

  SDL_LockMutex(loadMutex);
  SDL_SignalCondition(cv);
  SDL_UnlockMutex(loadMutex);

  SDL_WaitThread(thread, nullptr);

  SDL_DestroyMutex(loadMutex);
  SDL_DestroyMutex(readyMutex);
  SDL_DestroyMutex(cacheMutex);
  SDL_DestroyCondition(cv);
}

template<typename T, typename... Args>
std::shared_ptr<T> Manager::load(Args&&... args) {
  static_assert(std::is_base_of_v<Base, T>, "T must inherit from Assets::Base");

  const auto tempAsset = std::make_shared<T>(std::forward<Args>(args)...);
  std::string cacheKey = tempAsset->getCacheKey();

  if (const auto cached = loadCache(cacheKey))
    return std::static_pointer_cast<T>(cached);

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

void Manager::processReady(SDL_Renderer* renderer) {
  SDL_LockMutex(readyMutex);

  while (!ready.empty()) {
    if (auto&[asset, callback] = ready.front(); asset) {
      asset->use(renderer);

      if (callback)
        callback();
    }

    ready.pop();
  }

  SDL_UnlockMutex(readyMutex);
}

void Manager::clearCache() {
  SDL_LockMutex(cacheMutex);
  cache.clear();
  SDL_UnlockMutex(cacheMutex);
}

size_t Manager::getCacheSize() const {
  SDL_LockMutex(cacheMutex);
  const size_t size = cache.size();
  SDL_UnlockMutex(cacheMutex);
  return size;
}

size_t Manager::getPendingCount() const {
  SDL_LockMutex(loadMutex);
  const size_t count = loadTasks.size();
  SDL_UnlockMutex(loadMutex);
  return count;
}

size_t Manager::getReadyCount() const {
  SDL_LockMutex(readyMutex);
  const size_t count = ready.size();
  SDL_UnlockMutex(readyMutex);
  return count;
}

}
