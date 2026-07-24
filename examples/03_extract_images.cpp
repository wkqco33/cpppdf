#include "cpppdf/cpppdf.hpp"
#include "cpppdf/document.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./example_03_extract_images <file.pdf>\n";
        return 1;
    }

    cpppdf::PdfDocument doc;
    if (!doc.load(argv[1])) {
        std::cerr << "Error: Failed to load PDF file: " << argv[1] << "\n";
        return 1;
    }

    int total_images = 0;
    for (int i = 0; i < doc.page_count(); ++i) {
        auto images = cpppdf::extract_images(&doc, i);
        std::cout << "Page " << (i + 1) << ": found " << images.size() << " image(s)\n";
        for (size_t idx = 0; idx < images.size(); ++idx) {
            const auto& img = images[idx];
            std::cout << "  Image " << (idx + 1) << ": "
                      << img.width << " x " << img.height << " px, RGBA ("
                      << img.pixels.size() << " bytes)\n";
            total_images++;
        }
    }

    std::cout << "Total extracted images: " << total_images << "\n";
    return 0;
}
