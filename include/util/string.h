//
// Created by Johnathon Slightham on 2025-07-05.
//

#ifndef STRING_H
#define STRING_H
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

#endif // STRING_H
