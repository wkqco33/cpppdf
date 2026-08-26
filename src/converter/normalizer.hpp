#pragma once
#include "types.hpp"
#include <vector>

namespace cpppdf::converter {

NormalizationResult normalize_blocks(const std::vector<TextBlock> &blocks);

} // namespace cpppdf::converter
