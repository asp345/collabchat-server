#include "http_connection.hpp"
#include "base64.hpp"
#include "document.hpp"
#include "loginrequest.hpp"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <boost/json/fwd.hpp>
#include <boost/json/value_from.hpp>
#include <database.hpp>
#include <iostream>

http_connection::http_connection(tcp::socket socket, Database *db)
    : db(db), socket_(std::move(socket)) {}

void http_connection::start() {
  read_request();
  check_deadline();
}

void http_connection::read_request() {
  auto self = shared_from_this();

  http::async_read(socket_, buffer_, request_,
                   [self](beast::error_code ec, std::size_t bytes_transferred) {
                     boost::ignore_unused(bytes_transferred);
                     if (!ec)
                       self->router();
                   });
}

void http_connection::router() {
  response_.version(request_.version());
  response_.keep_alive(false);
  response_.result(http::status::ok);
  // Parse request values
  auto auth_header = request_.find(http::field::authorization);
  std::string workspace;
  if (auth_header != request_.end()) {
    workspace = Base64::decode(auth_header->value());
    std::cerr << "Workspace : " << workspace << '\n';
  } else {
    std::cerr << "Authorization header not found\n";
  }
  size_t last_slash_pos = request_.target().rfind('/');
  std::string last_segment = request_.target().substr(last_slash_pos + 1);
  std::string body_str;
  boost::json::value json_value;
  try {
    body_str = boost::beast::buffers_to_string(request_.body().data());
    json_value = boost::json::parse(body_str);
  } catch (std::runtime_error &e) {
    // Ignore error
  }
  std::cerr << request_.method() << ' ' << request_.target() << '\n';
  std::cerr << "Body :" << body_str << '\n';

  try {
    if (request_.method() == http::verb::post &&
        request_.target() == "/login") { // Login
      auto login_request =
          LoginRequest(boost::json::value_to<LoginRequest>(json_value));
      auto token = db->login(login_request.id, login_request.password);
      if (!token.empty()) {
        response_.set(http::field::content_type, "text/plain");
        beast::ostream(response_.body()) << token;
      } else {
        response_.result(http::status::unauthorized);
      }
    } else if (request_.method() == http::verb::get &&
               request_.target() == "/chat") { // Get chat messages
      response_.set(http::field::content_type, "application/json");
      auto chats = db->select_chats_by_workspace(workspace);
      boost::json::object obj;
      obj["list"] = boost::json::value_from(chats);
      boost::json::value response_value = obj;
      beast::ostream(response_.body()) << response_value;
    } else if (request_.method() == http::verb::get &&
               request_.target() == "/docs") { // Get list of docs
      response_.set(http::field::content_type, "application/json");
      boost::json::array arr;
      auto docs = db->select_docs_by_workspace_and_date(
          Base64::decode(auth_header->value()), body_str);
      for (auto &doc : docs) {
        arr.emplace_back(
            boost::json::value({{"id", doc.first}, {"title", doc.second}}));
      }
      boost::json::object obj;
      obj["list"] = arr;
      boost::json::value response_value = obj;
      beast::ostream(response_.body()) << response_value;
    } else if (request_.method() == http::verb::post &&
               request_.target() == "/chat") { // Send chat message
      db->insert_chat(Base64::decode(auth_header->value()), body_str);
    } else if (request_.method() == http::verb::post &&
               request_.target().starts_with("/docs")) {
      Document doc = boost::json::value_to<Document>(json_value);
      if (last_segment == "" || last_segment == "docs") {
        db->insert_doc(workspace, doc.date, doc.title, doc.content);
      } else {
        db->update_doc(last_segment, doc.title, doc.content);
      }
    } else if (request_.method() == http::verb::post &&
               request_.target() == "/ping") {
      if (auth_header != request_.end()) {
        db->upsert_online_users(workspace, body_str);
        beast::ostream(response_.body()) << "pong";
      }
    } else if (request_.method() == http::verb::post &&
               request_.target() ==
                   "/online_users") { // Get list of online users
      response_.set(http::field::content_type, "application/json");
      auto online_users = db->select_online_users_by_workspace(workspace);
      if (std::find(online_users.begin(), online_users.end(), body_str) ==
          online_users.end()) {
        online_users.push_back(body_str);
      }
      boost::json::array online_user_array;
      for (const auto &user : online_users) {
        online_user_array.emplace_back(user);
      }
      boost::json::object obj;
      obj["list"] = online_user_array;
      boost::json::value response_value = obj;
      beast::ostream(response_.body()) << response_value;
    } else if (request_.method() == http::verb::get &&
               request_.target().starts_with("/docs")) { // Get single document
      response_.set(http::field::content_type, "application/json");
      auto doc_id = last_segment;
      auto title_content = db->get_doc_by_id(doc_id);
      boost::json::object response_body;
      response_body["title"] = title_content.first;
      response_body["content"] = title_content.second;
      auto result = boost::json::serialize(response_body);
      beast::ostream(response_.body()) << result;
    } else if (request_.method() == http::verb::delete_ &&
               request_.target().starts_with(
                   "/docs")) { // Delete single document
      auto doc_id = last_segment;
      db->delete_doc(doc_id);
    } else { // Invalid request
      response_.result(http::status::not_found);
      response_.set(http::field::content_type, "text/plain");
      beast::ostream(response_.body()) << "Not found\r\n";
    }
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    response_.result(http::status::internal_server_error);
  }
  write_response();
}

void http_connection::write_response() {
  auto self = shared_from_this();
  response_.content_length(response_.body().size());
  http::async_write(socket_, response_,
                    [self](beast::error_code ec, std::size_t) {
                      self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                      self->deadline_.cancel();
                    });
}

void http_connection::check_deadline() {
  auto self = shared_from_this();
  deadline_.async_wait([self](beast::error_code ec) {
    if (!ec) {
      // Close socket
      self->socket_.close(ec);
    }
  });
}
