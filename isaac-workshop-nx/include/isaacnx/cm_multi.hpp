#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace isaacnx {
// Recursively unpack Steam CM EMsg::Multi, optionally gzip-compressed.
// Reject unbounded expansion, deep nesting and malformed child lengths.
bool cm_unpack_messages(const uint8_t* frame, size_t length,
                        std::vector<std::vector<uint8_t>>& messages,
                        std::string& error);
} // namespace isaacnx
