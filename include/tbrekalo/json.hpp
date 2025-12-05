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

  auto skip_while(auto&& predicate, ptrdiff_t offset) noexcept -> bool {
    for (first_ += offset; !error_ && first_ < last_ && predicate(*first_); ++first_);
    return !(error_ |= !(first_ <= last_));
  }

  template <class Predicate>
  auto skip_except(Predicate&& predicate, std::string_view set, ptrdiff_t offset = 0) noexcept -> bool {
    return skip_while(std::forward<Predicate>(predicate), offset) && !(error_ |= !(set.contains(*first_)));
  }

  static constexpr auto (*pred_wspace)(char) noexcept
      -> bool = [] [[gnu::always_inline]] (char c) noexcept -> bool { return std::isspace(c); };
  static constexpr auto (*pred_digits)(char) noexcept
      -> bool = [] [[gnu::always_inline]] (char c) noexcept -> bool { return c != ','; };

  static constexpr std::string_view VALID_ESCAPES{R"("\/bfnrt)"};
  static constexpr std::string_view FALSE_STR{"false"};
  static constexpr std::string_view TRUE_STR{"true"};
  static constexpr std::string_view NULL_STR{"null"};

  template <auto value> auto try_pull_value_str(std::string_view target, bool set_error) noexcept
      -> std::optional<decltype(value)> {
    if (error_ || last_ - first_ < std::ssize(target) || target != std::string_view(first_, target.size())) {
      error_ |= set_error;
      return std::nullopt;
    }
    first_ += target.length();
    return value;
  }

public:
  explicit parser(std::string_view src) : first_(src.begin()), last_(src.end()) {}

  auto halt() noexcept -> void { error_ |= true; }
  auto error() const noexcept -> bool { return error_; }

  auto pull_null() noexcept -> std::optional<std::nullptr_t> { return try_pull_value_str<nullptr>(NULL_STR, true); }

  auto pull_bollean() noexcept -> std::optional<bool> {
    return try_pull_value_str<false>(FALSE_STR, false).or_else([&] {
      return try_pull_value_str<true>(TRUE_STR, true);
    });
  }

  auto pull_string() noexcept -> std::optional<std::string_view> {
    if ((error_ |= *first_ != '"')) { return std::nullopt; }

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

  auto pull_number() noexcept -> std::optional<std::string_view> {
    auto const* begin = first_;
    if (first_ < last_ && *first_ == '-') { ++first_; }
    if ((error_ |= !(first_ < last_ && std::isdigit(*first_)))) { return std::nullopt; }
    for (; first_ < last_ && std::isdigit(*first_); ++first_);

    if (first_ < last_ && *first_ == '.') {
      auto n_digits = 0;
      for (++first_; first_ < last_ && std::isdigit(*first_); ++first_, ++n_digits);
      if ((error_ |= n_digits == 0)) { return std::nullopt; }
    };

    if (first_ < last_ && (*first_ == 'e' || *first_ == 'E')) {
      auto n_digits = 0;
      if (++first_ < last_ && (*first_ == '-' || *first_ == '+')) { ++first_; }
      for (; first_ < last_ && std::isdigit(*first_); ++first_, ++n_digits);
      if ((error_ |= n_digits == 0)) { return std::nullopt; }
    }
    return std::string_view(begin, first_);
  }

  template <class Sink>
    requires(std::is_invocable_r_v<void, Sink>)
  auto pull_list(Sink&& sink) noexcept(std::is_nothrow_invocable_r_v<void, Sink>) -> bool {
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
  auto pull_object(Sink&& siunk) noexcept(std::is_nothrow_invocable_r_v<void, Sink, std::string_view>) -> bool {
    if (error_ || !skip_except(pred_wspace, "{")) { return !(error_ |= true); }
    for (skip_while(pred_wspace, 1); !error_ && first_ < last_ && *first_ != '}';
         skip_except(pred_wspace, ",}") && skip_while(pred_wspace, *first_ == ',')) {
      if (auto key = pull_string(); key && skip_except(pred_wspace, ":") && skip_while(pred_wspace, 1)) { siunk(*key); }
    }

    if (first_ >= last_ || *first_++ != '}') { return !(error_ |= true); }
    return !error_;
  }
};

} // namespace tbrekalo::json

#endif /* TBREKALO_JSON_HPP_ */
