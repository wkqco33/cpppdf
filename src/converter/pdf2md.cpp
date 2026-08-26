#include "pdf2md.hpp"
#include "../document/document.hpp"
#include "../extractor/text.hpp"
#include "line_assembler.hpp"
#include "md_renderer.hpp"
#include "normalizer.hpp"
#include "paragraph_splitter.hpp"
#include "table_detector.hpp"
#include <sstream>

namespace cpppdf::converter {

namespace {

static bool ends_with_two_newlines(const std::string &text) {
    return text.size() >= 2 && text[text.size() - 1] == '\n' && text[text.size() - 2] == '\n';
}

static void append_markdown_with_images(std::ostringstream &out, const std::string &markdown,
                                        const std::vector<std::string> *image_links,
                                        int page_index) {
    out << markdown;

    if (!image_links || image_links->empty())
        return;

    if (!markdown.empty() && markdown.back() != '\n')
        out << '\n';
    if (!ends_with_two_newlines(markdown))
        out << '\n';

    for (size_t i = 0; i < image_links->size(); ++i) {
        out << "![page-" << (page_index + 1) << "-image-" << (i + 1) << "](" << (*image_links)[i]
            << ")\n\n";
    }
}

} // namespace

std::string convert_blocks_to_markdown(const std::vector<TextBlock> &blocks) {
    const NormalizationResult normalized = normalize_blocks(blocks);
    const std::vector<Line> lines = assemble_lines(normalized);
    std::vector<Paragraph> paragraphs = split_paragraphs(lines, normalized.stats);
    paragraphs = detect_tables(std::move(paragraphs), normalized.stats);
    return render_markdown(paragraphs);
}

std::string convert_page_to_markdown(const PdfDocument &doc, int page_index) {
    return convert_blocks_to_markdown(extractor::extract_text(doc, page_index));
}

std::string convert_document_to_markdown(const PdfDocument &doc, int page_index) {
    return convert_document_to_markdown(doc, Pdf2MdOptions{page_index, nullptr});
}

std::string convert_document_to_markdown(const PdfDocument &doc, const Pdf2MdOptions &options) {
    if (options.page_index >= 0) {
        std::ostringstream out;
        const std::vector<std::string> *page_links = nullptr;
        if (options.page_image_links &&
            options.page_index < static_cast<int>(options.page_image_links->size())) {
            page_links = &(*options.page_image_links)[static_cast<size_t>(options.page_index)];
        }
        append_markdown_with_images(out, convert_page_to_markdown(doc, options.page_index),
                                    page_links, options.page_index);
        return out.str();
    }

    std::ostringstream out;
    for (int page = 0; page < doc.page_count(); ++page) {
        if (page > 0 && out.tellp() > 0)
            out << "\n";
        const std::vector<std::string> *page_links = nullptr;
        if (options.page_image_links && page < static_cast<int>(options.page_image_links->size())) {
            page_links = &(*options.page_image_links)[static_cast<size_t>(page)];
        }
        append_markdown_with_images(out, convert_page_to_markdown(doc, page), page_links, page);
    }
    return out.str();
}

} // namespace cpppdf::converter
