#pragma once
#include <cstdint>
#include <string>

namespace cpppdf::parser {

enum class TokType : uint8_t {
    Null,
    Bool,
    Int,
    Real,
    StrLit,    // (...)
    StrHex,    // <...>
    Name,      // /Foo
    ArrBegin,  // [
    ArrEnd,    // ]
    DictBegin, // <<
    DictEnd,   // >>
    Keyword,   // obj endobj stream endstream xref trailer startxref R f n
    Eof
};

struct Token {
    TokType type = TokType::Eof;
    size_t pos = 0;  // 버퍼 내 시작 위치 (backtrack용)
    std::string sv;  // StrLit, StrHex, Name, Keyword
    int64_t iv = 0;  // Int
    double rv = 0.0; // Real
    bool bv = false; // Bool
};

class Lexer {
  public:
    Lexer(const uint8_t *data, size_t size, size_t start = 0);

    Token next();
    Token peek();
    bool at_eof() const {
        return pos_ >= size_;
    }
    size_t pos() const {
        return pos_;
    }
    void seek(size_t p) {
        pos_ = p;
        has_peek_ = false;
    }

    // raw bytes 접근 (stream 데이터 추출용)
    const uint8_t *buf() const {
        return buf_;
    }
    size_t bsize() const {
        return size_;
    }

  private:
    const uint8_t *buf_;
    size_t size_;
    size_t pos_ = 0;
    Token peek_tok_;
    bool has_peek_ = false;

    void skip_ws();
    Token read_str_lit();
    Token read_str_hex();
    Token read_name();
    Token read_number_or_kw();

    uint8_t cur() const {
        return pos_ < size_ ? buf_[pos_] : 0;
    }
    bool is_ws(uint8_t c) const;
    bool is_delim(uint8_t c) const;
    bool is_regular(uint8_t c) const {
        return !is_ws(c) && !is_delim(c);
    }
};

} // namespace cpppdf::parser
