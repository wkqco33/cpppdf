#pragma once
#include "types.hpp"
#include <string>
#include <vector>

namespace cpppdf::converter {

std::string render_markdown(const std::vector<Paragraph> &paragraphs);

} // namespace cpppdf::converter
