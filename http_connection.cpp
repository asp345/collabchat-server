#include "http_connection.hpp"
#include "base64.hpp"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <database.hpp>
#include <iostream>

http_connection::http_connection(tcp::socket socket, Database *db)
    : base64(Base64()), db(db), socket_(std::move(socket)) {}

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
  std::cerr << "Processing request : " << request_.target() << '\n';
  auto auth_header = request_.find(http::field::authorization);
  if (auth_header != request_.end()) {
    std::cerr << "Authorization header: " << auth_header->value() << '\n';
    std::cerr << "Workspace : " << Base64::decode(auth_header->value()) << '\n';
  } else {
    std::cerr << "Authorization header not found\n";
  }
  size_t last_slash_pos = request_.target().rfind('/');
  std::string last_segment = request_.target().substr(last_slash_pos + 1);
  std::string body_str = "";
  boost::json::value request_json_obj = boost::json::value();
  try {
    body_str = boost::beast::buffers_to_string(request_.body().data());
    std::cerr << "Body: " << body_str << '\n';
    request_json_obj = boost::json::parse(body_str);
  } catch (std::runtime_error &e) {
    // ignore error
  }

  try {
    if (request_.method() == http::verb::post &&
        request_.target() == "/login") {
      auto token =
          db->login(request_json_obj.at("workspace").as_string().c_str(),
                    request_json_obj.at("password").as_string().c_str());
      if (token != "") {
        response_.set(http::field::content_type, "text/plain");
        beast::ostream(response_.body()) << token;
      } else {
        response_.result(http::status::unauthorized);
      }
    } else if (request_.method() == http::verb::get &&
               request_.target() == "/chat") {
      response_.set(http::field::content_type, "application/json");
      auto chats =
          db->select_chats_by_workspace(Base64::decode(auth_header->value()));
      boost::json::array arr;
      boost::json::object obj;
      for (auto &i : chats) {
        arr.emplace_back(i);
      }
      obj["list"] = arr;
      auto result = boost::json::serialize(obj);
      beast::ostream(response_.body()) << result;
    } else if (request_.method() == http::verb::get &&
               request_.target() == "/docs") {
      response_.set(http::field::content_type, "application/json");
      boost::json::array arr;
      boost::json::object obj;
      auto docs = db->select_docs_by_workspace_and_date(
          Base64::decode(auth_header->value()), body_str);
      for (auto &i : docs) {
        boost::json::object item_obj;
        item_obj["id"] = i.first;
        item_obj["title"] = i.second;
        arr.emplace_back(item_obj);
      }
      obj["list"] = arr;
      auto result = boost::json::serialize(obj);
      beast::ostream(response_.body()) << result;
    } else if (request_.method() == http::verb::post &&
               request_.target() == "/chat") {
      db->insert_chat(Base64::decode(auth_header->value()), body_str);
    } else if (request_.method() == http::verb::post &&
               request_.target().starts_with("/docs")) {
      if (last_segment == "" || last_segment == "docs") {
        db->insert_doc(Base64::decode(auth_header->value()),
                       request_json_obj.at("date").as_string().c_str(),
                       request_json_obj.at("title").as_string().c_str(),
                       request_json_obj.at("content").as_string().c_str());
      } else {
        db->update_doc(last_segment,
                       request_json_obj.at("title").as_string().c_str(),
                       request_json_obj.at("content").as_string().c_str());
      }
    } else if (request_.method() == http::verb::post &&
               request_.target() == "/ping") {
      if (auth_header != request_.end()) {
        std::string workspace = Base64::decode(auth_header->value());
        db->upsert_online_users(workspace, body_str);
        beast::ostream(response_.body()) << "pong";
      }
    } else if (request_.method() == http::verb::post &&
               request_.target() == "/online_users") {
      response_.set(http::field::content_type, "application/json");

      std::string workspace = Base64::decode(auth_header->value());
      auto online_users = db->select_online_users_by_workspace(workspace);
      if (std::find(online_users.begin(), online_users.end(), body_str) ==
          online_users.end()) {
        online_users.push_back(body_str);
      }

      boost::json::array online_user_array;
      for (const auto &user : online_users) {
        online_user_array.emplace_back(user);
      }

      boost::json::object response_body;
      response_body["list"] = online_user_array;

      auto result = boost::json::serialize(response_body);
      beast::ostream(response_.body()) << result;
    } else if (request_.method() == http::verb::get &&
               request_.target().starts_with("/docs")) {
      response_.set(http::field::content_type, "application/json");
      auto doc_id = last_segment;
      auto res = db->get_doc_by_id(doc_id);
      boost::json::object response_body;
      response_body["title"] = res.first;
      response_body["content"] = res.second;
      auto result = boost::json::serialize(response_body);
      beast::ostream(response_.body()) << result;
    } else if (request_.method() == http::verb::delete_ &&
               request_.target().starts_with("/docs")) {
      auto doc_id = last_segment;
      db->delete_doc(doc_id);
    } else {
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
      // Close socket to cancel any outstanding operation.
      self->socket_.close(ec);
    }
  });
}
