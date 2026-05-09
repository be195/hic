#pragma once

#include "events.hpp"
#include "hicapi.hpp"
#include <SDL3/SDL_rect.h>

namespace hic {

struct HIC_API Position {
  float x = 0.0f;
  float y = 0.0f;

  Position() = default;
  Position(const float x, const float y) : x(x), y(y) {}

  Position operator+(const Position& other) const {
    return {x + other.x, y + other.y};
  }
};

struct HIC_API Size {
  float w = 0.0f;
  float h = 0.0f;

  Size() = default;
  Size(const float w, const float h) : w(w), h(h) {}

  [[nodiscard]] float area() const { return w * h; }
};

class HIC_API Rectangle {
public:
  Event<void, const char*, float, float> change;

  Rectangle() = default;
  Rectangle(const float x, const float y, const float w, const float h)
    : pos_(x, y), size_(w, h) {}

  Rectangle(const Rectangle& other) : pos_(other.pos_), size_(other.size_) {}
  Rectangle& operator=(const Rectangle& other) {
    if (this != &other) {
      pos_ = other.pos_;
      size_ = other.size_;
    }
    return *this;
  }

  [[nodiscard]] float x() const { return pos_.x; }
  [[nodiscard]] float y() const { return pos_.y; }

  Rectangle& setX(const float x) {
    if (x != pos_.x) {
      pos_.x = x;
      change("x", pos_.x, x);
    }
    return *this;
  }

  Rectangle& setY(const float y) {
    if (y != pos_.y) {
      pos_.y = y;
      change("y", pos_.y, y);
    }
    return *this;
  }

  Rectangle& setPos(const Position& pos) {
    return setX(pos.x).setY(pos.y);
  }

  [[nodiscard]] float w() const { return size_.w; }
  [[nodiscard]] float h() const { return size_.h; }
  void w(const float newW) {
    setW(newW);
  }
  void h(const float newH) {
    setH(newH);
  }

  Rectangle& setW(const float w) {
    if (w != size_.w) {
      size_.w = w;
      change("w", size_.w, w);
    }
    return *this;
  }

  Rectangle& setH(const float h) {
    if (h != size_.h) {
      size_.h = h;
      change("h", size_.h, h);
    }
    return *this;
  }

  Rectangle& setSize(const Size& size) {
    return setW(size.w).setH(size.h);
  }

  Rectangle& setBounds(const float x, const float y, const float w, const float h) {
    return setX(x).setY(y).setW(w).setH(h);
  }

  [[nodiscard]] const Position& pos() const { return pos_; }
  [[nodiscard]] const Position& fpos() const {
    return { std::floorf(pos_.x), std::floorf(pos_.y) };
  }
  [[nodiscard]] const Size& size() const { return size_; }
  [[nodiscard]] const Size& fsize() const {
    return { std::floorf(size_.w), std::floorf(size_.h) };
  }

  [[nodiscard]] const SDL_FRect& toSDLFRect() const {
    return { pos_.x, pos_.y, size_.w, size_.h };
  }

  [[nodiscard]] const SDL_Rect& toSDLRect() const {
    return {
      static_cast<int>(std::floorf(pos_.x)),
      static_cast<int>(std::floorf(pos_.y)),
      static_cast<int>(std::floorf(size_.w)),
      static_cast<int>(std::floorf(size_.h)),
    };
  }

  [[nodiscard]] const SDL_FRect& floorToSDLFRect() const {
    return {
      std::floorf(pos_.x),
      std::floorf(pos_.y),
      std::floorf(size_.w),
      std::floorf(size_.h),
    };
  }

  [[nodiscard]] const SDL_Rect& floorToSDLRect() const {
    return {
      static_cast<int>(std::floorf(pos_.x)),
      static_cast<int>(std::floorf(pos_.y)),
      static_cast<int>(std::floorf(size_.w)),
      static_cast<int>(std::floorf(size_.h)),
    };
  }

  [[nodiscard]] bool operator!=(const Rectangle& other) const {
    return !(*this == other);
  }

  [[nodiscard]] bool operator==(const Rectangle& other) const {
    return pos_.x == other.pos_.x && pos_.y == other.pos_.y &&
           size_.w == other.size_.w && size_.h == other.size_.h;
  }

  [[nodiscard]] bool contains(const float px, const float py) const {
    return px >= pos_.x && px < pos_.x + size_.w &&
           py >= pos_.y && py < pos_.y + size_.h;
  }

  [[nodiscard]] bool overlaps(const Rectangle& other) const {
    return !(pos_.x + size_.w <= other.pos_.x ||
             other.pos_.x + other.size_.w <= pos_.x ||
             pos_.y + size_.h <= other.pos_.y ||
             other.pos_.y + other.size_.h <= pos_.y);
  }

  [[nodiscard]] Position center() const {
    return {pos_.x + size_.w / 2.0f, pos_.y + size_.h / 2.0f};
  }

  [[nodiscard]] float area() const { return size_.area(); }

private:
  Position pos_;
  Size size_;
};

} // namespace hic