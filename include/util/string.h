//
// Created by Johnathon Slightham on 2025-07-05.
//

#ifndef STRING_H
#define STRING_H
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

inline std::vector<std::string> split(const std::string &str, const char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        result.push_back(token);
    }

    return result;
}

inline bool is_integer(const std::string &s) {
    if (s.empty())
        return false;

    size_t start = 0;
    if (s[0] == '-' || s[0] == '+')
        start = 1;

    return start < s.size() && std::all_of(s.begin() + start, s.end(), ::isdigit);
}

#endif // STRING_H
