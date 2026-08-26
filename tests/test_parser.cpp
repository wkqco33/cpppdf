#include "cpppdf/cpppdf.hpp"
#include "parser/lexer.hpp"
#include "parser/object_parser.hpp"
#include "parser/xref.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

// ---- 유틸 ----

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

// ---- Lexer 테스트 ----

static void test_lexer_numbers() {
    const char *src = "123 -4 3.14 .5 1.";
    auto data = reinterpret_cast<const uint8_t *>(src);
    cpppdf::parser::Lexer lex(data, strlen(src));

    auto t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Int && t.iv == 123);

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Int && t.iv == -4);

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Real);
    CHECK(t.rv > 3.13 && t.rv < 3.15);

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Real);
    CHECK(t.rv > 0.49 && t.rv < 0.51);

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Real);
    CHECK(t.rv > 0.99 && t.rv < 1.01);
}

static void test_lexer_strings() {
    {
        const char *src = "(hello world)";
        cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));
        auto t = lex.next();
        CHECK(t.type == cpppdf::parser::TokType::StrLit);
        CHECK(t.sv == "hello world");
    }
    {
        const char *src = "(it\\(s)";
        cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));
        auto t = lex.next();
        CHECK(t.type == cpppdf::parser::TokType::StrLit);
        CHECK(t.sv == "it(s");
    }
    {
        const char *src = "<4865 6c6c 6f>";
        cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));
        auto t = lex.next();
        CHECK(t.type == cpppdf::parser::TokType::StrHex);
        CHECK(t.sv == "Hello");
    }
}

static void test_lexer_names() {
    const char *src = "/Type /F#2Foo";
    cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));

    auto t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Name && t.sv == "Type");

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Name);
    CHECK(t.sv == "F/oo");
}

static void test_lexer_keywords() {
    const char *src = "true false null obj endobj R stream endstream";
    cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));

    auto t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Bool && t.bv == true);

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Bool && t.bv == false);

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Null);

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Keyword && t.sv == "obj");

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Keyword && t.sv == "endobj");

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Keyword && t.sv == "R");
}

static void test_lexer_dict() {
    const char *src = "<< /Type /Page >>";
    cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));

    auto t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::DictBegin);

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Name && t.sv == "Type");

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::Name && t.sv == "Page");

    t = lex.next();
    CHECK(t.type == cpppdf::parser::TokType::DictEnd);
}

// ---- ObjectParser 테스트 ----

static void test_parse_dict() {
    const char *src = "<< /Type /Catalog /Version /1.7 >>";
    cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));

    auto t = lex.next(); // <<
    CHECK(t.type == cpppdf::parser::TokType::DictBegin);

    cpppdf::PdfDict d = cpppdf::parser::parse_dict(lex);
    CHECK(d.size() == 2);
    CHECK(d.count("Type") && d.at("Type").is_name());
    CHECK(d.at("Type").s == "Catalog");
}

static void test_parse_indirect_ref() {
    const char *src = "1 0 R";
    cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));

    cpppdf::PdfObject obj = cpppdf::parser::parse_value(lex);
    CHECK(obj.is_ref());
    CHECK(obj.ref_num == 1 && obj.ref_gen == 0);
}

static void test_parse_array() {
    const char *src = "[1 2.5 /Name (hello) true null]";
    cpppdf::parser::Lexer lex(reinterpret_cast<const uint8_t *>(src), strlen(src));

    cpppdf::PdfObject obj = cpppdf::parser::parse_value(lex);
    CHECK(obj.is_array());
    CHECK(obj.arr.size() == 6);
    CHECK(obj.arr[0].is_int() && obj.arr[0].i == 1);
    CHECK(obj.arr[1].is_number());
    CHECK(obj.arr[2].is_name() && obj.arr[2].s == "Name");
    CHECK(obj.arr[3].is_string() && obj.arr[3].s == "hello");
    CHECK(obj.arr[4].is_bool() && obj.arr[4].b == true);
    CHECK(obj.arr[5].is_null());
}

// ---- find_startxref 테스트 ----

static void test_find_startxref() {
    const char *src = "%PDF-1.4\n"
                      "1 0 obj\n<</Type /Catalog>>\nendobj\n"
                      "xref\n0 2\n"
                      "0000000000 65535 f\r\n"
                      "0000000009 00000 n\r\n"
                      "trailer\n<</Size 2 /Root 1 0 R>>\n"
                      "startxref\n"
                      "42\n"
                      "%%EOF\n";

    size_t sz = strlen(src);
    size_t off = cpppdf::parser::find_startxref(reinterpret_cast<const uint8_t *>(src), sz);
    CHECK(off == 42);
}

// ---- 실제 PDF 파일 테스트 (있으면) ----

static void test_open_pdf(const char *path) {
    cpppdf::PdfDocument *doc = cpppdf::open(path);
    if (!doc) {
        fprintf(stderr, "FAIL: cannot open %s\n", path);
        ++g_fail;
        return;
    }

    int n = cpppdf::page_count(doc);
    CHECK(n > 0);
    fprintf(stdout, "  %s: %d page(s)\n", path, n);

    cpppdf::PageInfo info = cpppdf::page_info(doc, 0);
    CHECK(info.width > 0 && info.height > 0);
    fprintf(stdout, "  page 0: %.1f x %.1f pt\n", info.width, info.height);

    cpppdf::close(doc);
}

static void test_open_object_stream_pdf() {
    cpppdf::PdfDocument *doc = cpppdf::open("tests/fixtures/object_stream.pdf");
    if (!doc) {
        fprintf(stderr, "FAIL: cannot open tests/fixtures/object_stream.pdf\n");
        ++g_fail;
        return;
    }

    CHECK(cpppdf::page_count(doc) == 1);

    cpppdf::PageInfo info = cpppdf::page_info(doc, 0);
    CHECK(info.width == 200.0f);
    CHECK(info.height == 200.0f);

    cpppdf::close(doc);
}

// ---- main ----

int main() {
    fprintf(stdout, "=== Lexer ===\n");
    test_lexer_numbers();
    test_lexer_strings();
    test_lexer_names();
    test_lexer_keywords();
    test_lexer_dict();

    fprintf(stdout, "=== ObjectParser ===\n");
    test_parse_dict();
    test_parse_indirect_ref();
    test_parse_array();

    fprintf(stdout, "=== XRef ===\n");
    test_find_startxref();

    fprintf(stdout, "=== PDF files ===\n");
    test_open_pdf("tests/fixtures/sample.pdf");
    test_open_object_stream_pdf();

    fprintf(stdout, "\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
