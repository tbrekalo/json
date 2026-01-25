#include "tbrekalo/json.hpp"
#include <print>
#include <vector>
#include <string_view>

static constexpr std::string_view SRC{
R"(
{
  "meta": {
    "session": "\u007B\u0061\u0064\u006D\u0069\u006E\u007D",
    "copyright": "\u00A9 2026 Corp",
    "path": "C:\\Windows\\System32\\Drivers\\etc\\hosts",
    "status_icon": "\u26A0"
  },
  "settings": {
    "resolution": [1920, 1080],
    "fullscreen": true,
    "vsync": null
  },
  "matrix": [
    [1.0, 0.0, 0.0],
    [0.0, 1.0, 0.0],
    [0.0, 0.0, 1.0]
  ],
  "physics": [
    { 
      "id": "obj_01", 
      "pos": [-1.5e-2, 3.4E+5] 
    },
    { 
      "id": "obj_02", 
      "pos": [0, 0] 
    }
  ]
}
)"};

auto main(int, char**) -> int {
  namespace tb = tbrekalo;

  std::println("Raw JSON Source:\n{}", SRC);
  std::println("----------------------------------------");

  tb::json::parser parser(SRC);

  // Helper to handle object fields
  bool success = parser.pull_object([&](std::string_view key) {
    
    if (key == "meta") {
      std::println("[meta]");
      parser.pull_object([&](std::string_view sub_key) {
        // All values in meta are strings
        if (auto val = parser.pull_string()) {
          std::println("  {}: {}", sub_key, *val);
        }
      });
    }
    else if (key == "settings") {
      std::println("[settings]");
      parser.pull_object([&](std::string_view sub_key) {
        if (sub_key == "resolution") {
          std::vector<std::string_view> res;
          parser.pull_list([&] { 
              if (auto n = parser.pull_number()) res.push_back(*n); 
          });
          std::println("  resolution: {}x{}", res[0], res[1]);
        }
        else if (sub_key == "fullscreen") {
          std::println("  fullscreen: {}", *parser.pull_boolean());
        }
        else if (sub_key == "vsync") {
          auto is_null = parser.pull_null(); 
          std::println("  vsync: {}", is_null ? "null" : "???");
        }
      });
    }
    else if (key == "matrix") {
      std::println("[matrix]");
      parser.pull_list([&] {
        std::vector<std::string_view> row;
        parser.pull_list([&] { 
            if (auto n = parser.pull_number()) row.push_back(*n); 
        });
        std::println("  {}", row);
      });
    }
    else if (key == "physics") {
      std::println("[physics]");
      parser.pull_list([&] {
        std::println("  - Entity:");
        parser.pull_object([&](std::string_view k) {
           if (k == "id") {
               std::println("      ID: {}", *parser.pull_string());
           } 
           else if (k == "pos") {
               std::vector<std::string_view> pos;
               parser.pull_list([&] { 
                   if (auto n = parser.pull_number()) pos.push_back(*n); 
               });
               std::println("      Position: ({}, {})", pos[0], pos[1]);
           }
        });
      });
    }
  });

  if (!success || parser.error()) {
    std::println(stderr, "\nERROR: Parsing failed or JSON is malformed.");
    return EXIT_FAILURE;
  }

  std::println("\nParsing completed successfully.");
  return EXIT_SUCCESS;
}
