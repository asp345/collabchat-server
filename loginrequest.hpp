#ifndef LOGIN_REQUEST_HPP
#define LOGIN_REQUEST_HPP

#include <boost/json.hpp>
#include <boost/json/conversion.hpp>
#include <string>

class LoginRequest {
public:
  std::string id;
  std::string password;

  LoginRequest() = default;

  LoginRequest(std::string id, std::string password);

  friend void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,
                         const LoginRequest &req);
  friend LoginRequest tag_invoke(boost::json::value_to_tag<LoginRequest>,
                                 const boost::json::value &jv);
};

#endif // LOGIN_REQUEST_HPP

