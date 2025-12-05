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

  constexpr auto skip_while(auto&& predicate, ptrdiff_t offset) noexcept -> bool {
    for (first_ += offset; !error_ && first_ < last_ && predicate(*first_); ++first_);
    return !(error_ |= !(first_ <= last_));
  }

  constexpr auto skip_except(auto&& predicate, std::string_view set, ptrdiff_t offset = 0) noexcept -> bool {
    return skip_while(std::forward<decltype(predicate)>(predicate), offset) && !(error_ |= !(set.contains(*first_)));
  }

  static constexpr auto (*pred_wspace)(char) noexcept
      -> bool = [] [[gnu::always_inline]] (char c) noexcept -> bool { return std::isspace(c); };
  static constexpr auto (*pred_digits)(char) noexcept
      -> bool = [] [[gnu::always_inline]] (char c) noexcept -> bool { return c != ','; };

  static constexpr std::string_view VALID_ESCAPES{R"("\/bfnrt)"};
  static constexpr std::string_view FALSE_STR{"false"};
  static constexpr std::string_view TRUE_STR{"true"};
  static constexpr std::string_view NULL_STR{"null"};

  template <auto value> constexpr auto try_pull_value_str(std::string_view target, bool set_error) noexcept
      -> std::optional<decltype(value)> {
    if (error_ || last_ - first_ < std::ssize(target) || target != std::string_view(first_, target.size())) {
      error_ |= set_error;
      return std::nullopt;
    }
    first_ += target.length();
    return value;
  }

public:
  constexpr explicit parser(std::string_view src) noexcept : first_(src.begin()), last_(src.end()) {}

  constexpr auto halt() noexcept -> void { error_ |= true; }
  constexpr auto error() const noexcept -> bool { return error_; }

  constexpr auto pull_null() noexcept -> std::optional<std::nullptr_t> {
    return try_pull_value_str<nullptr>(NULL_STR, true);
  }

  constexpr auto pull_bollean() noexcept -> std::optional<bool> {
    return try_pull_value_str<false>(FALSE_STR, false).or_else([&] {
      return try_pull_value_str<true>(TRUE_STR, true);
    });
  }

  constexpr auto pull_string() noexcept -> std::optional<std::string_view> {
    if ((error_ |= (first_ >= last_ || *first_ != '"'))) { return std::nullopt; }

    bool escaped = false;
    for (char const* begin = first_++; first_ < last_; ++first_) {
      if (escaped) {
        escaped = false;
        if ((error_ |= !VALID_ESCAPES.contains((*first_)))) { return std::nullopt; }
      } else if (*first_ == '\\') {
        escaped = true;
      } else if (*first_ == '"') {
        return std::string_view(begin + 1, first_++);
      }
    }
    return std::nullopt;
  }

  constexpr auto pull_number() noexcept -> std::optional<std::string_view> {
    if ((error_ |= first_ >= last_)) { return std::nullopt; }

    auto const* begin = first_;
    if (*first_ == '-') { ++first_; }
    if ((error_ |= !(first_ < last_ && std::isdigit(*first_)))) { return std::nullopt; }
    for (; first_ < last_ && std::isdigit(*first_); ++first_);

    if (first_ < last_ && *first_ == '.') {
      size_t n_decimal_digits = 0;
      for (++first_; first_ < last_ && std::isdigit(*first_); ++first_, ++n_decimal_digits);
      if ((error_ |= n_decimal_digits == 0)) { return std::nullopt; }
    };

    if (first_ < last_ && (*first_ == 'e' || *first_ == 'E')) {
      size_t n_exp_digits = 0;
      if (++first_ < last_ && (*first_ == '-' || *first_ == '+')) { ++first_; }
      for (; first_ < last_ && std::isdigit(*first_); ++first_, ++n_exp_digits);
      if ((error_ |= n_exp_digits == 0)) { return std::nullopt; }
    }
    return std::string_view(begin, first_);
  }

  template <class Sink>
    requires(std::is_invocable_r_v<void, Sink>)
  constexpr auto pull_list(Sink&& sink) noexcept(std::is_nothrow_invocable_r_v<void, Sink>) -> bool {
    if (error_ || !skip_except(pred_wspace, "[")) { return !(error_ |= true); }
    for (skip_while(pred_wspace, 1); !error_ && first_ < last_ && *first_ != ']';
         skip_except(pred_wspace, ",]") && skip_while(pred_wspace, *first_ == ',')) {
      if (!skip_while(pred_wspace, *first_ == ',')) { return false; }
      sink();
    }
    if (first_ >= last_ || *first_++ != ']') { return !(error_ |= true); }
    return !error_;
  }

  template <class Sink>
    requires(std::is_invocable_r_v<void, Sink, std::string_view>)
  constexpr auto pull_object(Sink&& siunk) noexcept(std::is_nothrow_invocable_r_v<void, Sink, std::string_view>)
      -> bool {
    if (error_ || !skip_except(pred_wspace, "{")) { return !(error_ |= true); }
    for (skip_while(pred_wspace, 1); !error_ && first_ < last_ && *first_ != '}';
         skip_except(pred_wspace, ",}") && skip_while(pred_wspace, *first_ == ',')) {
      if (auto key = pull_string(); key && skip_except(pred_wspace, ":") && skip_while(pred_wspace, 1)) { siunk(*key); }
    }

    if (first_ >= last_ || *first_++ != '}') { return !(error_ |= true); }
    return !error_;
  }

  constexpr auto skip_value() -> bool {
    if (error_ || !skip_while(pred_wspace, 0) || first_ >= last_) { return !(error_ |= true); }
    switch (*first_) {
    case '{': return pull_object([&](std::string_view) { skip_value(); });
    case '[': return pull_list([&] { skip_value(); });
    case '"': return pull_string().has_value();
    case 'f': return pull_bollean().has_value();
    case 't': return pull_bollean().has_value();
    case 'n': return pull_null().has_value();
    default: pull_number().has_value();
    }
    return !error_;
  }
};

} // namespace tbrekalo::json

#endif /* TBREKALO_JSON_HPP_ */
