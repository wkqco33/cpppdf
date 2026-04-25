#pragma once
#include "lexer.hpp"
#include "../../include/cpppdf/types.hpp"

namespace cpppdf::parser {

// buf/size/offset을 넘기면 단일 오브젝트 파싱
PdfObject parse_value(Lexer& lex);

// '<< ... >>' 를 DictBegin 토큰 이후부터 파싱
// stream 이 이어지면 PdfStream 포함 객체를 반환
PdfObject parse_dict_or_stream(Lexer& lex);

// '<< ... >>' 만 파싱 (stream 포함 안 함)
PdfDict parse_dict(Lexer& lex);

// FlateDecode 압축 해제 (zlib)
bool flate_decode(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst);

// ASCII85Decode 압축 해제
bool ascii85_decode(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst);

// ASCIIHexDecode
bool asciihex_decode(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst);

// PNG predictor 해제 (FlateDecode DecodeParms Predictor >= 10)
bool apply_png_predictor(std::vector<uint8_t>& data,
                          int columns, int colors, int bits_per_component);

// 스트림 raw 데이터를 필터에 따라 디코딩 (DecodeParms 포함)
// DCTDecode는 raw 그대로 유지 (image.cpp에서 처리)
bool decode_stream(PdfStream& s);

} // namespace cpppdf::parser
