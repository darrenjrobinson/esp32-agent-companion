#include "JarvisSpriteRenderer.h"
#include "Character.h"
#include "Config.h"
#include "SpriteStorage.h"
#include "../generated/jarvis_assets.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace copilot {
namespace {
constexpr size_t kPixels = static_cast<size_t>(kCharacterFrameWidth) * kFrameHeight;
}

bool JarvisSpriteRenderer::decode(unsigned blockIndex, uint16_t* output) {
  if (blockIndex >= sizeof(kJarvisFrames) / sizeof(kJarvisFrames[0])) {
    error_ = "Jarvis sprite block index is invalid.";
    return false;
  }
  const auto block = kJarvisFrames[blockIndex];
  const auto source = acquireSdCharacterBlock(block.offset, block.size);
  if (!source.data) {
    error_ = "Jarvis SD sprite source is unavailable.";
    return false;
  }
  const bool decoded = block.size && inflate_(
      reinterpret_cast<uint8_t*>(output), kPixels * sizeof(uint16_t),
      source.data, block.size, kCharacterFrameWidth, kCharacterFrameWidth);
  releaseSdCharacterBlock(source);
  if (!decoded) error_ = "Jarvis sprite decompression failed.";
  return decoded;
}

void JarvisSpriteRenderer::prefetch(
    unsigned direction, unsigned index, unsigned blinkLevel) {
  if (direction >= kJarvisDirections || index >= kJarvisSteps
      || blinkLevel >= kJarvisBlinkLevels) return;
  const auto block = kJarvisFrames[
      (direction * kJarvisSteps + index) * kJarvisBlinkLevels + blinkLevel];
  prefetchSdCharacterBlock(block.offset, block.size);
}

bool JarvisSpriteRenderer::render(
    const SpritePose& pose, float effectSeconds, uint16_t* frame) {
  error_ = nullptr;
  if (!scratch_ || !cached_ || !inflate_ || !frame) {
    error_ = "Jarvis renderer requires initialized buffers.";
    return false;
  }
  if (pose.direction >= kJarvisDirections || pose.index >= kJarvisSteps
      || pose.blinkLevel >= kJarvisBlinkLevels
      || (pose.blinkBlend && pose.blinkLevel + 1 >= kJarvisBlinkLevels)
      || !std::isfinite(effectSeconds) || effectSeconds < 0) {
    error_ = "Invalid Jarvis sprite pose.";
    return false;
  }
  constexpr int top = (kCharacterFrameHeight - kFrameHeight) / 2;
  // Unlike OpenClaw's "working" track, Jarvis's art is static-per-pose (no
  // baked-in walk cycle), so the requested index is used directly.
  const unsigned index = pose.index;
  const unsigned frameIndex = pose.direction * kJarvisSteps + index;
  const unsigned blockIndex = frameIndex * kJarvisBlinkLevels + pose.blinkLevel;
  const uint32_t key = blockIndex * 256 + pose.blinkBlend;
  const auto prefetchUpcomingBlink = [&]() {
    if (!pose.blinkLevel && !pose.blinkBlend) {
      const unsigned last = index == 0 ? kJarvisBlinkLevels - 1 : 1;
      for (unsigned blink = 1; blink <= last; ++blink)
        prefetch(pose.direction, index, blink);
      return;
    }
    prefetch(pose.direction, index, std::min<unsigned>(
        kJarvisBlinkLevels - 1, pose.blinkLevel + (pose.blinkBlend ? 2 : 1)));
  };
  if (cacheKey_ == key) {
    std::memcpy(frame, cached_,
                kCharacterFrameWidth * kCharacterFrameHeight * sizeof(uint16_t));
    prefetchUpcomingBlink();
    return true;
  }
  uint16_t* cachedArt = cached_ + top * kCharacterFrameWidth;
  if (!decode(blockIndex, cachedArt)) return false;
  std::memset(cached_, 0, top * kCharacterFrameWidth * sizeof(uint16_t));
  std::memset(cached_ + (top + kFrameHeight) * kCharacterFrameWidth, 0,
              top * kCharacterFrameWidth * sizeof(uint16_t));
  if (pose.blinkBlend) {
    if (!decode(blockIndex + 1, scratch_)) return false;

    int first = kFrameHeight, last = -1;
    for (int y = 0; y < kFrameHeight; ++y) {
      if (std::memcmp(cachedArt + y * kCharacterFrameWidth,
                      scratch_ + y * kCharacterFrameWidth,
                      kCharacterFrameWidth * sizeof(uint16_t)) != 0) {
        first = std::min(first, y);
        last = y;
      }
    }
    if (last >= first) {
      const int height = last - first + 1;
      const int revealHalfHeight = std::max(
          1, static_cast<int>((static_cast<unsigned>(pose.blinkBlend)
              * (height + 1) / 2 + 254) / 255));
      const int center = (first + last) / 2;
      for (int y = first; y <= last; ++y) {
        if (std::abs(y - center) >= revealHalfHeight)
          std::memcpy(cachedArt + y * kCharacterFrameWidth,
                      scratch_ + y * kCharacterFrameWidth,
                      kCharacterFrameWidth * sizeof(uint16_t));
      }
    }
  }
  cacheKey_ = key;
  std::memcpy(frame, cached_,
              kCharacterFrameWidth * kCharacterFrameHeight * sizeof(uint16_t));
  prefetchUpcomingBlink();
  return true;
}
}
