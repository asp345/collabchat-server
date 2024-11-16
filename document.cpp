#include "document.hpp"

// Constructor with all parameters
Document::Document(int64_t id, std::string date, std::string title,
                   std::string content)
    : id(id), date(std::move(date)), title(std::move(title)),
      content(std::move(content)) {}

// Serialization (object -> JSON)
void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,
                const Document &doc) {
  boost::json::object obj;

  if (doc.id != 0)
    obj["id"] = doc.id;
  if (!doc.date.empty())
    obj["date"] = doc.date;
  if (!doc.title.empty())
    obj["title"] = doc.title;
  if (!doc.content.empty())
    obj["content"] = doc.content;

  jv = std::move(obj);
}

// Deserialization (JSON -> object)
Document tag_invoke(boost::json::value_to_tag<Document>,
                    const boost::json::value &jv) {
  const auto &obj = jv.as_object();

  Document doc = Document();
  if (obj.contains("id"))
    doc.id = boost::json::value_to<int64_t>(obj.at("id"));
  if (obj.contains("date"))
    doc.date = boost::json::value_to<std::string>(obj.at("date"));
  if (obj.contains("title"))
    doc.title = boost::json::value_to<std::string>(obj.at("title"));
  if (obj.contains("content"))
    doc.content = boost::json::value_to<std::string>(obj.at("content"));

  return doc;
}
