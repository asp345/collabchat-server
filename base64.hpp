#ifndef BASE64_HPP
#define BASE64_HPP

#include <string>

class Base64 {
public:
  static std::string encode(const std::string &s);
  static std::string decode(const std::string &base64);
};

#endif // BASE64_HPP
