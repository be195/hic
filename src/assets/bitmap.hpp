#pragma once

#include <string>
#include "base.hpp"
#include "../utils/bmfont.hpp"

namespace hic::Assets {

class HIC_API BitmapFont : public Base {
public:
  std::string folderName;

  explicit BitmapFont(std::string folderName);
  ~BitmapFont() override;

  std::string getCacheKey() const override { return folderName; }
private:
  std::unique_ptr<BMFont> font;
};

}
