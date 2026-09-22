#include "../firmware/Copilot/src/JarvisSpriteRenderer.h"
#include "../firmware/Copilot/src/SpriteStorage.h"
#include "../firmware/Copilot/generated/jarvis_assets.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

namespace {
uint8_t source = 0;
std::vector<std::pair<size_t, size_t>> prefetched;

bool inflate(uint8_t* output, size_t outputSize, const uint8_t*, size_t inputSize,
             size_t, size_t) {
  if (!inputSize) return false;
  std::fill(output, output + outputSize, 0);
  return true;
}
}

namespace copilot {
SpriteBlockLease acquireSdCharacterBlock(size_t, size_t bytes) {
  return bytes ? SpriteBlockLease{&source, -1} : SpriteBlockLease{nullptr, -1};
}

void releaseSdCharacterBlock(const SpriteBlockLease&) {}

void prefetchSdCharacterBlock(size_t offset, size_t bytes) {
  prefetched.emplace_back(offset, bytes);
}
}

int main() {
  using namespace copilot;
  if (!kJarvisDataSize) {
    // The character slot ships empty, so there is no pack metadata to render
    // against. Assert the contract that actually applies in that state: every
    // block is zero length, and the renderer refuses rather than reading from a
    // pack that was never installed.
    for (unsigned index = 0;
         index < unsigned{kJarvisDirections} * kJarvisSteps * kJarvisBlinkLevels; ++index)
      assert(!kJarvisFrames[index].size && !kJarvisFrames[index].offset);
    std::vector<uint16_t> emptyScratch(kCharacterFrameWidth * kFrameHeight);
    std::vector<uint16_t> emptyCached(kCharacterFrameWidth * kCharacterFrameHeight);
    std::vector<uint16_t> emptyFrame(kCharacterFrameWidth * kCharacterFrameHeight);
    JarvisSpriteRenderer empty(emptyScratch.data(), emptyCached.data(), inflate);
    assert(!empty.render(SpritePose{}, 0, emptyFrame.data()));
    std::puts("PASS: empty character slot reports no pack and refuses to draw");
    return 0;
  }
  std::vector<uint16_t> scratch(kCharacterFrameWidth * kFrameHeight);
  std::vector<uint16_t> cached(kCharacterFrameWidth * kCharacterFrameHeight);
  std::vector<uint16_t> frame(kCharacterFrameWidth * kCharacterFrameHeight);
  JarvisSpriteRenderer renderer(scratch.data(), cached.data(), inflate);

  SpritePose pose{};
  assert(renderer.render(pose, 0, frame.data()));
  assert(prefetched.size() == kJarvisBlinkLevels - 1);
  for (unsigned blink = 1; blink < kJarvisBlinkLevels; ++blink) {
    assert(prefetched[blink - 1] == std::make_pair(
        static_cast<size_t>(kJarvisFrames[blink].offset),
        static_cast<size_t>(kJarvisFrames[blink].size)));
  }

  prefetched.clear();
  pose.blinkLevel = 1;
  pose.blinkBlend = 128;
  assert(renderer.render(pose, 0, frame.data()));
  assert(prefetched.size() == 1);
  assert(prefetched[0] == std::make_pair(
      static_cast<size_t>(kJarvisFrames[3].offset),
      static_cast<size_t>(kJarvisFrames[3].size)));

  prefetched.clear();
  pose.index = 4;
  pose.blinkLevel = 0;
  pose.blinkBlend = 0;
  assert(renderer.render(pose, 0, frame.data()));
  assert(prefetched.size() == 1);
  const unsigned movingBlink = pose.index * kJarvisBlinkLevels + 1;
  assert(prefetched[0] == std::make_pair(
      static_cast<size_t>(kJarvisFrames[movingBlink].offset),
      static_cast<size_t>(kJarvisFrames[movingBlink].size)));

  // Unlike OpenClaw, Jarvis has no walk-cycle special case: direction 9
  // ("working") must play back pose.index directly, not a synthesized cycle.
  prefetched.clear();
  pose.direction = 9;
  pose.index = 7;
  pose.blinkLevel = 0;
  pose.blinkBlend = 0;
  assert(renderer.render(pose, 12.5f, frame.data()));
  const unsigned workingBlink = (9u * kJarvisSteps + 7u) * kJarvisBlinkLevels + 1;
  assert(prefetched.size() == 1);
  assert(prefetched[0] == std::make_pair(
      static_cast<size_t>(kJarvisFrames[workingBlink].offset),
      static_cast<size_t>(kJarvisFrames[workingBlink].size)));

  std::puts("PASS: Jarvis prefetches upcoming nonresident blink levels; no walk-cycle override");
}
