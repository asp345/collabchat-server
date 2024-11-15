#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <sqlite3.h>
#include <string>
#include <vector>

class Database {
public:
  explicit Database(const std::string &db_name = ":memory:");
  ~Database();

  void execute(const std::string &sql);
  void insert_chat(const std::string &workspace, const std::string &content);
  void insert_doc(const std::string &workspace, const std::string &date,
                  const std::string &title, const std::string &content);
  void delete_doc(const std::string &id);
  void update_doc(const std::string &id, const std::string &title,
                  const std::string &content);
  void upsert_online_users(const std::string workspace,
                           const std::string user_id);

  std::vector<std::string>
  select_chats_by_workspace(const std::string &workspace);
  std::vector<std::string>
  select_online_users_by_workspace(const std::string &workspace);

  std::string login(const std::string &id, const std::string &password);

  std::vector<std::pair<std::string, std::string>>
  select_docs_by_workspace_and_date(const std::string &workspace,
                                    const std::string &date);
  std::pair<std::string, std::string> get_doc_by_id(const std::string &id);

private:
  sqlite3 *db = nullptr;
};

#endif // DATABASE_HPP
