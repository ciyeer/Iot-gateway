#pragma once

// Filesystem
#if __has_include(<filesystem>)
#include <filesystem>
#else
#include <experimental/filesystem>
namespace std {
namespace filesystem = experimental::filesystem;
}
#endif

// Optional
#if __has_include(<optional>)
#include <optional>
#else
#include <experimental/optional>
namespace std {
using experimental::optional;
using experimental::nullopt;
}
#endif

// String View
#if __has_include(<string_view>)
#include <string_view>
#else
#include <experimental/string_view>
namespace std {
using experimental::string_view;
}
#endif
