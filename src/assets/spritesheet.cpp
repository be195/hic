#include "spritesheet.hpp"
#include <regex>
#include <algorithm>
#include <fstream>
#include "../utils/util.hpp"
#include "../utils/logging.hpp"

namespace hic::Assets {

namespace {
  const std::regex BRACE_REGEX(R"(\{(?:(\d+)-(\d+)|(\d+))\})");
  const std::regex MULTIPLY_REGEX(R"(^(?:([^*]+)\*(\d+)|(\d+)\*([^*]+))$)");
  const std::regex BRACE_PATTERN_REGEX(R"(\{[^}]+\})");
}

AnimatedSpritesheetPart::AnimatedSpritesheetPart(Spritesheet* spritesheet, const ssjson::SpritesheetAnimation* animData): spritesheet(spritesheet) {
  assertNotNull(animData, "spritesheet animation passed as a null pointer");
  frameTime = 1000.0f / static_cast<float>(animData->fps);
  normalizeFrames(animData->frames);
  it = frames.begin();
}

std::string padStart(const int num, const int w) {
  std::ostringstream out;
  out << std::setfill('0') << std::setw(w) << num;
  return out.str();
}

std::string trim(std::string input) {
  // ReSharper disable once CppUseRangeAlgorithm
  const auto start = std::find_if_not(input.begin(), input.end(),
    [](const unsigned char ch) { return std::isspace(ch); });
  const auto end = std::find_if_not(input.rbegin(), input.rend(),
    [](const unsigned char ch) { return std::isspace(ch); }).base();
  return (start < end) ? std::string(start, end) : std::string();
}

std::vector<std::vector<std::string>> cartesianProduct(const std::vector<std::vector<std::string>>& sets) {
  if (sets.empty()) return {{}};

  std::vector<std::vector<std::string>> result = {{}};

  for (const auto& set : sets) {
    std::vector<std::vector<std::string>> tmp;
    tmp.reserve(result.size() * set.size());

    for (const auto& item : set) {
      for (const auto& combination : result) {
        auto newCombination = combination;
        newCombination.push_back(item);
        tmp.push_back(newCombination);
      }
    }

    result = std::move(tmp);
  }

  return result;
}

std::vector<std::string> AnimatedSpritesheetPart::expandBraces(const std::string& pattern) {
  std::vector<Match> matches;

  const auto begin = std::sregex_iterator(pattern.begin(), pattern.end(), BRACE_REGEX);
  const auto end = std::sregex_iterator();

  for (auto it = begin; it != end; ++it) {
    const std::smatch& match = *it;
    std::string full = match[0].str();
    std::string fromStr = match[1].str();
    std::string toStr = match[2].str();
    std::string single = match[3].str();

    const size_t sstart = match.position();
    const size_t send = sstart + full.length();

    const int from = fromStr.empty() ? 1 : std::stoi(fromStr);
    const int to = !toStr.empty() ? std::stoi(toStr) :
      !single.empty() ? std::stoi(single) :
      !fromStr.empty() ? std::stoi(fromStr) : 1;

    if (from < 0 || to < 0 || from == to)
      throw std::invalid_argument("invalid range in brace expansion: \"" + full + "\"");

    const int width = std::max({
      0,
      static_cast<int>(fromStr.length()),
      static_cast<int>(toStr.length()),
      static_cast<int>(single.length())
    });

    std::vector<std::string> values;
    const int delta = to > from ? 1 : -1;

    for (int i = from; delta > 0 ? i <= to : i >= to; i += delta)
      values.push_back(padStart(i, width));

    matches.push_back({ full, sstart, send, values });
  }

  if (matches.empty())
    return { pattern };

  std::vector<std::vector<std::string>> parts;
  for (const auto &m : matches)
    parts.push_back(m.values);

  const auto combinations = cartesianProduct(parts);

  std::vector<std::string> results;
  for (const auto& combination : combinations) {
    std::string result = pattern;

    const auto start = std::sregex_iterator(result.begin(), result.end(), BRACE_PATTERN_REGEX);
    std::vector<std::pair<size_t, size_t>> positions;

    for (auto cit = start; cit != end; ++cit)
      positions.push_back({ cit->position(), cit->length() });

    for (int i = positions.size() - 1; i >= 0; --i)
      if (i < static_cast<int>(combination.size()))
        result.replace(positions[i].first, positions[i].second, combination[i]);

    results.push_back(result);
  }

  return results;
}

void AnimatedSpritesheetPart::normalizeFrames(const std::vector<std::string> &given) {
  for (size_t i = 0; i < given.size(); ++i) {
    std::string frame = trim(given[i]);

    // braces expansion
    if (const auto expanded = expandBraces(frame);
      expanded.size() > 1 || expanded[0] != frame) {
      frames.insert(frames.end(), expanded.begin(), expanded.end());
      continue;
    }

    // multiply frames (repeat)
    if (std::smatch repeatMatch; std::regex_match(frame, repeatMatch, MULTIPLY_REGEX)) {
      std::string filename = !repeatMatch[1].str().empty() ?
        repeatMatch[1].str() : repeatMatch[4].str();
      const int count = std::stoi(!repeatMatch[2].str().empty() ?
        repeatMatch[2].str() : repeatMatch[3].str());

      if (count < 1)
        throw std::invalid_argument("repeat count must be >= 1 in \"" + frame + "\" at index " + std::to_string(i));

      for (int j = 0; j < count; ++j)
        frames.push_back(filename);
      continue;
    }

    // empty str -> repeat previous frame
    if (frame.empty()) {
      if (frames.empty())
        throw std::invalid_argument("empty frames");
      frames.push_back(frames.back());
      continue;
    }

    // rollback (<x)
    if (frame[0] == '<') {
      std::string countStr = frame.substr(1);
      int count;

      try {
        count = std::stoi(countStr);
      } catch (...) {
        throw std::invalid_argument("invalid rollback count in animation frame: " + frame);
      }

      if (count < 1)
        throw std::invalid_argument("invalid rollback count in animation frame: " + frame);

      if (static_cast<size_t>(count) > frames.size())
        throw std::out_of_range("stack overflow");

      if (frames.empty())
        throw std::invalid_argument("stack underflow");

      frames.push_back(frames[frames.size() - count]);
      continue;
    }

    frames.push_back(frame);
  }
}

void AnimatedSpritesheetPart::render(SDL_Renderer* renderer, float x, float y) {
  if (it == frames.end() || !spritesheet) return;
  spritesheet->renderFrame(renderer, *it, x, y);
}

void AnimatedSpritesheetPart::update(const float deltaTime) {
  if (frames.empty()) return;
  accumulatedTime += deltaTime;

  while (accumulatedTime >= frameTime) {
    accumulatedTime -= frameTime;
    ++it;

    if (it == frames.end()) {
      it = frames.begin();
      loop();
    }
  }
}

Spritesheet::Spritesheet(std::string folderName) : Image("spritesheets/" + folderName + "/image.png"), folderName(std::move(folderName)) {
  animationMutex = SDL_CreateMutex();
  assertNotNull(animationMutex, "failed to create mutex");
}

Spritesheet::~Spritesheet() {
  if (animationMutex)
    SDL_DestroyMutex(animationMutex);
}

void Spritesheet::preload() {
  Image::preload();

  std::ifstream f("spritesheets/" + folderName + "/data.json");
  if (!f.is_open()) {
    HICL("Spritesheet").warn("failed to open spritesheet json file");
    return;
  }

  data = nlohmann::json::parse(f).get<ssjson::SpritesheetData>();
  if (!data.has_value()) {
    HICL("Spritesheet").warn("failed to load spritesheet data");
    return;
  }

  SDL_LockMutex(animationMutex);

  for (const auto& [name, animationData] : data->animations) {
    try {
      auto anim = std::make_shared<AnimatedSpritesheetPart>(this, &animationData);
      cache[name] = std::move(anim);
    } catch (const std::exception& e) {
      HICL("Spritesheet").warn("failed to load animation", name.c_str(), e.what());
    }
  }

  data->animations.clear();

  SDL_UnlockMutex(animationMutex);
}

void Spritesheet::use(SDL_Renderer *renderer) {
  Image::use(renderer);
}

std::shared_ptr<AnimatedSpritesheetPart> Spritesheet::animation(const std::string &animation) {
  SDL_LockMutex(animationMutex);

  std::shared_ptr<AnimatedSpritesheetPart> result;
  if (const auto it = cache.find(animation); it != cache.end())
    result = it->second;

  SDL_UnlockMutex(animationMutex);

  if (!result)
    HICL("Spritesheet").warn("couldn't find animation", animation.c_str());

  return result;
}

void Spritesheet::renderFrame(SDL_Renderer* renderer, const std::string &frame, const float x, const float y) {
  if (!texture) return;

  SDL_LockMutex(animationMutex);
  const auto frameIt = data->frames.find(frame);
  if (frameIt == data->frames.end()) {
    SDL_UnlockMutex(animationMutex);
    return;
  }
  const ssjson::SpritesheetFrameData frameData = frameIt->second;
  SDL_UnlockMutex(animationMutex);

  SDL_FRect destRect;
  destRect.x = x + frameData.spriteSourceSize.x;
  destRect.y = y + frameData.spriteSourceSize.y;
  destRect.w = frameData.spriteSourceSize.w;
  destRect.h = frameData.spriteSourceSize.h;

  SDL_FRect srcRect;
  srcRect.x = frameData.frame.x;
  srcRect.y = frameData.frame.y;
  srcRect.w = frameData.frame.w;
  srcRect.h = frameData.frame.h;

  SDL_RenderTexture(renderer, texture, &srcRect, &destRect);
}

}
