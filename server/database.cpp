#include "database.h"

#include <time.h>

#include <iomanip>
#include <iostream>
#include <sstream>

typedef std::unique_lock<std::mutex> UL;

namespace {
inline void warn(std::string a) { std::cout << a << std::endl; }

inline void error(std::string a) {
  std::cout << a << std::endl;
  exit(EXIT_FAILURE);
}
}  // namespace

USER::USER(const USER &old) {
  username = old.username;
  email = old.email;
  password = old.password;
}

Database::Database() {
  int status = sqlite3_open_v2(
      DB_FILENAME, &db,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
      nullptr);
  if (status != SQLITE_OK) {
    warn(sqlite3_errmsg(db));
  }
  instances = std::shared_ptr<std::atomic_int>(new std::atomic_int(0));
  (*instances)++;
  postSerial = 1;
  createTable();
}

Database::Database(const Database &old) {
  instances = old.instances;
  (*instances)++;
  db = old.db;
  postSerial = old.postSerial;
}

Database::~Database() {
  if (--(*instances) == 0) {
    sqlite3_close(db);
  }
}

void Database::createTable() {
  const char *sql =
      "CREATE TABLE main.'bbs_server' ("
      "username TEXT UNIQUE NOT NULL,"
      "email TEXT NOT NULL,"
      "password TEXT NOT NULL);";
  char *buf = nullptr;
  sqlite3_exec(db, sql, nullptr, nullptr, &buf);
  if (buf != nullptr &&
      std::string(buf).find("syntax error") != std::string::npos) {
    throw std::invalid_argument("SQL create table syntax error");
  }
}

bool Database::addUser(std::string username, std::string email,
                       std::string password) {
  std::string sql("INSERT INTO 'bbs_server' VALUES (");
  sql += "'" + username + "', '" + email + "', '" + password + "');";

  char *buf = nullptr;
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &buf);
  std::string err;
  if (buf != nullptr) err = buf;
  if (err.find("UNIQUE constraint") != std::string::npos) {
    return false;
  } else if (err.size() > 0) {
    warn(err.c_str());
    throw std::invalid_argument("syntax error");
  } else {
    return true;
  }
}

namespace {
struct GetUserResult {
  bool success = false;
  USER user;
};

int getUserCallback(void *a, int b, char **values, char **fieldName) {
  GetUserResult *r = (GetUserResult *)a;
  r->success = true;
  r->user.username = values[0];
  r->user.email = values[1];
  r->user.password = values[2];
  return 0;
}

}  // namespace

USER Database::getUser(std::string username, bool &success) {
  std::string sql(
      "SELECT username, email, password FROM 'bbs_server' WHERE username='");
  sql += username + "';";
  USER out;
  char *buf = nullptr;
  GetUserResult r;
  sqlite3_exec(db, sql.c_str(), ::getUserCallback, &r, &buf);
  std::string err;
  if (buf != nullptr) err = buf;
  if (err.size() > 0) {
    warn(err.c_str());
    throw std::runtime_error("Error selecting user from table");
  }
  success = r.success;
  if (success) {
    out = r.user;
  }

  return out;
}

namespace {
struct GetUsersResult {
  bool success = true;
  std::vector<USER> users;
};

int getUsersCallback(void *a, int b, char **values, char **fieldName) {
  GetUsersResult *r = (GetUsersResult *)a;
  USER u;
  u.username = values[0];
  u.email = values[1];
  u.password = values[2];
  r->users.push_back(u);
  return 0;
}
}  // namespace

std::vector<USER> Database::getUsers() {
  std::vector<USER> out;
  std::string sql("SELECT username, email, password FROM 'bbs_server';");

  char *buf = nullptr;
  GetUsersResult r;
  sqlite3_exec(db, sql.c_str(), ::getUsersCallback, &r, &buf);
  std::string err;
  if (buf != nullptr) err = buf;
  if (err.size() > 0) {
    r.success = false;
    warn(err.c_str());
    throw std::runtime_error("Error listing user from table");
  }
  if (r.success) {
    out = r.users;
  }
  return out;
}

std::string Database::createBoard(std::string boardname,
                                  std::string moderator) {
  UL boardlock(boardmux);
  if (boards.find(boardname) != boards.end()) {
    return "Board already exists.\n";
  }
  boards[boardname] = boardInfos.size();
  boardInfos.emplace_back(
      std::make_tuple(boardname, moderator, std::vector<int>{}));
  return "Create board successfully.\n";
}

std::string Database::createPost(std::string boardname, std::string title,
                                 std::string content, std::string creator) {
  UL boardlock(boardmux);
  UL postlock(postmux);
  if (boards.find(boardname) == boards.end()) {
    return "Board does not exist.\n";
  }
  int serial = postSerial++;
  std::get<2>(boardInfos.at(boards.at(boardname))).push_back(serial);
  POST p;
  p.author = creator;
  p.content = content;
  p.title = title;
  p.board = boardname;
  std::time_t now = std::time(0);
  std::tm *tstruct = std::localtime(&now);
  p.date = std::to_string(tstruct->tm_mon + 1) + "/" +
           std::to_string(tstruct->tm_mday);
  posts[serial] = std::move(p);

  return "Create post successfully.\n";
}

std::string Database::listBoards() {
  UL boardlock(boardmux);
  std::stringstream ss;
  ss << std::left << std::setw(9) << "Index" << std::left << std::setw(30)
     << "Name" << std::left << "Moderator" << std::endl;
  for (int a = 0; a < boardInfos.size(); ++a) {
    const auto &tt = boardInfos.at(a);
    ss << std::left << std::setw(9) << a + 1 << std::left << std::setw(30)
       << std::get<0>(tt) << std::left << std::get<1>(tt) << std::endl;
  }

  return ss.str();
}

std::string Database::listPosts(std::string boardname) {
  UL boardlock(boardmux);
  UL postlock(postmux);
  if (boards.find(boardname) == boards.end()) {
    return "Board does not exist.\n";
  }
  std::stringstream ss;
  ss << std::left << std::setw(5) << "S/N" << std::left << std::setw(30)
     << "Title" << std::left << std::setw(15) << "Author" << std::left << "Date"
     << std::endl;
  std::vector<int> &pp = std::get<2>(boardInfos.at(boards.at(boardname)));
  for (int i : pp) {
    POST &post = posts.at(i);
    ss << std::left << std::setw(5) << i << std::left << std::setw(30)
       << post.title << std::left << std::setw(15) << post.author << std::left
       << post.date << std::endl;
  }

  return ss.str();
}

std::string Database::readPost(int serial) {
  UL postlock(postmux);
  if (posts.find(serial) == posts.end()) {
    return "Post does not exist.\n";
  }
  POST &post = posts.at(serial);
  std::stringstream ss;
  ss << "Author: " << post.author << std::endl
     << "Title: " << post.title << std::endl
     << "Date: " << post.date << std::endl
     << "--" << std::endl
     << post.content << std::endl
     << "--" << std::endl;
  for (auto i : post.comments) {
    ss << std::get<0>(i) << ": " << std::get<1>(i) << std::endl;
  }
  return ss.str();
}

std::string Database::deletePost(int serial, std::string user) {
  UL boardlock(boardmux);
  UL postlock(postmux);
  if (posts.find(serial) == posts.end()) {
    return "Post does not exist.\n";
  }
  POST &p = posts.at(serial);
  if (p.author != user) {
    return "Not the post owner.\n";
  }

  std::vector<int> &b = std::get<2>(boardInfos.at(boards.at(p.board)));
  for (std::vector<int>::const_iterator i = b.begin(); i != b.end(); ++i) {
    if (*i == serial) {
      b.erase(i);
      break;
    }
  }
  posts.erase(serial);
  return "Delete successfully.\n";
}

std::string Database::updatePost(int serial, std::string user, bool title,
                                 std::string replace) {
  UL postlock(postmux);
  if (posts.find(serial) == posts.end()) {
    return "Post does not exist.\n";
  }
  POST &p = posts.at(serial);
  if (p.author != user) {
    return "Not the post owner.\n";
  }
  if (title) {
    p.title = replace;
  } else {
    p.content = replace;
  }

  return "Update successfully.\n";
}
std::string Database::comment(int serial, std::string user,
                              std::string content) {
  UL postlock(postmux);
  if (posts.find(serial) == posts.end()) {
    return "Post does not exist.\n";
  }
  POST &p = posts.at(serial);
  p.comments.push_back(std::make_tuple(user, content));
  
  return "Comment successfully.\n";
}

bool Database::getRoom(std::string roomname, Chatroom &room) {
  UL rl(roommux);
  if (rooms.find(roomname) != rooms.end()) {
    room = rooms.at(roomname);
    return true;
  }
  return false;
}


