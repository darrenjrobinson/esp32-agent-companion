#pragma once
#include "Character.h"
#include <cstddef>
#include <cstdint>

namespace copilot {
// Initialize once before starting rendering; the read-only mapping lives for the application's lifetime.
bool initializeSpriteStorage();
const char* spriteStorageError();

struct SpriteBlockLease {
  const uint8_t* data;
  int slot;
};
struct SdSpriteStatus {
  const char* state;
  const char* detail;
  const char* cardType;
  uint64_t capacityBytes;
  uint32_t cacheBytes, hits, misses, revision;
};

SdSpriteStatus sdSpriteStatus();
SpriteBlockLease acquireSpriteBlock(size_t offset, size_t bytes);
void releaseSpriteBlock(const SpriteBlockLease& block);

// SD-streamed characters (OpenClaw, Jarvis) share a single resident pack
// slot: only one such character's pack is ever loaded at a time, and
// loading a different one closes the previous pack first. Copilot itself
// has no SD pack; loading it clears whatever pack was active.
void loadSdCharacterPack(CharacterId character);
bool sdCharacterAvailable();
bool prepareSdCharacterUpdate();
SpriteBlockLease acquireSdCharacterBlock(size_t offset, size_t bytes);
void prefetchSdCharacterBlock(size_t offset, size_t bytes);
void releaseSdCharacterBlock(const SpriteBlockLease& block);
}
