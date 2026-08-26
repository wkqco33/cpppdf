#include "cpppdf/cpppdf.hpp"
#include "document/document.hpp"
#include "extractor/image.hpp"
#include "extractor/text.hpp"
#include "renderer/terminal.hpp"

namespace cpppdf {

PdfDocument *open(const std::string &path) {
    auto *doc = new PdfDocument();
    if (!doc->load(path)) {
        delete doc;
        return nullptr;
    }
    return doc;
}

void close(PdfDocument *doc) {
    delete doc;
}

int page_count(const PdfDocument *doc) {
    if (!doc)
        return 0;
    return doc->page_count();
}

PageInfo page_info(const PdfDocument *doc, int page_index) {
    if (!doc)
        return {};
    return doc->page_info(page_index);
}

std::vector<TextBlock> extract_text(const PdfDocument *doc, int page_index) {
    if (!doc)
        return {};
    return extractor::extract_text(*doc, page_index);
}

std::vector<ImageData> extract_images(const PdfDocument *doc, int page_index) {
    if (!doc)
        return {};
    return extractor::extract_images(*doc, page_index);
}

void render_text(const PdfDocument *doc, int page_index) {
    if (!doc)
        return;
    renderer::render_text(*doc, page_index);
}

void render_image(const ImageData &img, int max_cols, int max_rows) {
    renderer::render_image(img, max_cols, max_rows);
}

} // namespace cpppdf
