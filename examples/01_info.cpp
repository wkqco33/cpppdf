#include "cpppdf/cpppdf.hpp"
#include "cpppdf/document.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./example_01_info <file.pdf>\n";
        return 1;
    }

    cpppdf::PdfDocument doc;
    if (!doc.load(argv[1])) {
        std::cerr << "Error: Failed to load PDF file: " << argv[1] << "\n";
        return 1;
    }

    std::cout << "=== PDF Info ===\n";
    std::cout << "File: " << argv[1] << "\n";
    std::cout << "Page count: " << doc.page_count() << "\n\n";

    constexpr float kMmPerInch = 25.4f;
    constexpr float kPtsPerInch = 72.0f;

    for (int i = 0; i < doc.page_count(); ++i) {
        cpppdf::PageInfo info = doc.page_info(i);
        float mm_w = info.width * kMmPerInch / kPtsPerInch;
        float mm_h = info.height * kMmPerInch / kPtsPerInch;
        std::cout << "  Page " << (i + 1) << ": "
                  << info.width << " x " << info.height << " pt ("
                  << mm_w << " x " << mm_h << " mm)\n";
    }

    return 0;
}
