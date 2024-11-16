#ifndef DOCUMENT_HPP
#define DOCUMENT_HPP

#include <boost/json.hpp>
#include <string>

class Document {
public:
  Document(int64_t id = -1, std::string date = "", std::string title = "",
           std::string content = "");

  int64_t id;
  std::string date, title, content;

  friend void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,
                         const Document &doc);
  friend Document tag_invoke(boost::json::value_to_tag<Document>,
                             const boost::json::value &jv);
};

#endif // DOCUMENT_HPP
