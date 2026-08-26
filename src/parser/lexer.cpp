#include "lexer.hpp"
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace cpppdf::parser {

Lexer::Lexer(const uint8_t *data, size_t size, size_t start)
    : buf_(data), size_(size), pos_(start) {}

bool Lexer::is_ws(uint8_t c) const {
    return c == 0x00 || c == 0x09 || c == 0x0A || c == 0x0C || c == 0x0D || c == 0x20;
}

bool Lexer::is_delim(uint8_t c) const {
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' ||
           c == '}' || c == '/' || c == '%';
}

void Lexer::skip_ws() {
    while (pos_ < size_) {
        uint8_t c = buf_[pos_];
        if (is_ws(c)) {
            ++pos_;
        } else if (c == '%') {
            // 주석: 줄 끝까지 스킵
            while (pos_ < size_ && buf_[pos_] != '\n' && buf_[pos_] != '\r')
                ++pos_;
        } else {
            break;
        }
    }
}

Token Lexer::peek() {
    if (!has_peek_) {
        peek_tok_ = next();
        has_peek_ = true;
    }
    return peek_tok_;
}

Token Lexer::next() {
    if (has_peek_) {
        has_peek_ = false;
        return peek_tok_;
    }

    skip_ws();

    Token t;
    t.pos = pos_;

    if (pos_ >= size_) {
        t.type = TokType::Eof;
        return t;
    }

    uint8_t c = buf_[pos_];

    if (c == '(') {
        ++pos_;
        return read_str_lit();
    }

    if (c == '<') {
        if (pos_ + 1 < size_ && buf_[pos_ + 1] == '<') {
            pos_ += 2;
            t.type = TokType::DictBegin;
            return t;
        }
        ++pos_;
        return read_str_hex();
    }

    if (c == '>') {
        if (pos_ + 1 < size_ && buf_[pos_ + 1] == '>') {
            pos_ += 2;
            t.type = TokType::DictEnd;
            return t;
        }
        // 단독 > 는 비정상이지만 skip하고 재시도
        ++pos_;
        return next();
    }

    if (c == '[') {
        ++pos_;
        t.type = TokType::ArrBegin;
        return t;
    }
    if (c == ']') {
        ++pos_;
        t.type = TokType::ArrEnd;
        return t;
    }

    if (c == '/') {
        ++pos_;
        return read_name();
    }

    return read_number_or_kw();
}

Token Lexer::read_str_lit() {
    // '(' 는 이미 소비됨
    Token t;
    t.type = TokType::StrLit;
    t.pos = pos_ - 1;

    int depth = 1;
    while (pos_ < size_ && depth > 0) {
        uint8_t ch = buf_[pos_++];

        if (ch == '(') {
            ++depth;
            t.sv += '(';
        } else if (ch == ')') {
            if (--depth > 0)
                t.sv += ')';
        } else if (ch == '\\') {
            if (pos_ >= size_)
                break;
            uint8_t esc = buf_[pos_++];
            switch (esc) {
            case 'n':
                t.sv += '\n';
                break;
            case 'r':
                t.sv += '\r';
                break;
            case 't':
                t.sv += '\t';
                break;
            case 'b':
                t.sv += '\b';
                break;
            case 'f':
                t.sv += '\f';
                break;
            case '(':
                t.sv += '(';
                break;
            case ')':
                t.sv += ')';
                break;
            case '\\':
                t.sv += '\\';
                break;
            case '\r':
                if (pos_ < size_ && buf_[pos_] == '\n')
                    ++pos_;
                break;
            case '\n':
                break; // 줄 계속
            default:
                if (esc >= '0' && esc <= '7') {
                    int oct = esc - '0';
                    for (int k = 0; k < 2 && pos_ < size_; k++) {
                        uint8_t nc = buf_[pos_];
                        if (nc < '0' || nc > '7')
                            break;
                        oct = oct * 8 + (nc - '0');
                        ++pos_;
                    }
                    t.sv += static_cast<char>(oct & 0xFF);
                } else {
                    t.sv += static_cast<char>(esc);
                }
            }
        } else {
            t.sv += static_cast<char>(ch);
        }
    }
    return t;
}

Token Lexer::read_str_hex() {
    // '<' 는 이미 소비됨
    Token t;
    t.type = TokType::StrHex;
    t.pos = pos_ - 1;

    std::string hex;
    while (pos_ < size_ && buf_[pos_] != '>') {
        uint8_t ch = buf_[pos_++];
        if (std::isxdigit(ch))
            hex += static_cast<char>(ch);
        // 공백 무시
    }
    if (pos_ < size_)
        ++pos_; // '>' 소비

    if (hex.size() % 2 != 0)
        hex += '0'; // 홀수 패딩

    t.sv.reserve(hex.size() / 2);
    for (size_t k = 0; k < hex.size(); k += 2) {
        auto from_hex = [](char ch) -> uint8_t {
            if (ch >= '0' && ch <= '9')
                return static_cast<uint8_t>(ch - '0');
            if (ch >= 'a' && ch <= 'f')
                return static_cast<uint8_t>(ch - 'a' + 10);
            return static_cast<uint8_t>(ch - 'A' + 10);
        };
        uint8_t hi = from_hex(hex[k]);
        uint8_t lo = from_hex(hex[k + 1]);
        t.sv += static_cast<char>((hi << 4) | lo);
    }
    return t;
}

Token Lexer::read_name() {
    // '/' 는 이미 소비됨
    Token t;
    t.type = TokType::Name;
    t.pos = pos_ - 1;

    while (pos_ < size_ && is_regular(buf_[pos_])) {
        uint8_t ch = buf_[pos_];
        if (ch == '#' && pos_ + 2 < size_) {
            uint8_t h = buf_[pos_ + 1], l = buf_[pos_ + 2];
            if (std::isxdigit(h) && std::isxdigit(l)) {
                auto from_hex = [](uint8_t c) -> uint8_t {
                    if (c >= '0' && c <= '9')
                        return static_cast<uint8_t>(c - '0');
                    if (c >= 'a' && c <= 'f')
                        return static_cast<uint8_t>(c - 'a' + 10);
                    return static_cast<uint8_t>(c - 'A' + 10);
                };
                t.sv += static_cast<char>((from_hex(h) << 4) | from_hex(l));
                pos_ += 3;
                continue;
            }
        }
        t.sv += static_cast<char>(ch);
        ++pos_;
    }
    return t;
}

Token Lexer::read_number_or_kw() {
    Token t;
    t.pos = pos_;

    std::string raw;
    while (pos_ < size_ && is_regular(buf_[pos_]))
        raw += static_cast<char>(buf_[pos_++]);

    if (raw.empty()) {
        // 알 수 없는 문자 스킵 후 재시도
        ++pos_;
        return next();
    }

    // bool / null
    if (raw == "true") {
        t.type = TokType::Bool;
        t.bv = true;
        return t;
    }
    if (raw == "false") {
        t.type = TokType::Bool;
        t.bv = false;
        return t;
    }
    if (raw == "null") {
        t.type = TokType::Null;
        return t;
    }

    // 알려진 키워드
    if (raw == "obj" || raw == "endobj" || raw == "stream" || raw == "endstream" || raw == "xref" ||
        raw == "trailer" || raw == "startxref" || raw == "R" || raw == "f" || raw == "n") {
        t.type = TokType::Keyword;
        t.sv = raw;
        return t;
    }

    // 숫자 시도: 선택적 부호 + 자릿수 + 선택적 소수점
    bool has_dot = false;
    bool valid = true;
    size_t start = 0;

    if (!raw.empty() && (raw[0] == '+' || raw[0] == '-'))
        start = 1;

    for (size_t k = start; k < raw.size(); k++) {
        if (raw[k] == '.') {
            if (has_dot) {
                valid = false;
                break;
            }
            has_dot = true;
        } else if (!std::isdigit(static_cast<unsigned char>(raw[k]))) {
            valid = false;
            break;
        }
    }

    // 부호만 있거나 점만 있으면 숫자 아님
    if (valid && raw.size() > start && !(start == raw.size())) {
        try {
            if (has_dot) {
                t.type = TokType::Real;
                t.rv = std::stod(raw);
            } else {
                t.type = TokType::Int;
                t.iv = std::stoll(raw);
            }
        } catch (...) {
            valid = false;
        }
        if (valid)
            return t;
    }

    // 그 외는 keyword로 처리 (%%EOF 등)
    t.type = TokType::Keyword;
    t.sv = raw;
    return t;
}

} // namespace cpppdf::parser
