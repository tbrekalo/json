#ifndef TBREKALO_JSON_HPP_
#define TBREKALO_JSON_HPP_

#include <cwctype>
#include <optional>
#include <string_view>
#include <type_traits>

namespace tbrekalo::json {

class parser {
  bool error_{false};
  char const* first_;
  char const* last_;

  static constexpr auto is_space = [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; };
  static constexpr auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
  static constexpr auto is_hex = [](char c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); };

  constexpr auto skip_while(auto&& predicate, ptrdiff_t offset) noexcept -> bool {
    if (last_ - first_ < offset) { return !(error_ |= true); }
    for (first_ += offset; first_ < last_ && predicate(*first_); ++first_);
    return !error_;
  }

  constexpr auto skip_except(auto&& predicate, std::string_view set, ptrdiff_t offset = 0) noexcept -> bool {
    skip_while(predicate, offset);
    return !(error_ |= (first_ >= last_ || !set.contains(*first_)));
  }

  static constexpr std::string_view VALID_ESCAPES{R"("\|/bfnrtu)"};
  static constexpr std::string_view FALSE_STR{"false"};
  static constexpr std::string_view TRUE_STR{"true"};
  static constexpr std::string_view NULL_STR{"null"};

  template <auto value> constexpr auto try_pull_const(std::string_view target, bool set_err) noexcept
      -> std::optional<decltype(value)> {
    if (std::size_t(last_ - first_) < target.size() || std::string_view(first_, target.size()) != target) {
      error_ |= set_err;
      return std::nullopt;
    }
    first_ += target.size();
    return value;
  }

public:
  constexpr explicit parser(std::string_view src) noexcept : first_(src.begin()), last_(src.end()) {}

  constexpr auto halt() noexcept -> void { error_ |= true; }
  constexpr auto error() const noexcept -> bool { return error_; }

  constexpr auto pull_null() noexcept -> std::optional<std::nullptr_t> {
    return try_pull_const<nullptr>(NULL_STR, true);
  }

  constexpr auto pull_boolean() noexcept -> std::optional<bool> {
    return try_pull_const<false>(FALSE_STR, false).or_else([&] { return try_pull_const<true>(TRUE_STR, true); });
  }

  constexpr auto pull_string() noexcept -> std::optional<std::string_view> {
    if ((error_ |= (first_ >= last_ || *first_ != '"'))) return std::nullopt;

    auto const* begin = ++first_;
    for (; first_ < last_ && *first_ != '"'; ++first_) {
      if (static_cast<unsigned char>(*first_) < 0x20) {
        error_ = true;
        return std::nullopt;
      }
      if (*first_ == '\\') {
        if (++first_ >= last_ || !VALID_ESCAPES.contains(*first_)) {
          error_ = true;
          return std::nullopt;
        }
        if (*first_ == 'u') {
          for (int i = 0; i < 4; ++i)
            if (++first_ >= last_ || !is_hex(*first_)) {
              error_ = true;
              return std::nullopt;
            }
        }
      }
    }
    if ((error_ |= (first_ >= last_))) return std::nullopt;
    return std::string_view(begin, first_++);
  }

  constexpr auto pull_number() noexcept -> std::optional<std::string_view> {
    if ((error_ |= first_ >= last_)) return std::nullopt;
    auto const* begin = first_;

    if (*first_ == '-') ++first_;
    if ((error_ |= (first_ >= last_ || !is_digit(*first_)))) return std::nullopt;

    if (*first_ == '0') {
      if (++first_ < last_ && is_digit(*first_)) {
        error_ = true;
        return std::nullopt;
      }
    } else {
      while (first_ < last_ && is_digit(*first_)) ++first_;
    }

    if (first_ < last_ && *first_ == '.') {
      if (++first_ >= last_ || !is_digit(*first_)) {
        error_ = true;
        return std::nullopt;
      }
      while (first_ < last_ && is_digit(*first_)) ++first_;
    }

    if (first_ < last_ && (*first_ == 'e' || *first_ == 'E')) {
      if (++first_ < last_ && (*first_ == '-' || *first_ == '+')) ++first_;
      if (first_ >= last_ || !is_digit(*first_)) {
        error_ = true;
        return std::nullopt;
      }
      while (first_ < last_ && is_digit(*first_)) ++first_;
    }
    return std::string_view(begin, first_);
  }

  template <class Sink>
    requires(std::is_invocable_r_v<void, Sink>)
  constexpr auto pull_list(Sink&& sink) noexcept(std::is_nothrow_invocable_r_v<void, Sink>) -> bool {
    if (!skip_except(is_space, "[", 0)) return false;
    if (skip_while(is_space, 1) && first_ < last_ && *first_ == ']') {
      ++first_;
      return true;
    }

    do {
      sink();
      skip_while(is_space, 0);
      if (first_ < last_ && *first_ == ']') {
        ++first_;
        return true;
      }
    } while (first_ < last_ && *first_ == ',' && (skip_while(is_space, 1), true));

    return !(error_ |= true);
  }

  template <class Sink>
    requires(std::is_invocable_r_v<void, Sink, std::string_view>)
  constexpr auto pull_object(Sink&& sink) noexcept(std::is_nothrow_invocable_r_v<void, Sink, std::string_view>)
      -> bool {
    if (!skip_except(is_space, "{", 0)) return false;
    if (skip_while(is_space, 1) && first_ < last_ && *first_ == '}') {
      ++first_;
      return true;
    }

    do {
      auto key = pull_string();
      if (!key || !skip_except(is_space, ":") || !skip_while(is_space, 1)) return !(error_ |= true);
      sink(*key);
      skip_while(is_space, 0);
      if (first_ < last_ && *first_ == '}') {
        ++first_;
        return true;
      }
    } while (first_ < last_ && *first_ == ',' && (skip_while(is_space, 1), true));

    return !(error_ |= true);
  }

  constexpr auto skip_value() -> bool {
    if (!skip_while(is_space, 0) || first_ >= last_) return !(error_ |= true);
    switch (*first_) {
    case '{': return pull_object([&](std::string_view) { skip_value(); });
    case '[': return pull_list([&] { skip_value(); });
    case '"': return pull_string().has_value();
    case 'f': return pull_boolean().has_value();
    case 't': return pull_boolean().has_value();
    case 'n': return pull_null().has_value();
    default: return pull_number().has_value();
    }
  }
};

} // namespace tbrekalo::json

#endif /* TBREKALO_JSON_HPP_ */
