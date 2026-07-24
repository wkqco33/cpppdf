#include "cpppdf/cpppdf.hpp"
#include "cpppdf/document.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./example_04_render_terminal <file.pdf> [page_index]\n";
        return 1;
    }

    cpppdf::PdfDocument doc;
    if (!doc.load(argv[1])) {
        std::cerr << "Error: Failed to load PDF file: " << argv[1] << "\n";
        return 1;
    }

    int page = (argc >= 3) ? std::atoi(argv[2]) : 0;
    if (page < 0 || page >= doc.page_count()) {
        std::cerr << "Error: Page index out of range (0 .. " << (doc.page_count() - 1) << ")\n";
        return 1;
    }

    std::cout << "=== Terminal Render (Page " << (page + 1) << ") ===\n";
    cpppdf::render_text(&doc, page);

    return 0;
}
