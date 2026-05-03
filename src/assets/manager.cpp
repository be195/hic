#include "manager.hpp"
#include "base.hpp"
#include "../utils/util.hpp"
#include "utils/logging.hpp"

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
      loading.store(true, std::memory_order_release);

      HICL("AssetManager").info("preloading", task.asset->getCacheKey());
      task.asset->preload(this);

      SDL_LockMutex(readyMutex);
      ready.push(std::move(task));
      SDL_UnlockMutex(readyMutex);

      loading.store(false, std::memory_order_release);
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

void Manager::addSearchPath(const std::string& path) {
  searchPaths.push_back(path);
}

std::string Manager::resolve(const std::string& fileName) const {
  for (const auto& path : searchPaths) {
    std::string fullPath = path.empty() ? fileName : path + "/" + fileName;
    if (SDL_GetPathInfo(fullPath.c_str(), nullptr))
      return fullPath;
  }

  return fileName;
}

Manager::Manager() {
  loadMutex = SDL_CreateMutex();
  cacheMutex = SDL_CreateMutex();
  readyMutex = SDL_CreateMutex();
  cv = SDL_CreateCondition();

  thread = SDL_CreateThread(workerThreadFunc, "hic::Assets::Manager", this);
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

void Manager::processReady(SDL_Renderer* renderer) {
  if (!renderer) return;

  SDL_LockMutex(readyMutex);

  while (!ready.empty()) {
    const auto [asset, callback] = std::move(ready.front());
    ready.pop();

    HICL("AssetManager").info("processing ready asset", asset->getCacheKey());
    asset->use(renderer);

    if (callback)
      callback();
  }

  SDL_UnlockMutex(readyMutex);
}

bool Manager::isLoading() const {
  return loading.load(std::memory_order_acquire);
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
