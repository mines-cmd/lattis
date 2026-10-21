#pragma once

#include <span>
#include <string_view>

// // See docs/code/theme.md.
namespace ThemeSchema {
enum class Type { Color, Int, Real, Bool, String, Enum, StringList, ColorList };

struct Setting
{
    std::string_view section;
    std::string_view key;
    Type type;
    std::string_view def;
    std::string_view doc;
    double min = 0;
    double max = 0;
    std::string_view choices;
    std::string_view fallback;
    bool onRelease = false;
    bool heading = false;
};

struct Section
{
    std::string_view name;
    std::string_view doc;
};

std::span<const Section> sections();
struct Heading
{
    std::string_view section;
    std::string_view title;
    int first;
};

std::span<const Setting> settings();
std::span<const Heading> headings();

std::string_view typeName(Type type);

constexpr bool isColorSetting(const Setting &setting)
{
    return setting.type == Type::Color || setting.type == Type::ColorList;
}
}
