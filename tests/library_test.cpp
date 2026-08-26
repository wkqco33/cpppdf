#include "cpppdf/cpppdf.hpp"
#include "cpppdf/document.hpp"
#include <cstdio>
#include <fstream>
#include <string>

static bool test_null_api() {
    if (cpppdf::page_count(nullptr) != 0)
        return false;
    if (cpppdf::page_info(nullptr, 0).index != 0)
        return false;
    if (!cpppdf::extract_text(nullptr, 0).empty())
        return false;
    if (!cpppdf::extract_images(nullptr, 0).empty())
        return false;
    cpppdf::render_text(nullptr, 0);
    return true;
}

static bool test_invalid_pages(const cpppdf::PdfDocument &doc) {
    for (int index : {-1, 1}) {
        const cpppdf::PageInfo info = doc.page_info(index);
        if (info.index != index || info.width != 0 || info.height != 0)
            return false;
        if (!doc.get_content(index).empty() || !doc.get_resources(index).empty())
            return false;
    }
    return true;
}

int main() {
    if (!test_null_api())
        return 1;

    cpppdf::PdfDocument doc;
    if (!doc.load("tests/fixtures/sample.pdf") || !doc.is_loaded())
        return 1;
    if (doc.page_count() != 1)
        return 1;
    if (!test_invalid_pages(doc))
        return 1;

    if (cpppdf::open("tests/fixtures/does-not-exist.pdf") != nullptr)
        return 1;

    {
        std::ofstream malformed("build/debug/malformed.pdf");
        malformed << "%PDF-1.7\n";
    }
    if (doc.load("build/debug/malformed.pdf"))
        return 1;
    if (doc.is_loaded() || doc.page_count() != 0)
        return 1;
    std::remove("build/debug/malformed.pdf");
    return 0;
}
