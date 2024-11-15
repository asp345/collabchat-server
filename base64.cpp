#include "base64.hpp"
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/transform_width.hpp>

using namespace boost::archive::iterators;

std::string Base64::encode(const std::string &s) {
  using it_base64_t = insert_linebreaks<
      base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>,
      72>;
  unsigned int writePaddChars = (3 - s.length() % 3) % 3;
  std::string base64(it_base64_t(s.begin()), it_base64_t(s.end()));
  base64.append(writePaddChars, '=');
  return base64;
}

std::string Base64::decode(const std::string &base64) {
  using it_binary_t = transform_width<
      binary_from_base64<remove_whitespace<std::string::const_iterator>>, 8, 6>;
  unsigned int paddChars = std::count(base64.begin(), base64.end(), '=');
  std::string result(it_binary_t(base64.begin()), it_binary_t(base64.end()));
  result.erase(result.end() - paddChars, result.end());
  return result;
}
