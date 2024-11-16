#include "loginrequest.hpp"

LoginRequest::LoginRequest(std::string id, std::string password)
    : id(std::move(id)), password(std::move(password)) {}

// Serialization (object -> JSON)
void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,
                const LoginRequest &req) {
  jv = {{"id", req.id}, {"password", req.password}};
}

// Deserialization (JSON -> object)
LoginRequest tag_invoke(boost::json::value_to_tag<LoginRequest>,
                        const boost::json::value &jv) {
  const auto &obj = jv.as_object();
  return LoginRequest{boost::json::value_to<std::string>(obj.at("workspace")),
                      boost::json::value_to<std::string>(obj.at("password"))};
}
