#pragma once

#include "geometry.hpp"
#include "easing.hpp"
#include <vector>
#include <functional>

namespace hic {

template<typename Target>
struct BaseTween {
  Target* target;
  Target from;
  Target to;
  float duration{};
  float elapsed = 0.0f;
  EasingFunction easing;
  std::function<void()> complete;
};

struct Tween : BaseTween<float> {};
struct PositionTween : BaseTween<Position> {};

struct Waiter {
  float time;
  std::function<void()> complete;
};

class AnimationMixin {
public:
  void updateAnimations(const float deltaTime) {
    // prop tweens
    for (auto it = tweens.begin(); it != tweens.end();) {
      it->elapsed += deltaTime;
      const float progress = std::min(it->elapsed / it->duration, 1.0f);
      const float eased = it->easing(progress);

      *it->target = it->from + (it->to - it->from) * eased;

      if (progress >= 1.0f) {
        *it->target = it->to;
        if (it->complete) it->complete();
        it = tweens.erase(it);
      } else
        ++it;
    }

    // position tweens
    for (auto it = positionTweens.begin(); it != positionTweens.end();) {
      it->elapsed += deltaTime;
      const float progress = std::min(it->elapsed / it->duration, 1.0f);
      const float eased = it->easing(progress);

      it->target->x = it->from.x + (it->to.x - it->from.x) * eased;
      it->target->y = it->from.y + (it->to.y - it->from.y) * eased;

      if (progress >= 1.0f) {
        *it->target = it->to;
        if (it->complete) it->complete();
        it = positionTweens.erase(it);
      } else
        ++it;
    }

    for (auto it = waiters.begin(); it != waiters.end();) {
      it->time -= deltaTime;
      if (it->time <= 0.0f) {
        if (it->complete) it->complete();
        it = waiters.erase(it);
      } else
        ++it;
    }
  }

  void tweenProp(float* target, const float from, const float to, const float duration,
    const EasingFunction& easing = Easing::linear, const std::function<void()>& complete = nullptr)
  {
    if (duration <= 0.0f || from == to) {
      *target = to;
      if (complete) complete();
      return;
    }

    tweens.push_back({
      target,
      from,
      to,
      duration,
      0.0f,
      easing,
      complete
    });
  }

  void tweenTo(Position* target, const Position from, const Position to, const float duration,
    const EasingFunction &easing = Easing::linear, const std::function<void()>& complete = nullptr)
  {
    if (duration <= 0.0f) {
      *target = to;
      if (complete) complete();
      return;
    }

    positionTweens.push_back({
      target,
      from,
      to,
      duration,
      0.0f,
      easing,
      complete
    });
  }

  void wait(const float duration, const std::function<void()>& callback) {
    waiters.push_back({ duration, callback });
  }

  void clearAnimations() {
    tweens.clear();
    positionTweens.clear();
    waiters.clear();
  }
protected:
  std::vector<Tween> tweens;
  std::vector<PositionTween> positionTweens;
  std::vector<Waiter> waiters;
};

}