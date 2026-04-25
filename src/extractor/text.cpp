#include "text.hpp"
#include "../document/document.hpp"
#include "../parser/lexer.hpp"
#include "../parser/object_parser.hpp"
#include <array>
#include <cstring>
#include <map>
#include <vector>

namespace cpppdf::extractor {

// ---- UTF-8 변환 ----

static std::string to_utf8(uint32_t cp) {
    if (cp == 0)       return {};
    if (cp < 0x80)     return {static_cast<char>(cp)};
    if (cp < 0x800)    return {static_cast<char>(0xC0 | (cp >> 6)),
                               static_cast<char>(0x80 | (cp & 0x3F))};
    if (cp < 0x10000)  return {static_cast<char>(0xE0 | (cp >> 12)),
                               static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
                               static_cast<char>(0x80 | (cp & 0x3F))};
    return {static_cast<char>(0xF0 | (cp >> 18)),
            static_cast<char>(0x80 | ((cp >> 12) & 0x3F)),
            static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
            static_cast<char>(0x80 | (cp & 0x3F))};
}

// ---- Windows-1252 to Unicode ----

static const uint32_t kWin1252[256] = {
    // 0x00-0x1F
    0,0,0,0,0,0,0,0,0,9,10,0,12,13,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // 0x20-0x7F (ASCII)
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0,
    // 0x80-0x9F (Windows-1252 특수)
    0x20AC,0,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,
    0x02C6,0x2030,0x0160,0x2039,0x0152,0,0x017D,0,
    0,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
    0x02DC,0x2122,0x0161,0x203A,0x0153,0,0x017E,0x0178,
    // 0xA0-0xFF (Latin-1)
    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF,
};

// ---- 폰트 인코딩 ----

struct FontEncoding {
    std::array<uint32_t, 256>       table;  // 단일 바이트 → Unicode codepoint
    std::map<std::string, std::string> cmap; // ToUnicode CMap (raw bytes → utf-8)
    bool                            has_cmap = false;
    size_t                          min_code_bytes = 0;
    size_t                          max_code_bytes = 1;

    FontEncoding() {
        for (int i = 0; i < 256; i++) table[static_cast<size_t>(i)] = static_cast<uint32_t>(i);
    }
};

// CMap hex string → uint32
static uint32_t hex_sv_to_u32(const std::string& s) {
    uint32_t v = 0;
    for (unsigned char c : s) v = (v << 8) | c;
    return v;
}

static std::string u32_to_bytes(uint32_t value, size_t width) {
    std::string out(width, '\0');
    for (size_t i = 0; i < width; ++i) {
        size_t shift = (width - 1 - i) * 8;
        out[i] = static_cast<char>((value >> shift) & 0xFFu);
    }
    return out;
}

static std::string utf16be_to_utf8(const std::string& bytes) {
    if (bytes.empty()) return {};

    size_t pos = 0;
    if (bytes.size() >= 2 &&
        static_cast<uint8_t>(bytes[0]) == 0xFE &&
        static_cast<uint8_t>(bytes[1]) == 0xFF) {
        pos = 2;
    }

    std::string out;
    while (pos + 1 < bytes.size()) {
        uint16_t hi = (static_cast<uint8_t>(bytes[pos]) << 8) |
                      static_cast<uint8_t>(bytes[pos + 1]);
        pos += 2;

        uint32_t cp = hi;
        if (hi >= 0xD800 && hi <= 0xDBFF && pos + 1 < bytes.size()) {
            uint16_t lo = (static_cast<uint8_t>(bytes[pos]) << 8) |
                          static_cast<uint8_t>(bytes[pos + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000u + (((static_cast<uint32_t>(hi) - 0xD800u) << 10)
                                 | (static_cast<uint32_t>(lo) - 0xDC00u));
                pos += 2;
            }
        }
        out += to_utf8(cp);
    }

    if (!out.empty()) return out;

    for (unsigned char c : bytes)
        out += to_utf8(c);
    return out;
}

static void update_code_width(FontEncoding& enc, size_t width) {
    if (width == 0) return;
    enc.min_code_bytes = (enc.min_code_bytes == 0)
                       ? width
                       : std::min(enc.min_code_bytes, width);
    enc.max_code_bytes = std::max(enc.max_code_bytes, width);
}

// ToUnicode CMap 스트림 파싱
static FontEncoding parse_to_unicode(const std::vector<uint8_t>& data) {
    FontEncoding enc;
    parser::Lexer lex(data.data(), data.size());

    while (!lex.at_eof()) {
        parser::Token t = lex.next();
        if (t.type != parser::TokType::Keyword) continue;

        if (t.sv == "beginbfchar") {
            while (!lex.at_eof()) {
                parser::Token src = lex.next();
                if (src.type == parser::TokType::Keyword) break; // endbfchar
                if (src.type != parser::TokType::StrHex) continue;
                parser::Token dst = lex.next();
                if (dst.type != parser::TokType::StrHex) continue;

                enc.cmap[src.sv] = utf16be_to_utf8(dst.sv);
                update_code_width(enc, src.sv.size());

                if (src.sv.size() == 1 && dst.sv.size() == 2) {
                    uint32_t dst_cp = hex_sv_to_u32(dst.sv);
                    enc.table[static_cast<uint8_t>(src.sv[0])] = dst_cp;
                }
            }
            enc.has_cmap = true;

        } else if (t.sv == "beginbfrange") {
            while (!lex.at_eof()) {
                parser::Token lo = lex.next();
                if (lo.type == parser::TokType::Keyword) break; // endbfrange
                parser::Token hi    = lex.next();
                parser::Token start = lex.next();
                if (lo.type != parser::TokType::StrHex ||
                    hi.type != parser::TokType::StrHex) continue;

                uint32_t lo_c = hex_sv_to_u32(lo.sv);
                uint32_t hi_c = hex_sv_to_u32(hi.sv);
                size_t source_width = lo.sv.size();

                if (start.type == parser::TokType::StrHex) {
                    uint32_t base_cp = hex_sv_to_u32(start.sv);
                    for (uint32_t c = lo_c; c <= hi_c; c++) {
                        uint32_t cp = base_cp + (c - lo_c);
                        std::string key = u32_to_bytes(c, source_width);
                        enc.cmap[key] = utf16be_to_utf8(u32_to_bytes(cp, start.sv.size()));
                        update_code_width(enc, key.size());
                        if (key.size() == 1) enc.table[static_cast<uint8_t>(key[0])] = cp;
                    }
                } else if (start.type == parser::TokType::ArrBegin) {
                    uint32_t c = lo_c;
                    while (!lex.at_eof()) {
                        parser::Token at = lex.peek();
                        if (at.type == parser::TokType::ArrEnd) { lex.next(); break; }
                        parser::Token av = lex.next();
                        if (av.type == parser::TokType::StrHex && c <= hi_c) {
                            std::string key = u32_to_bytes(c, source_width);
                            enc.cmap[key] = utf16be_to_utf8(av.sv);
                            update_code_width(enc, key.size());
                            if (key.size() == 1 && av.sv.size() == 2) {
                                enc.table[static_cast<uint8_t>(key[0])] = hex_sv_to_u32(av.sv);
                            }
                            c++;
                        }
                    }
                }
            }
            enc.has_cmap = true;
        }
    }
    return enc;
}

static FontEncoding build_font_encoding(const PdfObject& font_obj,
                                         const PdfDocument& doc) {
    FontEncoding enc;
    if (!font_obj.is_dict()) return enc;
    const PdfDict& fd = font_obj.as_dict();

    // ToUnicode 우선
    auto tu_it = fd.find("ToUnicode");
    if (tu_it != fd.end()) {
        PdfObject tu = doc.resolve(tu_it->second);
        if (tu.is_stream() && tu.stream) {
            if (!tu.stream->decoded_ok) parser::decode_stream(*tu.stream);
            if (tu.stream->decoded_ok) return parse_to_unicode(tu.stream->decoded);
        }
    }

    // /Encoding
    auto enc_it = fd.find("Encoding");
    if (enc_it == fd.end()) return enc;
    PdfObject encoding = doc.resolve(enc_it->second);

    if (encoding.is_name() && encoding.s == "WinAnsiEncoding") {
        for (int i = 0; i < 256; i++)
            enc.table[static_cast<size_t>(i)] = kWin1252[i];
    }
    // MacRomanEncoding, StandardEncoding: Latin-1 근사 유지

    return enc;
}

// ---- 텍스트 상태 ----

struct TextState {
    float tm[6]  = {1,0,0,1,0,0}; // 텍스트 행렬
    float tlm[6] = {1,0,0,1,0,0}; // 텍스트 라인 행렬
    std::string font_name;
    float font_size  = 12;
    float leading    = 0;
    float char_space = 0;
    float word_space = 0;
    float rise       = 0;

    void reset() {
        const float id[6] = {1,0,0,1,0,0};
        memcpy(tm,  id, sizeof(id));
        memcpy(tlm, id, sizeof(id));
    }

    // Td / TD: tlm = translation(tx,ty) × tlm, tm = tlm
    void move_to(float tx, float ty) {
        tlm[4] = tx*tlm[0] + ty*tlm[2] + tlm[4];
        tlm[5] = tx*tlm[1] + ty*tlm[3] + tlm[5];
        memcpy(tm, tlm, sizeof(tlm));
    }

    void next_line() { move_to(0.0f, -leading); }

    float x() const { return tm[4]; }
    float y() const { return tm[5]; }
};

// ---- 바이트 문자열 디코딩 ----

static std::string decode_bytes(const std::string& raw, const FontEncoding& enc) {
    std::string out;
    out.reserve(raw.size());
    for (size_t pos = 0; pos < raw.size();) {
        if (enc.has_cmap) {
            size_t min_len = enc.min_code_bytes > 0 ? enc.min_code_bytes : 1;
            size_t max_len = std::min(enc.max_code_bytes, raw.size() - pos);
            for (size_t len = max_len; len >= min_len; --len) {
                auto it = enc.cmap.find(raw.substr(pos, len));
                if (it != enc.cmap.end()) {
                    out += it->second;
                    pos += len;
                    goto next_code;
                }
                if (len == min_len) break;
            }
        }

        {
            unsigned char c = static_cast<unsigned char>(raw[pos]);
            uint32_t cp = enc.table[c];
            if (cp) out += to_utf8(cp);
            pos++;
        }

    next_code:
        continue;
    }
    return out;
}

// ---- Content Stream 파서 ----

static std::vector<TextBlock>
parse_content(const std::vector<uint8_t>& stream,
              const std::map<std::string, FontEncoding>& fonts) {
    std::vector<TextBlock>   blocks;
    std::vector<PdfObject>   stack;
    TextState                state;
    bool                     in_text  = false;
    const FontEncoding*      cur_enc  = nullptr;

    parser::Lexer lex(stream.data(), stream.size());

    auto pop_real = [&](int n) -> float {
        int sz = static_cast<int>(stack.size());
        if (sz < n) return 0.0f;
        return static_cast<float>(stack[static_cast<size_t>(sz - n)].as_number());
    };

    auto emit = [&](const std::string& raw) {
        std::string text = cur_enc ? decode_bytes(raw, *cur_enc) : raw;
        if (text.empty()) return;
        TextBlock tb;
        tb.text      = std::move(text);
        tb.x         = state.x();
        tb.y         = state.y();
        tb.font_size = state.font_size;
        blocks.push_back(std::move(tb));
        // 너비 근사: 글자 수 * font_size * 0.5
        state.tm[4] += static_cast<float>(raw.size()) * state.font_size * 0.5f;
    };

    while (!lex.at_eof()) {
        parser::Token t = lex.peek();

        // 배열 / 딕셔너리는 parse_value로 통째로 처리
        if (t.type == parser::TokType::ArrBegin ||
            t.type == parser::TokType::DictBegin) {
            stack.push_back(parser::parse_value(lex));
            continue;
        }

        lex.next();

        switch (t.type) {
            case parser::TokType::Int:
                stack.push_back(PdfObject::from_int(t.iv));    break;
            case parser::TokType::Real:
                stack.push_back(PdfObject::from_real(t.rv));   break;
            case parser::TokType::Bool:
                stack.push_back(PdfObject::from_bool(t.bv));   break;
            case parser::TokType::Null:
                stack.push_back(PdfObject::null_obj());         break;
            case parser::TokType::StrLit:
            case parser::TokType::StrHex:
                stack.push_back(PdfObject::from_string(t.sv)); break;
            case parser::TokType::Name:
                stack.push_back(PdfObject::from_name(t.sv));   break;

            case parser::TokType::Keyword: {
                const std::string& op = t.sv;

                if      (op == "BT") { state.reset(); in_text = true; }
                else if (op == "ET") { in_text = false; }

                // 텍스트 파라미터
                else if (op == "Tf" && stack.size() >= 2) {
                    int sz = static_cast<int>(stack.size());
                    if (stack[static_cast<size_t>(sz-2)].is_name()) {
                        state.font_name = stack[static_cast<size_t>(sz-2)].s;
                        auto it = fonts.find(state.font_name);
                        cur_enc = (it != fonts.end()) ? &it->second : nullptr;
                    }
                    state.font_size = pop_real(1);
                }
                else if (op == "TL") { state.leading    = pop_real(1); }
                else if (op == "Tc") { state.char_space = pop_real(1); }
                else if (op == "Tw") { state.word_space = pop_real(1); }
                else if (op == "Ts") { state.rise       = pop_real(1); }

                // 텍스트 위치
                else if (op == "Td" && stack.size() >= 2) {
                    state.move_to(pop_real(2), pop_real(1));
                }
                else if (op == "TD" && stack.size() >= 2) {
                    state.leading = -pop_real(1);
                    state.move_to(pop_real(2), pop_real(1));
                }
                else if (op == "Tm" && stack.size() >= 6) {
                    size_t sz = stack.size();
                    for (int k = 0; k < 6; k++)
                        state.tm[k] = static_cast<float>(
                            stack[sz - 6 + static_cast<size_t>(k)].as_number());
                    memcpy(state.tlm, state.tm, sizeof(state.tm));
                }
                else if (op == "T*") { state.next_line(); }

                // 텍스트 출력
                else if (op == "Tj" && in_text && !stack.empty()
                         && stack.back().is_string()) {
                    emit(stack.back().s);
                }
                else if (op == "'" && in_text && !stack.empty()
                         && stack.back().is_string()) {
                    state.next_line();
                    emit(stack.back().s);
                }
                else if (op == "\"" && in_text && stack.size() >= 3) {
                    state.word_space = pop_real(3);
                    state.char_space = pop_real(2);
                    state.next_line();
                    if (stack.back().is_string()) emit(stack.back().s);
                }
                else if (op == "TJ" && in_text && !stack.empty()
                         && stack.back().is_array()) {
                    std::string text;
                    float x0 = state.x();
                    for (const auto& item : stack.back().arr) {
                        if (item.is_string()) {
                            text += cur_enc ? decode_bytes(item.s, *cur_enc) : item.s;
                            state.tm[4] += static_cast<float>(item.s.size())
                                           * state.font_size * 0.5f;
                        } else if (item.is_number()) {
                            float kern = static_cast<float>(item.as_number());
                            state.tm[4] -= kern * state.font_size / 1000.0f;
                            // 큰 공백은 단어 구분
                            if (kern < -100 && !text.empty() && text.back() != ' ')
                                text += ' ';
                        }
                    }
                    if (!text.empty()) {
                        TextBlock tb;
                        tb.text      = std::move(text);
                        tb.x         = x0;
                        tb.y         = state.y();
                        tb.font_size = state.font_size;
                        blocks.push_back(std::move(tb));
                    }
                }

                // 인라인 이미지 스킵: BI ... ID <binary> EI
                else if (op == "BI") {
                    while (!lex.at_eof()) {
                        parser::Token it = lex.next();
                        if (it.type == parser::TokType::Keyword && it.sv == "ID") {
                            // ID 이후 raw binary를 EI까지 바이트 탐색으로 스킵
                            const uint8_t* buf  = lex.buf();
                            size_t         bsz  = lex.bsize();
                            size_t         pos  = lex.pos();
                            while (pos + 2 < bsz) {
                                if (buf[pos] == 'E' && buf[pos+1] == 'I' &&
                                    (buf[pos-1] == '\n' || buf[pos-1] == '\r' ||
                                     buf[pos-1] == ' '))
                                { lex.seek(pos + 2); break; }
                                pos++;
                            }
                            break;
                        }
                    }
                }

                stack.clear();
                break;
            }

            default: break;
        }
    }

    return blocks;
}

// ---- 공개 함수 ----

std::vector<TextBlock> extract_text(const PdfDocument& doc, int page_index) {
    std::vector<uint8_t> content = doc.get_content(page_index);
    if (content.empty()) return {};

    // 폰트 인코딩 빌드
    std::map<std::string, FontEncoding> fonts;
    PdfDict resources = doc.get_resources(page_index);
    auto font_it = resources.find("Font");
    if (font_it != resources.end()) {
        PdfObject font_map = doc.resolve(font_it->second);
        if (font_map.is_dict()) {
            for (const auto& [name, ref] : font_map.as_dict())
                fonts[name] = build_font_encoding(doc.resolve(ref), doc);
        }
    }

    return parse_content(content, fonts);
}

} // namespace cpppdf::extractor
