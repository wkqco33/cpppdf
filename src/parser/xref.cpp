#include "xref.hpp"
#include "object_parser.hpp"
#include <cstring>

namespace cpppdf::parser {

size_t find_startxref(const uint8_t *data, size_t size) {
    if (size < 20)
        return 0;

    // 끝에서 최대 1024바이트 범위 내에서 역방향 탐색
    const char kw[] = "startxref";
    constexpr size_t kwlen = 9;
    size_t search_from = (size > 1024) ? size - 1024 : 0;

    for (size_t i = size - kwlen; i >= search_from; i--) {
        if (memcmp(data + i, kw, kwlen) == 0) {
            Lexer lex(data, size, i + kwlen);
            Token t = lex.next();
            if (t.type == TokType::Int && t.iv > 0)
                return static_cast<size_t>(t.iv);
        }
        if (i == search_from)
            break;
    }
    return 0;
}

// 전통적인 xref 테이블 파싱
// "xref" 키워드는 이미 소비된 상태
static void parse_xref_table(Lexer &lex, XRefTable &table) {
    while (true) {
        Token t = lex.peek();
        if (t.type != TokType::Int)
            break;

        lex.next();
        int32_t start_num = static_cast<int32_t>(t.iv);

        t = lex.next();
        if (t.type != TokType::Int)
            break;
        int32_t count = static_cast<int32_t>(t.iv);

        for (int32_t k = 0; k < count; k++) {
            Token offset_tok = lex.next();
            Token gen_tok = lex.next();
            Token type_tok = lex.next();

            if (offset_tok.type != TokType::Int || gen_tok.type != TokType::Int ||
                type_tok.type != TokType::Keyword)
                break;

            XRefEntry entry;
            entry.offset = static_cast<uint64_t>(offset_tok.iv);
            entry.gen = static_cast<int32_t>(gen_tok.iv);
            entry.in_use = (type_tok.sv == "n");
            entry.type = entry.in_use ? XRefEntryType::InUse : XRefEntryType::Free;

            // 기존에 없는 경우만 추가 (후속 업데이트 우선)
            if (table.find(start_num + k) == table.end())
                table[start_num + k] = entry;
        }
    }
}

// xref stream 파싱 (PDF 1.5+)
// 스트림 내 바이너리 데이터로 xref를 인코딩
static bool parse_xref_stream(const uint8_t *data, size_t size, size_t offset, XRefTable &table,
                              PdfDict &trailer) {
    Lexer lex(data, size, offset);

    // <num> <gen> obj
    Token t1 = lex.next();
    Token t2 = lex.next();
    Token t3 = lex.next(); // "obj"
    (void)t1;
    (void)t2;

    if (t3.type != TokType::Keyword || t3.sv != "obj")
        return false;

    Token dictBegin = lex.next();
    if (dictBegin.type != TokType::DictBegin)
        return false;

    PdfObject obj = parse_dict_or_stream(lex);
    if (!obj.is_stream() || !obj.stream)
        return false;

    // stream raw 데이터 읽기
    size_t stream_pos = lex.pos();
    // stream 키워드 뒤 줄바꿈 건너뜀
    while (stream_pos < size && (data[stream_pos] == '\r' || data[stream_pos] == '\n'))
        ++stream_pos;

    const PdfDict &sd = obj.stream->dict;
    trailer = sd;

    // /Length 읽기
    auto lit = sd.find("Length");
    if (lit == sd.end())
        return false;
    int64_t length = lit->second.is_int() ? lit->second.i : 0;
    if (length <= 0 || stream_pos + static_cast<size_t>(length) > size)
        return false;

    obj.stream->raw.assign(data + stream_pos, data + stream_pos + length);

    if (!decode_stream(*obj.stream))
        return false;
    const auto &raw = obj.stream->decoded;

    // /W: 각 필드 너비 [type, offset, gen]
    auto wit = sd.find("W");
    if (wit == sd.end() || !wit->second.is_array())
        return false;
    const auto &warr = wit->second.arr;
    if (warr.size() < 3)
        return false;

    int w0 = static_cast<int>(warr[0].is_int() ? warr[0].i : 0);
    int w1 = static_cast<int>(warr[1].is_int() ? warr[1].i : 0);
    int w2 = static_cast<int>(warr[2].is_int() ? warr[2].i : 0);
    int entry_size = w0 + w1 + w2;
    if (entry_size <= 0)
        return false;

    // /Index: [start count ...]
    std::vector<std::pair<int32_t, int32_t>> sections;
    auto ixit = sd.find("Index");
    if (ixit != sd.end() && ixit->second.is_array()) {
        const auto &idx = ixit->second.arr;
        for (size_t k = 0; k + 1 < idx.size(); k += 2) {
            if (idx[k].is_int() && idx[k + 1].is_int())
                sections.emplace_back(static_cast<int32_t>(idx[k].i),
                                      static_cast<int32_t>(idx[k + 1].i));
        }
    }

    // /Size
    auto sit = sd.find("Size");
    if (sections.empty() && sit != sd.end() && sit->second.is_int())
        sections.emplace_back(0, static_cast<int32_t>(sit->second.i));

    auto read_field = [&](const uint8_t *p, int w) -> uint64_t {
        uint64_t v = 0;
        for (int k = 0; k < w; k++)
            v = (v << 8) | p[k];
        return v;
    };

    size_t byte_pos = 0;
    for (auto [start, count] : sections) {
        for (int32_t k = 0; k < count; k++) {
            if (byte_pos + static_cast<size_t>(entry_size) > raw.size())
                break;
            const uint8_t *ep = raw.data() + byte_pos;

            uint64_t type = (w0 > 0) ? read_field(ep, w0) : 1;
            uint64_t f2 = read_field(ep + w0, w1);
            uint64_t gen = (w2 > 0) ? read_field(ep + w0 + w1, w2) : 0;

            if (table.find(start + k) == table.end()) {
                XRefEntry e;
                if (type == 1) {
                    e.gen = static_cast<int32_t>(gen);
                    e.in_use = true;
                    e.type = XRefEntryType::InUse;
                    e.offset = f2;
                } else if (type == 2) {
                    // For compressed entries the third field is the object index, not a generation.
                    e.gen = 0;
                    e.in_use = true;
                    e.type = XRefEntryType::Compressed;
                    e.obj_stream_num = static_cast<int32_t>(f2);
                    e.obj_stream_index = static_cast<int32_t>(gen);
                } else {
                    e.in_use = false;
                    e.type = XRefEntryType::Free;
                }
                table[start + k] = e;
            }
            byte_pos += static_cast<size_t>(entry_size);
        }
    }
    return true;
}

XRefResult parse_xref(const uint8_t *data, size_t size, size_t xref_offset) {
    XRefResult result;
    if (xref_offset == 0 || xref_offset >= size)
        return result;

    Lexer lex(data, size, xref_offset);
    Token t = lex.next();

    if (t.type == TokType::Keyword && t.sv == "xref") {
        // 전통적인 xref 테이블
        parse_xref_table(lex, result.table);

        // trailer
        Token trailer_tok = lex.next();
        if (trailer_tok.type == TokType::Keyword && trailer_tok.sv == "trailer") {
            Token db = lex.next();
            if (db.type == TokType::DictBegin)
                result.trailer = parse_dict(lex);
        }
        result.ok = true;
    } else if (t.type == TokType::Int) {
        // xref stream
        result.ok = parse_xref_stream(data, size, xref_offset, result.table, result.trailer);
    }

    // /Prev: 이전 xref 테이블도 파싱 (incremental update 지원)
    auto prev_it = result.trailer.find("Prev");
    if (result.ok && prev_it != result.trailer.end() && prev_it->second.is_int()) {
        size_t prev_offset = static_cast<size_t>(prev_it->second.i);
        XRefResult prev = parse_xref(data, size, prev_offset);
        if (prev.ok) {
            // 이미 있는 항목은 덮어쓰지 않음 (최신 xref 우선)
            for (auto &[num, entry] : prev.table) {
                if (result.table.find(num) == result.table.end())
                    result.table[num] = entry;
            }
        }
    }

    return result;
}

} // namespace cpppdf::parser
