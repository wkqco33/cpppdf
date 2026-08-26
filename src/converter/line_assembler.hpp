#pragma once
#include "types.hpp"
#include <vector>

namespace cpppdf::converter {

std::vector<Line> assemble_lines(const NormalizationResult &normalized);

} // namespace cpppdf::converter
