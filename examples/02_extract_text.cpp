#include "cpppdf/cpppdf.hpp"
#include "cpppdf/document.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./example_02_extract_text <file.pdf> [page_index]\n";
        return 1;
    }

    cpppdf::PdfDocument doc;
    if (!doc.load(argv[1])) {
        std::cerr << "Error: Failed to load PDF file: " << argv[1] << "\n";
        return 1;
    }

    int target_page = (argc >= 3) ? std::atoi(argv[2]) : -1;
    int start_page = (target_page >= 0) ? target_page : 0;
    int end_page =
        (target_page >= 0) ? std::min(target_page + 1, doc.page_count()) : doc.page_count();

    for (int p = start_page; p < end_page; ++p) {
        std::cout << "--- Page " << (p + 1) << " ---\n";
        auto blocks = cpppdf::extract_text(&doc, p);

        // Y 내림차순 정렬 (PDF 좌표계 Y축 역순)
        std::sort(blocks.begin(), blocks.end(),
                  [](const cpppdf::TextBlock &a, const cpppdf::TextBlock &b) {
                      if (std::abs(a.y - b.y) > 2.0f) {
                          return a.y > b.y;
                      }
                      return a.x < b.x;
                  });

        float prev_y = -1.0e9f;
        for (const auto &b : blocks) {
            if (prev_y >= 0.0f && std::abs(b.y - prev_y) > 4.0f) {
                std::cout << "\n";
            }
            std::cout << b.text << " ";
            prev_y = b.y;
        }
        std::cout << "\n\n";
    }

    return 0;
}
