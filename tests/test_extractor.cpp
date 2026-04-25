#include "cpppdf/cpppdf.hpp"
#include "extractor/text.hpp"
#include "extractor/image.hpp"
#include "document/document.hpp"
#include <cassert>
#include <cstdio>

static int g_pass = 0, g_fail = 0;

#define CHECK(cond) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); } \
} while(0)

static void test_extract_text_sample() {
    cpppdf::PdfDocument doc;
    if (!doc.load("tests/fixtures/sample.pdf")) {
        fprintf(stderr, "SKIP: cannot load sample.pdf\n");
        return;
    }

    auto blocks = cpppdf::extractor::extract_text(doc, 0);
    CHECK(!blocks.empty());

    bool found = false;
    for (const auto& b : blocks) {
        if (b.text.find("Hello") != std::string::npos ||
            b.text.find("cpppdf") != std::string::npos) {
            found = true;
        }
        fprintf(stdout, "  text=%-30s  pos=(%.1f, %.1f)  size=%.1f\n",
                b.text.c_str(), b.x, b.y, b.font_size);
    }
    CHECK(found);
}

static void test_public_api() {
    cpppdf::PdfDocument* doc = cpppdf::open("tests/fixtures/sample.pdf");
    if (!doc) { fprintf(stderr, "SKIP: cannot open sample.pdf\n"); return; }

    auto blocks = cpppdf::extract_text(doc, 0);
    CHECK(!blocks.empty());
    cpppdf::close(doc);
}

// ---- 이미지 추출 테스트 ----

static void test_extract_images() {
    cpppdf::PdfDocument doc;
    if (!doc.load("tests/fixtures/image_test.pdf")) {
        fprintf(stderr, "SKIP: cannot load image_test.pdf\n");
        return;
    }

    auto images = cpppdf::extractor::extract_images(doc, 0);
    CHECK(images.size() == 2);

    // Image 1: 4x4 RGB FlateDecode
    bool found_4x4 = false, found_8x8 = false;
    for (const auto& img : images) {
        fprintf(stdout, "  image: %dx%d  pixels=%zu\n",
                img.width, img.height, img.pixels.size());

        if (img.width == 4 && img.height == 4) {
            found_4x4 = true;
            CHECK(img.pixels.size() == 4u * 4u * 4u); // RGBA
            // 좌상단 픽셀: R=0, G=0, B=128
            CHECK(img.pixels[0] == 0);
            CHECK(img.pixels[1] == 0);
            CHECK(img.pixels[2] == 128);
            CHECK(img.pixels[3] == 255);
            // 우하단 픽셀 (3,3): R=255, G=255, B=128
            size_t last = (4*3 + 3) * 4;
            CHECK(img.pixels[last + 0] == 255);
            CHECK(img.pixels[last + 1] == 255);
            CHECK(img.pixels[last + 2] == 128);
        }

        if (img.width == 8 && img.height == 8) {
            found_8x8 = true;
            CHECK(img.pixels.size() == 8u * 8u * 4u);
            // 첫 픽셀: gray=0 → R=G=B=0
            CHECK(img.pixels[0] == 0);
            CHECK(img.pixels[1] == 0);
            CHECK(img.pixels[2] == 0);
        }
    }
    CHECK(found_4x4);
    CHECK(found_8x8);
}

static void test_extract_images_api() {
    cpppdf::PdfDocument* doc = cpppdf::open("tests/fixtures/image_test.pdf");
    if (!doc) { fprintf(stderr, "SKIP: cannot open image_test.pdf\n"); return; }

    auto images = cpppdf::extract_images(doc, 0);
    CHECK(images.size() == 2);
    cpppdf::close(doc);
}

int main() {
    fprintf(stdout, "=== Text extractor ===\n");
    test_extract_text_sample();

    fprintf(stdout, "=== Public API (text) ===\n");
    test_public_api();

    fprintf(stdout, "=== Image extractor ===\n");
    test_extract_images();

    fprintf(stdout, "=== Public API (images) ===\n");
    test_extract_images_api();

    fprintf(stdout, "\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
