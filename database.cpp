#include "database.hpp"
#include "base64.hpp"
#include <ctime>
#include <stdexcept>

Base64 base64;
std::time_t now() { return std::time(0); }

Database::Database(const std::string &db_name) : db(nullptr) {
  int rc = sqlite3_open(db_name.c_str(), &db);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Cannot open database: " +
                             std::string(sqlite3_errmsg(db)));
  }
}

Database::~Database() {
  if (db) {
    sqlite3_close(db);
  }
}

void Database::execute(const std::string &sql) {
  char *err_msg = nullptr;
  int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string error = "SQL error: " + std::string(err_msg);
    sqlite3_free(err_msg);
    throw std::runtime_error(error);
  }
}

void Database::insert_chat(const std::string &workspace,
                           const std::string &content) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(
      db, "INSERT INTO chat (workspace, time, content) VALUES (?, ?, ?)", -1,
      &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_text(stmt, 1, workspace.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, now());
  sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute insert statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_finalize(stmt);
}

void Database::insert_doc(const std::string &workspace, const std::string &date,
                          const std::string &title,
                          const std::string &content) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db,
                              "INSERT INTO docs (workspace, time, date, title, "
                              "content) VALUES (?, ?, ?, ?, ?)",
                              -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_text(stmt, 1, workspace.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, now());
  sqlite3_bind_text(stmt, 3, date.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, content.c_str(), -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void Database::update_doc(const std::string &id, const std::string &title,
                          const std::string &content) {
  sqlite3_stmt *stmt;
  int64_t long_id = std::stol(id);
  int rc = sqlite3_prepare_v2(
      db, "UPDATE docs SET title = ?, content = ? WHERE id = ?", -1, &stmt,
      nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, long_id);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void Database::delete_doc(const std::string &id) {
  sqlite3_stmt *stmt;
  int64_t long_id = std::stol(id);
  int rc = sqlite3_prepare_v2(db, "DELETE FROM docs WHERE id = ?", -1, &stmt,
                              nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_int64(stmt, 1, long_id);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void Database::upsert_online_users(const std::string workspace,
                                   const std::string user_id) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(
      db,
      "INSERT INTO online_users (workspace, user_id, last_ping) VALUES (?,"
      "?, ?) ON CONFLICT(workspace, user_id) DO "
      "UPDATE SET last_ping=excluded.last_ping",
      -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_text(stmt, 1, workspace.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, now());

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute insert statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_finalize(stmt);
}

std::vector<std::string>
Database::select_chats_by_workspace(const std::string &workspace) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db,
                              "SELECT content FROM chat "
                              "WHERE workspace = ? ORDER BY time ASC",
                              -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_text(stmt, 1, workspace.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<std::string> results;

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    results.push_back(std::string(
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::vector<std::string>
Database::select_online_users_by_workspace(const std::string &workspace) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db,
                              "SELECT user_id FROM online_users WHERE "
                              "workspace = ? AND last_ping >= ?",
                              -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_text(stmt, 1, workspace.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, now() - 300);

  std::vector<std::string> results;

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    results.push_back(std::string(
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))));
  }

  sqlite3_finalize(stmt);
  return results;
}

std::vector<std::pair<std::string, std::string>>
Database::select_docs_by_workspace_and_date(const std::string &workspace,
                                            const std::string &date) {
  sqlite3_stmt *stmt;
  int rc;
  if (!date.empty()) {
    rc =
        sqlite3_prepare_v2(db,
                           "SELECT id, title FROM docs "
                           "WHERE workspace = ? AND date = ? ORDER BY time ASC",
                           -1, &stmt, nullptr);
  } else {
    rc = sqlite3_prepare_v2(db,
                            "SELECT id, title FROM docs "
                            "WHERE workspace = ? ORDER BY time ASC",
                            -1, &stmt, nullptr);
  }
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_text(stmt, 1, workspace.c_str(), -1, SQLITE_TRANSIENT);
  if (!date.empty())
    sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<std::pair<std::string, std::string>> results;

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    results.push_back({std::string(reinterpret_cast<const char *>(
                           sqlite3_column_text(stmt, 0))),
                       std::string(reinterpret_cast<const char *>(
                           sqlite3_column_text(stmt, 1)))});
  }

  sqlite3_finalize(stmt);
  return results;
}

std::string Database::login(const std::string &id,
                            const std::string &password) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(
      db, "SELECT password FROM workspaces WHERE name = ?", -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    if (base64.encode(password) ==
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))) {
      sqlite3_finalize(stmt);
      return base64.encode(id);
    } else { // login fail
      sqlite3_finalize(stmt);
      return "";
    }
  } else { // sign up
    rc = sqlite3_prepare_v2(
        db, "INSERT INTO workspaces (name, password) VALUES (?, ?)", -1, &stmt,
        nullptr);
    if (rc != SQLITE_OK) {
      throw std::runtime_error("Failed to prepare statement: " +
                               std::string(sqlite3_errmsg(db)));
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, base64.encode(password).c_str(), -1,
                      SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      throw std::runtime_error("Failed to execute insert statement: " +
                               std::string(sqlite3_errmsg(db)));
    }
    sqlite3_finalize(stmt);
    return base64.encode(id);
  }
}

std::pair<std::string, std::string>
Database::get_doc_by_id(const std::string &id) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(
      db, "SELECT title, content FROM docs WHERE id = ?", -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  int64_t long_id = std::stol(id);

  sqlite3_bind_int64(stmt, 1, long_id);

  if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    std::pair<std::string, std::string> result = {
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))};
    sqlite3_finalize(stmt);
    return result;
  } else {
    sqlite3_finalize(stmt);
    return {};
  }
}
