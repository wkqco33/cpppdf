#include "cpppdf/error.hpp"

namespace cpppdf {

std::string_view to_string(ErrorCode ec) {
    switch (ec) {
        case ErrorCode::Success:             return "Success";
        case ErrorCode::FileNotFound:        return "File Not Found";
        case ErrorCode::InvalidPdfHeader:    return "Invalid PDF Header";
        case ErrorCode::StartXRefNotFound:   return "Start XRef Not Found";
        case ErrorCode::XRefParseError:      return "XRef Parse Error";
        case ErrorCode::StreamDecodeError:   return "Stream Decode Error";
        case ErrorCode::UnsupportedFilter:   return "Unsupported Filter";
        case ErrorCode::InvalidObjectStream: return "Invalid Object Stream";
        case ErrorCode::InvalidPageTree:     return "Invalid Page Tree";
        case ErrorCode::ParserSyntaxError:   return "Parser Syntax Error";
    }
    return "Unknown Error";
}

} // namespace cpppdf
