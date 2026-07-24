#include "object_parser.hpp"
#include <zlib.h>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace cpppdf::parser {

// ---- forward declarations ----
static PdfArray parse_array(Lexer& lex);

// ---- parse_value ----

PdfObject parse_value(Lexer& lex) {
    Token t = lex.next();

    switch (t.type) {
        case TokType::Null:     return PdfObject::null_obj();
        case TokType::Bool:     return PdfObject::from_bool(t.bv);
        case TokType::Real:     return PdfObject::from_real(t.rv);
        case TokType::StrLit:
        case TokType::StrHex:   return PdfObject::from_string(t.sv);
        case TokType::Name:     return PdfObject::from_name(t.sv);
        case TokType::ArrBegin: return PdfObject::from_array(parse_array(lex));
        case TokType::DictBegin:return parse_dict_or_stream(lex);

        case TokType::Int: {
            // 간접 참조 체크: <int> <int> R
            Token t2 = lex.peek();
            if (t2.type == TokType::Int) {
                size_t saved = t2.pos;
                lex.next(); // t2 소비
                Token t3 = lex.peek();
                if (t3.type == TokType::Keyword && t3.sv == "R") {
                    lex.next(); // R 소비
                    return PdfObject::from_ref(
                        static_cast<int32_t>(t.iv),
                        static_cast<int32_t>(t2.iv));
                }
                // R이 아님: t2 위치로 되돌림
                lex.seek(saved);
            }
            return PdfObject::from_int(t.iv);
        }

        default:
            return PdfObject::null_obj();
    }
}

// ---- parse_array ----

static PdfArray parse_array(Lexer& lex) {
    PdfArray arr;
    while (true) {
        Token t = lex.peek();
        if (t.type == TokType::ArrEnd || t.type == TokType::Eof) {
            if (t.type == TokType::ArrEnd) lex.next();
            break;
        }
        arr.push_back(parse_value(lex));
    }
    return arr;
}

// ---- parse_dict ----

PdfDict parse_dict(Lexer& lex) {
    PdfDict dict;
    while (true) {
        Token key = lex.peek();
        if (key.type == TokType::DictEnd || key.type == TokType::Eof) {
            if (key.type == TokType::DictEnd) lex.next();
            break;
        }
        if (key.type != TokType::Name) {
            // 비정상 PDF: 키가 name이 아님 → skip
            lex.next();
            continue;
        }
        lex.next(); // key 소비
        dict[key.sv] = parse_value(lex);
    }
    return dict;
}

// ---- decode helpers ----

bool flate_decode(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst) {
    dst.clear();
    if (src.empty()) return true;

    uLongf out_size = std::max<size_t>(1024, src.size() * 4);
    dst.resize(out_size);

    int ret;
    z_stream zs{};
    zs.next_in  = const_cast<Bytef*>(src.data());
    zs.avail_in = static_cast<uInt>(src.size());

    if (inflateInit(&zs) != Z_OK) return false;

    constexpr size_t kMaxDecodeLimit = 128 * 1024 * 1024; // 128MB 안전 제한

    while (true) {
        zs.next_out  = dst.data() + zs.total_out;
        zs.avail_out = static_cast<uInt>(dst.size() - zs.total_out);

        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_END) break;
        if (ret == Z_BUF_ERROR || zs.avail_out == 0) {
            if (dst.size() >= kMaxDecodeLimit) {
                inflateEnd(&zs);
                return false;
            }
            dst.resize(dst.size() * 2);
        } else if (ret != Z_OK) {
            inflateEnd(&zs);
            return false;
        }
    }

    dst.resize(zs.total_out);
    inflateEnd(&zs);
    return true;
}

bool ascii85_decode(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst) {
    dst.clear();
    size_t i = 0;
    while (i < src.size()) {
        uint8_t c = src[i];
        if (c == '~') break; // ~> 종료 마커
        if (c <= ' ') { ++i; continue; } // 공백 무시

        if (c == 'z') {
            // 특수: 0x00000000
            dst.push_back(0); dst.push_back(0);
            dst.push_back(0); dst.push_back(0);
            ++i; continue;
        }

        // 5개 문자 → 4바이트
        uint8_t grp[5] = {0, 0, 0, 0, 0};
        int cnt = 0;
        while (cnt < 5 && i < src.size()) {
            uint8_t ch = src[i++];
            if (ch <= ' ') continue;
            if (ch == '~') { i--; break; }
            grp[cnt++] = ch - '!';
        }
        if (cnt == 0) break;

        uint32_t val = 0;
        for (int k = 0; k < 5; k++)
            val = val * 85 + grp[k];

        int out_bytes = cnt - 1;
        for (int k = 3; k >= 4 - out_bytes; k--)
            dst.push_back(static_cast<uint8_t>((val >> (k * 8)) & 0xFF));
    }
    return true;
}

bool asciihex_decode(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst) {
    dst.clear();
    std::string hex;
    for (uint8_t c : src) {
        if (c == '>') break;
        if (std::isxdigit(c)) hex += static_cast<char>(c);
    }
    if (hex.size() % 2 != 0) hex += '0';

    dst.reserve(hex.size() / 2);
    for (size_t k = 0; k < hex.size(); k += 2) {
        auto fh = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            return static_cast<uint8_t>(c - 'A' + 10);
        };
        dst.push_back(static_cast<uint8_t>((fh(hex[k]) << 4) | fh(hex[k + 1])));
    }
    return true;
}

bool apply_png_predictor(std::vector<uint8_t>& data,
                          int columns, int colors, int bpc) {
    int bytes_per_pixel = (colors * bpc + 7) / 8;
    int row_bytes       = (columns * colors * bpc + 7) / 8;
    int stride          = row_bytes + 1; // filter byte + row data

    if (stride <= 0 || data.size() < static_cast<size_t>(stride)) return false;

    size_t num_rows = data.size() / static_cast<size_t>(stride);
    std::vector<uint8_t> out(num_rows * static_cast<size_t>(row_bytes));
    std::vector<uint8_t> prev(static_cast<size_t>(row_bytes), 0);

    for (size_t row = 0; row < num_rows; row++) {
        uint8_t ftype       = data[row * static_cast<size_t>(stride)];
        const uint8_t* in_r = data.data() + row * static_cast<size_t>(stride) + 1;
        uint8_t* out_r      = out.data() + row * static_cast<size_t>(row_bytes);

        for (int col = 0; col < row_bytes; col++) {
            uint8_t a = (col >= bytes_per_pixel) ? out_r[col - bytes_per_pixel] : 0;
            uint8_t b = prev[static_cast<size_t>(col)];
            uint8_t c = (col >= bytes_per_pixel)
                        ? prev[static_cast<size_t>(col - bytes_per_pixel)] : 0;
            uint8_t x = in_r[col];

            switch (ftype) {
                case 0: out_r[col] = x;                              break; // None
                case 1: out_r[col] = static_cast<uint8_t>(x + a);   break; // Sub
                case 2: out_r[col] = static_cast<uint8_t>(x + b);   break; // Up
                case 3: out_r[col] = static_cast<uint8_t>(x + (a + b) / 2); break; // Average
                case 4: { // Paeth
                    int p  = static_cast<int>(a) + b - c;
                    int pa = std::abs(p - a);
                    int pb = std::abs(p - b);
                    int pc = std::abs(p - c);
                    uint8_t pr = (pa <= pb && pa <= pc) ? a
                               : (pb <= pc)             ? b : c;
                    out_r[col] = static_cast<uint8_t>(x + pr);
                } break;
                default: out_r[col] = x; break;
            }
        }
        prev.assign(out_r, out_r + row_bytes);
    }

    data = std::move(out);
    return true;
}

bool decode_stream(PdfStream& s) {
    if (s.decoded_ok) return true;

    // /Filter 읽기
    auto filt_it = s.dict.find("Filter");
    if (filt_it == s.dict.end()) {
        s.decoded    = s.raw;
        s.decoded_ok = true;
        return true;
    }

    const PdfObject& filt = filt_it->second;
    std::vector<std::string> filters;
    if (filt.is_name()) {
        filters.push_back(filt.s);
    } else if (filt.is_array()) {
        for (const auto& f : filt.arr)
            if (f.is_name()) filters.push_back(f.s);
    }

    // /DecodeParms 읽기 (필터별 파라미터)
    std::vector<PdfObject> dp_list;
    auto dp_it = s.dict.find("DecodeParms");
    if (dp_it != s.dict.end()) {
        const PdfObject& dp = dp_it->second;
        if (dp.is_dict()) {
            dp_list.push_back(dp);
        } else if (dp.is_array()) {
            dp_list = dp.arr;
        }
    }

    std::vector<uint8_t> data = s.raw;
    for (size_t idx = 0; idx < filters.size(); idx++) {
        const std::string& fname = filters[idx];
        const PdfObject*   dp    = (idx < dp_list.size()) ? &dp_list[idx] : nullptr;

        std::vector<uint8_t> out;
        bool ok = false;

        if (fname == "FlateDecode" || fname == "Fl") {
            ok = flate_decode(data, out);
            // PNG predictor 처리
            if (ok && dp && dp->is_dict()) {
                auto pred_it = dp->as_dict().find("Predictor");
                if (pred_it != dp->as_dict().end() && pred_it->second.is_int()) {
                    int predictor = static_cast<int>(pred_it->second.i);
                    if (predictor >= 10) {
                        int columns = 1, colors = 1, bpc = 8;
                        auto get_int = [&](const char* key, int def) -> int {
                            auto it2 = dp->as_dict().find(key);
                            return (it2 != dp->as_dict().end() && it2->second.is_int())
                                   ? static_cast<int>(it2->second.i) : def;
                        };
                        columns = get_int("Columns",            1);
                        colors  = get_int("Colors",             1);
                        bpc     = get_int("BitsPerComponent",   8);
                        apply_png_predictor(out, columns, colors, bpc);
                    }
                }
            }
        } else if (fname == "DCTDecode" || fname == "DCT") {
            // JPEG는 raw 그대로 (image.cpp에서 libjpeg로 처리)
            ok  = true;
            out = data;
        } else if (fname == "ASCII85Decode" || fname == "A85") {
            ok = ascii85_decode(data, out);
        } else if (fname == "ASCIIHexDecode" || fname == "AHx") {
            ok = asciihex_decode(data, out);
        } else {
            return false; // 지원하지 않는 필터
        }

        if (!ok) return false;
        data = std::move(out);
    }

    s.decoded    = std::move(data);
    s.decoded_ok = true;
    return true;
}

// ---- parse_dict_or_stream ----

PdfObject parse_dict_or_stream(Lexer& lex) {
    PdfDict dict = parse_dict(lex);

    // stream 키워드 체크
    Token next = lex.peek();
    if (!(next.type == TokType::Keyword && next.sv == "stream"))
        return PdfObject::from_dict(std::move(dict));

    lex.next(); // "stream" 소비

    // "stream" 키워드 뒤: LF 또는 CR LF
    // lex의 내부 버퍼 위치에서 직접 바이트 처리
    // → Lexer 인터페이스로는 raw bytes 접근이 안 되므로
    //   여기서는 Length 기반으로 raw data를 추출한다.
    // 이 함수는 Lexer 인터페이스를 통해서만 호출되므로
    // PdfStream의 raw는 parse_indirect_object에서 채운다.
    // 여기서는 dict만 포함한 stream 객체를 반환한다.
    auto stream = std::make_shared<PdfStream>();
    stream->dict = std::move(dict);
    // raw 데이터는 호출자(document)가 Lexer 위치를 이용해 채운다.
    return PdfObject::from_stream(std::move(stream));
}

} // namespace cpppdf::parser
