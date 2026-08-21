#pragma once

#include <string>

namespace Solar::CupheadPlayer3 {

// Registers the Player 3 Test 2 Mono hook under the hook id requested by the
// existing Cuphead test mod. Test 2 discovers the exact managed surface needed
// to create Player 3 (ID 2) without guessing managed method signatures.
bool RegisterHook(const std::string &hookId);

void Reset();

} // namespace Solar::CupheadPlayer3
