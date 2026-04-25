#pragma once
#include "../../include/cpppdf/types.hpp"
#include <vector>

namespace cpppdf {
class PdfDocument;
}

namespace cpppdf::extractor {

std::vector<TextBlock> extract_text(const PdfDocument& doc, int page_index);

} // namespace cpppdf::extractor
