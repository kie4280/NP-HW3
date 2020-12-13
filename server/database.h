#ifndef DATABASE_H
#define DATABASE_H

#define DB_FILENAME "server.db"

#include <arpa/inet.h>
#include <sqlite3.h>

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

struct USER {
  USER(){};
  USER(const USER &);
  std::string username;
  std::string email;
  std::string password;
};

struct POST {
  POST(){};
  POST(const POST &);
  std::string board;
  std::string author;
  std::string title;
  std::string date;
  std::string content;
  std::vector<std::tuple<std::string, std::string>> comments;
};

struct Chatroom {
  Chatroom(){};
  Chatroom(const Chatroom &old) {
    roomname = old.roomname;
    port = old.port;
    opened = old.opened;
  }
  std::string roomname;
  int port;
  bool opened = false;
};

class Database {
 public:
  Database();
  Database(const Database &);
  ~Database();
  void createTable();
  bool addUser(std::string username, std::string email, std::string password);
  USER getUser(std::string username, bool &success);
  std::vector<USER> getUsers();

  std::string createBoard(std::string boardname, std::string moderator);
  std::string createPost(std::string boardname, std::string title,
                         std::string content, std::string creator);
  std::string listBoards();
  std::string listPosts(std::string boardname);
  std::string readPost(int serial);
  std::string deletePost(int serial, std::string user);
  std::string updatePost(int serial, std::string user, bool title,
                         std::string replace);
  std::string comment(int serial, std::string user, std::string content);
  int createChatroom(int port, std::string roomname);
  bool getRoom(std::string roomname, Chatroom &room);
  bool setRoom(std::string roomname, Chatroom &room);
  std::string listChatroom();

 private:
  sqlite3 *db;
  std::shared_ptr<std::atomic_int> instances;
  int postSerial;
  std::unordered_map<std::string, int> boards;
  std::vector<std::tuple<std::string, std::string, std::vector<int>>>
      boardInfos;
  std::map<int, POST> posts;
  std::mutex postmux, boardmux;
  std::mutex roommux;
  std::unordered_map<std::string, Chatroom> rooms;
};

#endif