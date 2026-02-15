#pragma once

#include <vector>
#include <string>

namespace hic::UTF8 {

class utf8_error : public std::runtime_error {
public:
  explicit utf8_error(const std::string& msg) : std::runtime_error(msg) {}
};

inline size_t char_byte_count(const unsigned char c) {
  if ((c & 0x80) == 0x00) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  throw utf8_error("invalid UTF-8 start byte");
}

inline size_t length(const std::string& str) {
  size_t count = 0;
  size_t i = 0;

  while (i < str.length()) {
    const unsigned char c = str[i];
    const size_t byte_count = char_byte_count(c);

    if (i + byte_count > str.length())
      throw utf8_error("invalid UTF-8 sequence");

    i += byte_count;
    ++count;
  }

  return count;
}

inline std::string substr(const std::string& str, const size_t start, const size_t count = std::string::npos) {
  size_t byte_start = 0;
  size_t char_index = 0;
  size_t i = 0;

  while (i < str.length() && char_index < start) {
    i += char_byte_count(str[i]);
    ++char_index;
  }

  if (char_index < start)
    throw std::out_of_range("start position out of range");

  byte_start = i;

  if (count == std::string::npos)
    return str.substr(byte_start);

  size_t chars_found = 0;
  while (i < str.length() && chars_found < count) {
    i += char_byte_count(str[i]);
    ++chars_found;
  }

  return str.substr(byte_start, i - byte_start);
}

inline std::string at(const std::string& str, const size_t index) {
  return substr(str, index, 1);
}

inline std::vector<uint32_t> to_codepoints(const std::string& str) {
  std::vector<uint32_t> codepoints;
  size_t i = 0;

  while (i < str.length()) {
    const unsigned char c = str[i];
    const size_t byte_count = char_byte_count(c);

    if (i + byte_count > str.length())
      throw utf8_error("invalid UTF-8 sequence");

    uint32_t codepoint = 0;

    if (byte_count == 1)
      codepoint = c;
    else if (byte_count == 2)
      codepoint = ((c & 0x1F) << 6) | (str[i + 1] & 0x3F);
    else if (byte_count == 3)
      codepoint = ((c & 0x0F) << 12) | 
             ((static_cast<unsigned char>(str[i + 1]) & 0x3F) << 6) | 
             (static_cast<unsigned char>(str[i + 2]) & 0x3F);
    else if (byte_count == 4)
      codepoint = ((c & 0x07) << 18) | 
             ((static_cast<unsigned char>(str[i + 1]) & 0x3F) << 12) | 
             ((static_cast<unsigned char>(str[i + 2]) & 0x3F) << 6) | 
             (static_cast<unsigned char>(str[i + 3]) & 0x3F);

    codepoints.push_back(codepoint);
    i += byte_count;
  }

  return codepoints;
}

inline std::string from_codepoint(const uint32_t codepoint) {
  std::string result;
  
  if (codepoint <= 0x7F) {
    result += static_cast<char>(codepoint);
  } else if (codepoint <= 0x7FF) {
    result += static_cast<char>(0xC0 | (codepoint >> 6));
    result += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0xFFFF) {
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
      throw utf8_error("invalid code point (surrogate)");

    result += static_cast<char>(0xE0 | (codepoint >> 12));
    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    result += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0x10FFFF) {
    result += static_cast<char>(0xF0 | (codepoint >> 18));
    result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    result += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else
    throw utf8_error("invalid code point (out of range)");

  return result;
}

inline std::string from_codepoints(const std::vector<uint32_t>& codepoints) {
  std::string result;

  for (const uint32_t cp : codepoints)
    result += from_codepoint(cp);

  return result;
}

inline size_t char_to_byte_index(const std::string& str, const size_t char_index) {
  size_t byte_pos = 0;
  size_t current_char = 0;

  while (byte_pos < str.length() && current_char < char_index) {
    byte_pos += char_byte_count(str[byte_pos]);
    ++current_char;
  }

  if (current_char < char_index)
    throw std::out_of_range("character index out of range");

  return byte_pos;
}

inline size_t byte_to_char_index(const std::string& str, const size_t byte_index) {
  if (byte_index > str.length())
    throw std::out_of_range("byte index out of range");

  size_t char_count = 0;
  size_t byte_pos = 0;

  while (byte_pos < byte_index) {
    byte_pos += char_byte_count(str[byte_pos]);
    ++char_count;
  }

  return char_count;
}

class iterator {
  const std::string* str_;
  size_t pos_;

public:
  iterator(const std::string* str, const size_t pos) : str_(str), pos_(pos) {}

  std::string operator*() const {
    if (pos_ >= str_->length())
      throw std::out_of_range("iterator out of range");
    const size_t byte_count = char_byte_count((*str_)[pos_]);
    return str_->substr(pos_, byte_count);
  }

  iterator& operator++() {
    if (pos_ < str_->length())
      pos_ += char_byte_count((*str_)[pos_]);
    return *this;
  }

  iterator operator++(int) {
    const iterator tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(const iterator& other) const {
    return str_ == other.str_ && pos_ == other.pos_;
  }

  bool operator!=(const iterator& other) const {
    return !(*this == other);
  }

  size_t byte_position() const { return pos_; }
};

inline iterator begin(const std::string& str) {
  return iterator(&str, 0);
}

inline iterator end(const std::string& str) {
  return iterator(&str, str.length());
}

}