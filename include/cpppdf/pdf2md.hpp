#pragma once
#include "types.hpp"
#include <string>
#include <vector>

namespace cpppdf {
class PdfDocument;
}

namespace cpppdf::converter {

struct Pdf2MdOptions {
    int page_index = -1;
    const std::vector<std::vector<std::string>> *page_image_links = nullptr;
};

std::string convert_blocks_to_markdown(const std::vector<TextBlock> &blocks);
std::string convert_page_to_markdown(const PdfDocument &doc, int page_index);
std::string convert_document_to_markdown(const PdfDocument &doc, int page_index = -1);
std::string convert_document_to_markdown(const PdfDocument &doc, const Pdf2MdOptions &options);

} // namespace cpppdf::converter
