#include "tbrekalo/json.hpp"
#include <print>
#include <vector>

static constexpr std::string_view SRC{
R"(
{
  "foo": 42,
  "bar": null,
  "baz": [
    1,
    2.0,
    -3,
    0,
    -3.0E+2,
    3.0E+2
  ],
  "buz": [
    {
      "qux": {
        "corge": [
          "null"
        ]
      }
    }
  ],
  "42": true,
  "": false,
  "skip": {
    "true": true,
    "false": false,
    "null": null,
    "list": [true, false, null, [{"a": [], "b": {"c": null}}, 1, 2]]
  },
  "happy": true
}
)"};

auto main(int, char**) -> int {
  namespace tb = tbrekalo;

  std::print("{}\n", SRC);
  tb::json::parser parser(SRC);

  parser.pull_object([&](std::string_view key) {
    if (key == "foo") { std::print("{}: {}\n", key, *parser.pull_number()); }
    if (key == "bar") { std::print("{}: {}\n", key, *parser.pull_null()); }
    if (key == "baz") {
      std::vector<std::string_view> nums;
      parser.pull_list([&] {
        if (auto number = parser.pull_number(); number) { nums.push_back(*number); }
      });

      std::print("{}: {}\n", key, nums);
    }
    if (key == "buz") {
      parser.pull_list([&] {
        parser.pull_object([&](std::string_view key) {
          if (key == "qux") {
            parser.pull_object([&](std::string_view key) {
              std::vector<std::string_view> vec;

              if (key == "corge") {
                parser.pull_list([&] { vec.push_back(*parser.pull_string()); });
                std::print("buz:qux:corge: {}\n", vec);
              }
            });
          }
        });
      });
    }
    if (key == "42") { std::print("42: {}\n", *parser.pull_bollean()); }
    if (key == "") { std::print("\"\": {}\n", *parser.pull_bollean()); }
    if (key == "skip") { std::print("skip: {}\n", parser.skip_value()); }
    if (key == "happy") { std::print("happy: {}\n", *parser.pull_bollean()); }
  });

  return EXIT_SUCCESS;
}
