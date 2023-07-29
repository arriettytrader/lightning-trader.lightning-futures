﻿#pragma once
#include <string>
#include <vector>

class string_helper
{
public:
	static std::vector<std::string> split(const std::string& str, char delim) {
		std::size_t previous = 0;
		std::size_t current = str.find(delim);
		std::vector<std::string> elems;
		while (current != std::string::npos) {
			if (current > previous) {
				elems.push_back(str.substr(previous, current - previous));
			}
			previous = current + 1;
			current = str.find(delim, previous);
		}
		if (previous != str.size()) {
			elems.push_back(str.substr(previous));
		}
		return elems;
	}

    template<typename T>
    static typename std::enable_if<false == std::is_convertible<T, std::string>::value, std::string>::type
        to_string(const T& value) {
        return std::to_string(value);
    }

    template<typename T>
    static typename std::enable_if<false == std::is_convertible<T, std::vector<std::string>>::value, std::string> ::type
        to_string(const std::vector<T>& values) {
        std::string result;

        for (auto value : values) {
            result.append(value);
        }

        return result;
    }

    template<typename T>
    static bool contains(const std::vector<T>& values, const T value) {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    template<typename... T>
    static std::string join(const std::string& separator, const std::vector<T> &... values) {
        std::vector<std::string> args;
        std::string result;

        using unused = int[];
        (void)unused {
            0, (args.push_back(to_string(values)), 0)...
        };

        for (const auto& s : args) {
            result.append(s);
            result.append(separator);
        }

        return result;
    }


    template<typename... T>
    static std::string format(const std::string& s, const T &... values) {
        std::vector<std::string> args;
        std::string result = s;

        using unused = int[];
        (void)unused {
            0, (args.push_back(to_string(values)), 0)...
        };

        char open = '{';
        char close = '}';
        bool is_open = false;
        size_t index = 0;
        size_t index_length = 0;
        size_t start = 0;

        for (size_t i = 0; i < result.length(); ++i) {
            char c = result.at(i);

            if (c == '\\') {
                i += 1;
            }
            else if (c == open) {
                is_open = true;
                start = i;
            }
            else if (c == close) {
                size_t one = result.length();
                result.erase(start, i + 1 - start);

                if (index_length > 0) {
                    result.insert(start, args.at(index));
                }

                size_t two = result.length();

                i -= one - two;
                is_open = false;
                index = 0;
                index_length = 0;
            }
            else if (is_open) {
                int n = c - '0';

                if (n < 0 || n > 9) {
                    continue;
                }

                index = (index * 10) + n;
                ++index_length;
            }
        }

        return result;
    }
};