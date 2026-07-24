#include "cpppdf/cpppdf.hpp"
#include "cpppdf/document.hpp"
#include "cpppdf/pdf2md.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./example_05_pdf2md <file.pdf> [out.md]\n";
        return 1;
    }

    cpppdf::PdfDocument doc;
    if (!doc.load(argv[1])) {
        std::cerr << "Error: Failed to load PDF file: " << argv[1] << "\n";
        return 1;
    }

    std::string markdown = cpppdf::converter::convert_document_to_markdown(doc);

    if (argc >= 3) {
        std::ofstream out(argv[2], std::ios::binary);
        if (!out) {
            std::cerr << "Error: Failed to open output file: " << argv[2] << "\n";
            return 1;
        }
        out << markdown;
        std::cout << "Successfully saved markdown conversion to " << argv[2] << "\n";
    } else {
        std::cout << markdown;
    }

    return 0;
}
