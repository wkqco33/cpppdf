#pragma once
#include "types.hpp"
#include <vector>

namespace cpppdf::converter {

std::vector<Paragraph> detect_tables(std::vector<Paragraph> paragraphs,
                                     const BlockStats& stats);

} // namespace cpppdf::converter
