#pragma once

#include <functional>
#include <vector>
#include <optional>
#include <algorithm>

namespace hic {

template<typename ReturnType, typename... Args>
class Event {
public:
  using Callback = std::function<ReturnType(Args...)>;
  using ListenerHandle = size_t;

  bool enabled = true;

  /// @brief Add a listener and get a handle for removal
  /// @return Handle that can be used to remove this listener
  ListenerHandle operator+=(Callback callback) {
    ListenerHandle handle = _nextHandle++;
    _listeners.push_back({handle, callback});
    return handle;
  }

  /// @brief Remove a listener by handle
  void remove(ListenerHandle handle) {
    _listeners.erase(
      std::remove_if(_listeners.begin(), _listeners.end(),
        [handle](const auto& pair) { return pair.first == handle; }),
      _listeners.end()
    );
  }

  /// @brief Call all listeners
  std::vector<ReturnType> operator()(Args... args) {
    std::vector<ReturnType> results;
    if (!enabled) return results;

    results.reserve(_listeners.size());
    for (auto& [handle, listener] : _listeners) {
      results.push_back(listener(args...));
    }

    return results;
  }

  /// @brief Get first result
  std::optional<ReturnType> first(Args... args) {
    if (!_listeners.empty() && enabled) {
      return _listeners.front().second(args...);
    }
    return std::nullopt;
  }

  /// @brief Get last result
  std::optional<ReturnType> last(Args... args) {
    if (!_listeners.empty() && enabled) {
      return _listeners.back().second(args...);
    }
    return std::nullopt;
  }

  void clear() {
    _listeners.clear();
  }

  [[nodiscard]] bool empty() const {
    return _listeners.empty();
  }

  [[nodiscard]] size_t size() const {
    return _listeners.size();
  }

  template<typename Reducer>
  ReturnType reduce(ReturnType initial, Reducer reducer, Args... args) {
    if (!enabled) return initial;

    ReturnType result = initial;
    for (auto& [handle, listener] : _listeners) {
      result = reducer(result, listener(args...));
    }
    return result;
  }

  /// @brief Run until one returns true
  bool trip(Args... args)
    requires std::is_same_v<ReturnType, bool>
  {
    if (!enabled) return false;

    for (auto& [handle, listener] : _listeners) {
      if (listener(args...)) return true;
    }
    return false;
  }

private:
  std::vector<std::pair<ListenerHandle, Callback>> _listeners;
  ListenerHandle _nextHandle = 0;
};

template<typename... Args>
class Event<void, Args...> {
public:
  using Callback = std::function<void(Args...)>;
  using ListenerHandle = size_t;

  bool enabled = true;

  ListenerHandle operator+=(Callback callback) {
    ListenerHandle handle = _nextHandle++;
    _listeners.push_back({handle, callback});
    return handle;
  }

  void remove(ListenerHandle handle) {
    _listeners.erase(
      std::remove_if(_listeners.begin(), _listeners.end(),
        [handle](const auto& pair) { return pair.first == handle; }),
      _listeners.end()
    );
  }

  void operator()(Args... args) {
    if (!enabled) return;

    for (auto& [handle, listener] : _listeners)
      listener(args...);
  }

  void clear() {
    _listeners.clear();
  }

  [[nodiscard]] bool empty() const {
    return _listeners.empty();
  }

  [[nodiscard]] size_t size() const {
    return _listeners.size();
  }

private:
  std::vector<std::pair<ListenerHandle, Callback>> _listeners;
  ListenerHandle _nextHandle = 0;
};

} // namespace hic