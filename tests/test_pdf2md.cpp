#include "converter/pdf2md.hpp"
#include "document/document.hpp"
#include <cstdio>
#include <string>
#include <vector>

static int g_pass = 0, g_fail = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (cond) {                                                                                \
            ++g_pass;                                                                              \
        } else {                                                                                   \
            ++g_fail;                                                                              \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);                      \
        }                                                                                          \
    } while (0)

static cpppdf::TextBlock block(float x, float y, float font_size, const char *text) {
    cpppdf::TextBlock out;
    out.x = x;
    out.y = y;
    out.font_size = font_size;
    out.text = text;
    return out;
}

static void test_convert_blocks_to_markdown() {
    std::vector<cpppdf::TextBlock> blocks = {
        block(72.0f, 800.0f, 24.0f, "Doc Title"),
        block(72.0f, 760.0f, 12.0f, "First paragraph"),
        block(72.0f, 746.0f, 12.0f, "continues on the next line."),
        block(72.0f, 710.0f, 12.0f, "- Item one"),
        block(72.0f, 696.0f, 12.0f, "- Item two"),
        block(72.0f, 650.0f, 12.0f, "Name"),
        block(200.0f, 650.0f, 12.0f, "Value"),
        block(72.0f, 638.0f, 12.0f, "Alpha"),
        block(200.0f, 638.0f, 12.0f, "10"),
        block(72.0f, 626.0f, 12.0f, "Beta"),
        block(200.0f, 626.0f, 12.0f, "20"),
    };

    const std::string markdown = cpppdf::converter::convert_blocks_to_markdown(blocks);

    CHECK(markdown.find("# Doc Title") != std::string::npos);
    CHECK(markdown.find("First paragraph continues on the next line.") != std::string::npos);
    CHECK(markdown.find("- Item one") != std::string::npos);
    CHECK(markdown.find("- Item two") != std::string::npos);
    CHECK(markdown.find("| Name | Value |") != std::string::npos);
    CHECK(markdown.find("| Alpha | 10 |") != std::string::npos);
    CHECK(markdown.find("| Beta | 20 |") != std::string::npos);
}

static void test_convert_document_sample() {
    cpppdf::PdfDocument doc;
    if (!doc.load("tests/fixtures/sample.pdf")) {
        fprintf(stderr, "FAIL: cannot load sample.pdf\n");
        ++g_fail;
        return;
    }

    const std::string markdown = cpppdf::converter::convert_document_to_markdown(doc, 0);
    CHECK(!markdown.empty());
    CHECK(markdown.find("Hello, cpppdf!") != std::string::npos);
}

static void test_convert_document_with_image_links() {
    cpppdf::PdfDocument doc;
    if (!doc.load("tests/fixtures/sample.pdf")) {
        fprintf(stderr, "FAIL: cannot load sample.pdf\n");
        ++g_fail;
        return;
    }

    std::vector<std::vector<std::string>> page_links = {
        {"sample_images/page0_img0.ppm", "sample_images/page0_img1.ppm"}};
    const std::string markdown = cpppdf::converter::convert_document_to_markdown(
        doc, cpppdf::converter::Pdf2MdOptions{0, &page_links});

    CHECK(markdown.find("![page-1-image-1](sample_images/page0_img0.ppm)") != std::string::npos);
    CHECK(markdown.find("![page-1-image-2](sample_images/page0_img1.ppm)") != std::string::npos);
}

int main() {
    test_convert_blocks_to_markdown();
    test_convert_document_sample();
    test_convert_document_with_image_links();

    fprintf(stdout, "Result: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
