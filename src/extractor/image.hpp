#pragma once
#include "../../include/cpppdf/types.hpp"
#include <vector>

namespace cpppdf {
class PdfDocument;
}

namespace cpppdf::extractor {

std::vector<ImageData> extract_images(const PdfDocument& doc, int page_index);

} // namespace cpppdf::extractor
