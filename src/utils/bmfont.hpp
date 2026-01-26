#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

// see: https://www.angelcode.com/products/bmfont/doc/file_format.html#bin
class BMFont {
private:
  static uint8_t read_uint8(std::ifstream& file) {
    uint8_t value;
    if (!file.read(reinterpret_cast<char*>(&value), 1))
      throw std::overflow_error("could not continue to read the opened file");
    return value;
  }

  static int8_t read_int8(std::ifstream& file) {
    int8_t value;
    if (!file.read(reinterpret_cast<char*>(&value), 1))
      throw std::overflow_error("could not continue to read the opened file");
    return value;
  }

  static uint16_t read_uint16(std::ifstream& file) {
    uint8_t bytes[2];
    if (!file.read(reinterpret_cast<char*>(bytes), 2))
      throw std::overflow_error("could not continue to read the opened file");
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8);
  }

  static int16_t read_int16(std::ifstream& file) {
    return static_cast<int16_t>(read_uint16(file));
  }

  static uint32_t read_uint32(std::ifstream& file) {
    uint8_t bytes[4];
    if (!file.read(reinterpret_cast<char*>(bytes), 4))
      throw std::overflow_error("could not continue to read the opened file");
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
  }

  static int32_t read_int32(std::ifstream& file) {
    return static_cast<int32_t>(read_uint32(file));
  }

  static std::string read_string(std::ifstream& file) {
    // read until zero byte
    std::string value;
    char c;
    while (file.get(c) && c != '\0')
      value.push_back(c);
    return value;
  }

  void readInfoBlock(std::ifstream& file) {
    info.fontSize = read_int16(file);
    info.bitField = read_uint8(file);
    info.charSet = read_uint8(file);
    info.stretchH = read_uint16(file);
    info.aa = read_uint8(file);
    info.paddingUp = read_uint8(file);
    info.paddingRight = read_uint8(file);
    info.paddingDown = read_uint8(file);
    info.paddingLeft = read_uint8(file);
    info.spacingHoriz = read_uint8(file);
    info.spacingVert = read_uint8(file);
    info.outline = read_uint8(file);
    info.name = read_string(file);
  }

  void readCommonBlock(std::ifstream& file) {
    common.lineHeight = read_uint16(file);
    common.base = read_uint16(file);
    common.scaleW = read_uint16(file);
    common.scaleH = read_uint16(file);
    common.pages = read_uint16(file);
    common.bitField = read_uint8(file);
    common.alphaCh = read_uint8(file);
    common.redCh = read_uint8(file);
    common.greenCh = read_uint8(file);
    common.blueCh = read_uint8(file);
  }

  void readPagesBlock(std::ifstream& file, const uint32_t size) {
    pages.clear();

    uint32_t read = 0;
    while (read < size) {
      std::string page = read_string(file);
      read += page.length() + 1; // 1 byte for null
      pages.push_back(page);
    }
  }

  void readCharsBlock(std::ifstream& file, const uint32_t size) {
    const uint32_t charsN = size / 20;
    chars.clear(); chars.reserve(charsN);

    for (uint32_t i = 0; i < charsN; i++) {
      CharBlock block;
      block.id = read_uint32(file);
      block.x = read_uint16(file);
      block.y = read_uint16(file);
      block.width = read_uint16(file);
      block.height = read_uint16(file);
      block.xOffset = read_int16(file);
      block.yOffset = read_int16(file);
      block.xAdvance = read_int16(file);
      block.page = read_uint8(file);
      block.chnl = read_uint8(file);
      chars.insert({block.id, block});
    }
  }

  void readKernBlock(std::ifstream& file, const uint32_t size) {
    const uint32_t kernelN = size / 10;
    kernels.clear(); kernels.reserve(kernelN);

    for (uint32_t i = 0; i < kernelN; i++) {
      const uint32_t first = read_uint32(file);
      const uint32_t second = read_uint32(file);

      uint64_t key = (static_cast<uint64_t>(first) << 32) | second;
      kernels.insert({key, read_int16(file)});
    }
  }
public:
  struct InfoBlock {
    int16_t fontSize;
    uint8_t bitField;
    uint8_t charSet;
    uint16_t stretchH;
    uint8_t aa;
    uint8_t paddingUp;
    uint8_t paddingRight;
    uint8_t paddingDown;
    uint8_t paddingLeft;
    uint8_t spacingHoriz;
    uint8_t spacingVert;
    uint8_t outline;
    std::string name;
  };

  struct CommonBlock {
    uint16_t lineHeight;
    uint16_t base;
    uint16_t scaleW;
    uint16_t scaleH;
    uint16_t pages;
    uint8_t bitField;
    uint8_t alphaCh;
    uint8_t redCh;
    uint8_t greenCh;
    uint8_t blueCh;
  };

  struct CharBlock {
    uint32_t id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t xOffset;
    int16_t yOffset;
    int16_t xAdvance;
    uint8_t page;
    uint8_t chnl;
  };

  struct KernBlock {
    uint32_t first;
    uint32_t second;
    int16_t amount;
  };

  InfoBlock info;
  CommonBlock common;
  std::vector<std::string> pages;
  std::unordered_map<uint32_t, CharBlock> chars;

  // where key is (first << 32) | second
  std::unordered_map<uint64_t, int16_t> kernels;

  bool load(const std::string& filename) {
    std::ifstream file;
    file.open(filename, std::ios::binary);
    if (!file.is_open())
      throw std::invalid_argument("could not open file");

    // BMF tag check
    char header[3];
    if (!file.read(header, 3))
      throw std::invalid_argument("failed to read the first 3 bytes");
    if (header[0] != 'B' || header[1] != 'M' || header[2] != 'F') {
      file.close();
      throw std::invalid_argument("unrecognizable font file");
    }

    // version check
    if (read_uint8(file) != 3) {
      file.close();
      throw std::invalid_argument("bad version, only v3 is supported");
    }

    uint8_t type;
    while (file.read(reinterpret_cast<char*>(&type), 1)) {
      const uint32_t size = read_uint32(file);

      const auto startPos = file.tellg();
      switch (type) {
        case 1: readInfoBlock(file); break;
        case 2: readCommonBlock(file); break;
        case 3: readPagesBlock(file, size); break;
        case 4: readCharsBlock(file, size); break;
        case 5: readKernBlock(file, size); break;
        default: break; // we're going to seek anyway
      }
      file.seekg(startPos + static_cast<std::streamoff>(size), std::ios::beg);
    }

    file.close();
    return true;
  }

  bool tryLoad(const std::string& filename) {
    try {
      return load(filename);
    } catch (...) {
      return false;
    }
  }
};
