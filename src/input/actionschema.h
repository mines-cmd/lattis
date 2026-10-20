#pragma once

#include <span>
#include <string_view>

// See docs/code/input.md.
namespace ActionSchema {
enum class Scope { Main, Settings, App };

struct Group
{
    std::string_view name;
    std::string_view title;
    Scope scope;
};

struct Action
{
    std::string_view group;
    std::string_view name;
    std::string_view keys;
    std::string_view title;
};

std::span<const Group> groups();
std::span<const Action> actions();

const Group *groupOf(const Action &action);
}
