#pragma once
#include "types.hpp"
#include <vector>

namespace cpppdf::converter {

std::vector<Paragraph> split_paragraphs(const std::vector<Line>& lines,
                                        const BlockStats& stats);

} // namespace cpppdf::converter
