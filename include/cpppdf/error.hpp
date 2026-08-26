#pragma once
#include <string>
#include <string_view>

namespace cpppdf {

enum class ErrorCode {
    Success = 0,
    FileNotFound,
    InvalidPdfHeader,
    StartXRefNotFound,
    XRefParseError,
    StreamDecodeError,
    UnsupportedFilter,
    InvalidObjectStream,
    InvalidPageTree,
    ParserSyntaxError
};

std::string_view to_string(ErrorCode ec);

template <typename T> class Result {
  public:
    Result(T val) : val_(std::move(val)), ok_(true) {}
    Result(ErrorCode ec, std::string details = "")
        : ec_(ec), details_(std::move(details)), ok_(false) {}

    bool ok() const {
        return ok_;
    }
    ErrorCode error() const {
        return ec_;
    }
    const std::string &details() const {
        return details_;
    }

    const T &value() const {
        return val_;
    }
    T &value() {
        return val_;
    }

  private:
    T val_{};
    ErrorCode ec_ = ErrorCode::Success;
    std::string details_;
    bool ok_ = false;
};

} // namespace cpppdf
