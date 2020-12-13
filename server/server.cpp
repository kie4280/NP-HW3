#include <signal.h>
#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "server/database.h"
#include "socket.h"

TCP_socket TCPsock;
UDP_socket UDPsock;
sockaddr_in serv_addr;
Database db;
bool canrun = true;
std::unordered_map<int, std::string> logins;
std::random_device rd;
std::default_random_engine randEngine(rd());
std::uniform_int_distribution<int> dis(1, 10000);
std::mutex login_mutex;
typedef std::unique_lock<std::mutex> UL;

void startServer();
void dispatchUDP();
void dispatchTCP();
void handleTCP(TCP_socket);
std::vector<std::string> splitstr(std::string, std::string);

void on_C_exit(TCP_socket &, const Data_package *);
void on_C_listUser(TCP_socket &);
void on_C_whoami(sockaddr_in &, const Data_package *);
void on_C_logout(TCP_socket &, const Data_package *);
void on_C_login(TCP_socket &, const Data_package *);
void on_C_register(sockaddr_in &, const Data_package *);
void on_C_createBoard(TCP_socket &, const Data_package *);
void on_C_createPost(TCP_socket &, const Data_package *);
void on_C_listBoard(TCP_socket &, const Data_package *);
void on_C_listPost(TCP_socket &, const Data_package *);
void on_C_readPost(TCP_socket &, const Data_package *);
void on_C_deletePost(TCP_socket &, const Data_package *);
void on_C_updatePost(TCP_socket &, const Data_package *);
void on_C_comment(TCP_socket &, const Data_package *);
void on_C_joinroom(TCP_socket &, const Data_package *);
void on_C_list_chatroom(sockaddr_in &, const Data_package *);
void on_C_create_chatroom(TCP_socket &, const Data_package *);
void on_C_restart_chatroom(TCP_socket &, const Data_package *);
void on_C_close_chatroom(TCP_socket &, const Data_package *);
void on_C_attach(TCP_socket &, const Data_package *);

void on_sig(int signal) { exit(EXIT_SUCCESS); }

int main(int argc, char **argv) {
  struct sigaction ac;
  ac.sa_handler = on_sig;
  if (sigaction(SIGTERM, &ac, nullptr) < 0) {
    error(strerror(errno));
  }
  if (sigaction(SIGINT, &ac, nullptr) < 0) {
    error(strerror(errno));
  }
  if (argc < 2) {
    error("Usage: server <portnum>");
  }
  uint16_t serv_port;
  if ((serv_port = strtol(argv[1], nullptr, 10)) == 0L) {
    error("Invalid port number");
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(serv_port);

  startServer();
}

void startServer() {
  UDPsock.bind(serv_addr);
  TCPsock.bind(serv_addr);
  TCPsock.listen();
  std::thread tcp(dispatchTCP);
  std::thread udp(dispatchUDP);
  tcp.detach();
  udp.detach();
  while (canrun) {
    std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

void dispatchUDP() {
  while (canrun) {
    Data_package data_recv;
    sockaddr_in return_addr;
    socklen_t return_len = sizeof(return_addr);
    ssize_t recvlen = UDPsock.recv(&return_addr, &data_recv);
    std::string type = data_recv.fields["type"];
    if (type == "TYPE_REGISTER") {
      on_C_register(return_addr, &data_recv);
    } else if (type == "TYPE_WHOAMI") {
      on_C_whoami(return_addr, &data_recv);
    } else if (type == "TYPE_LIST_CHATROOM") {
      on_C_list_chatroom(return_addr, &data_recv);
    }
    // std::cout << data_recv.message << std::endl;
  }
}

void dispatchTCP() {
  std::vector<std::future<void>> futures;
  while (canrun) {
    TCP_socket sock;
    TCPsock.accept(sock);

    std::packaged_task<void(TCP_socket)> task(handleTCP);
    std::future<void> result = task.get_future();
    std::thread a(std::move(task), sock);
    a.detach();
    futures.push_back(std::move(result));
  }
}

void handleTCP(TCP_socket tcpsock) {
  warn("New connection");
  char text[1000] =
      "********************************\n"
      "** Welcome to the BBS server. **\n"
      "********************************";
  Data_package data;
  data.fields["type"] = "TYPE_MSG";
  data.fields["message"] = text;
  tcpsock.send(&data);
  while (canrun) {
    Data_package rec;
    int len = tcpsock.recv(&rec);
    if (len == 0) {
      break;
    }
    std::string type = rec.fields.at("type");
    if (type == "TYPE_LOGIN") {
      on_C_login(tcpsock, &rec);
    } else if (type == "TYPE_LOGOUT") {
      on_C_logout(tcpsock, &rec);
    } else if (type == "TYPE_EXIT") {
      on_C_exit(tcpsock, &rec);
    } else if (type == "TYPE_LIST_USER") {
      on_C_listUser(tcpsock);
    } else if (type == "TYPE_CREATE_BOARD") {
      on_C_createBoard(tcpsock, &rec);
    } else if (type == "TYPE_CREATE_POST") {
      on_C_createPost(tcpsock, &rec);
    } else if (type == "TYPE_LIST_BOARD") {
      on_C_listBoard(tcpsock, &rec);
    } else if (type == "TYPE_LIST_POST") {
      on_C_listPost(tcpsock, &rec);
    } else if (type == "TYPE_READ_POST") {
      on_C_readPost(tcpsock, &rec);
    } else if (type == "TYPE_DELETE_POST") {
      on_C_deletePost(tcpsock, &rec);
    } else if (type == "TYPE_UPDATE_POST") {
      on_C_updatePost(tcpsock, &rec);
    } else if (type == "TYPE_COMMENT") {
      on_C_comment(tcpsock, &rec);
    } else if (type == "TYPE_JOIN_CHATROOM") {
      on_C_joinroom(tcpsock, &rec);
    } else if (type == "TYPE_CREATE_CHATROOM") {
      on_C_create_chatroom(tcpsock, &rec);
    } else if (type == "TYPE_RESTART_CHATROOM") {
      on_C_restart_chatroom(tcpsock, &rec);
    } else if (type == "TYPE_CLOSE_CHATROOM") {
      on_C_close_chatroom(tcpsock, &rec);
    } else if (type == "TYPE_ATTACH") {
      on_C_attach(tcpsock, &rec);
    }
  }
}

std::vector<std::string> splitstr(std::string a, std::string seperator) {
  size_t pos, old_pos = 0;
  std::vector<std::string> subs;
  pos = a.find(seperator, 0);
  while (pos != std::string::npos) {
    std::string left = a.substr(old_pos, pos - old_pos);
    if (left != seperator) {
      subs.push_back(left);
    }

    old_pos = pos + seperator.size();
    pos = a.find(seperator, old_pos);
  }
  std::string left = a.substr(old_pos, pos - old_pos);
  if (left.size() > 0) {
    subs.push_back(left);
  }

  return subs;
}

void on_C_register(sockaddr_in &udpsock, const Data_package *res) {
  auto p = splitstr(res->fields.at("message"), " ");
  bool success = db.addUser(p.at(0), p.at(1), p.at(2));
  Data_package data;
  data.fields["type"] = "TYPE_REGISTER";
  if (success) {
    data.fields["message"] = "Register successfully.";
  } else {
    data.fields["message"] = "Username is already used.";
  }
  UDPsock.send(udpsock, &data);
}

void on_C_login(TCP_socket &tcpsock, const Data_package *rec) {
  auto p = splitstr(rec->fields.at("message"), " ");
  bool success;
  USER u = db.getUser(p.at(0), success);
  Data_package out;
  out.fields["type"] = "TYPE_LOGIN";

  if (stoi(rec->fields.at("transaction_id")) != -1) {
    out.fields["message"] = "Please logout first.";
    out.fields["transaction_id"] = rec->fields.at("transaction_id");
  } else if (!success || (success && u.password != p.at(1))) {
    out.fields["message"] = "Login failed.";
    out.fields["transaction_id"] = std::to_string(-1);
  } else {
    out.fields["message"] = "Welcome, " + u.username + ".";
    int ran = 0;
    if (logins.size() >= 1000) {
      throw std::runtime_error("Too many clients logged in at the same time");
    }

    UL lk(login_mutex);
    do {
      ran = dis(randEngine);
    } while (logins.find(ran) != logins.end());
    logins[ran] = u.username;
    lk.unlock();
    out.fields["transaction_id"] = std::to_string(ran);
  }
  tcpsock.send(&out);
}

void on_C_whoami(sockaddr_in &return_addr, const Data_package *data) {
  Data_package out;
  out.fields["type"] = "TYPE_WHOAMI";
  out.fields["transaction_id"] = data->fields.at("transaction_id");
  UL lk(login_mutex);
  if (logins.find(stoi(data->fields.at("transaction_id"))) != logins.end()) {
    out.fields["message"] = logins[stoi(data->fields.at("transaction_id"))];
  } else {
    out.fields["message"] = "Please login first.";
  }

  lk.unlock();
  UDPsock.send(return_addr, &out);
}

void on_C_logout(TCP_socket &tcpsock, const Data_package *rec) {
  Data_package out;
  out.fields["type"] = "TYPE_LOGOUT";
  out.fields["transaction_id"] = std::to_string(-1);
  UL lk(login_mutex);
  if (stoi(rec->fields.at("transaction_id")) != -1 &&
      logins.find(stoi(rec->fields.at("transaction_id"))) != logins.end()) {
    out.fields["message"] =
        "Bye, " + logins[stoi(rec->fields.at("transaction_id"))] + ".";
    logins.erase(stoi(rec->fields.at("transaction_id")));

  } else {
    out.fields["message"] = "Please login first.";
  }
  lk.unlock();
  tcpsock.send(&out);
}

void on_C_listUser(TCP_socket &tcpsock) {
  Data_package out;
  out.fields["type"] = "TYPE_LIST_USER";
  auto p = db.getUsers();
  std::stringstream ss;
  ss << std::left << std::setw(15) << "Name" << std::left << "Email\n";
  for (int a = 0; a < p.size(); ++a) {
    if (a > 0) {
      ss << "\n";
    }
    ss << std::left << std::setw(15) << p.at(a).username << std::left
       << p.at(a).email;
  }
  out.fields["message"] = ss.str();
  tcpsock.send(&out);
}

void on_C_exit(TCP_socket &tcpsock, const Data_package *rec) {
  logins.erase(stoi(rec->fields.at("transaction_id")));
  tcpsock.disconnect();
}

void on_C_createBoard(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  int userID = std::stoi(recv_data->fields.at("transaction_id"));
  if (userID == -1) {
    out.fields["message"] = "Please login first.\n";
  } else {
    UL lk(login_mutex);
    std::string username = logins.at(userID);
    lk.unlock();
    out.fields["message"] =
        db.createBoard(recv_data->fields.at("boardname"), username);
  }
  tcpsock.send(&out);
}
void on_C_createPost(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  int userID = std::stoi(recv_data->fields.at("transaction_id"));
  if (userID == -1) {
    out.fields["message"] = "Please login first.\n";
  } else {
    UL lk(login_mutex);
    std::string username = logins.at(userID);
    lk.unlock();
    out.fields["message"] = db.createPost(
        recv_data->fields.at("boardname"), recv_data->fields.at("title"),
        recv_data->fields.at("content"), username);
  }
  tcpsock.send(&out);
}
void on_C_listBoard(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  out.fields["message"] = db.listBoards();
  tcpsock.send(&out);
}
void on_C_listPost(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  out.fields["message"] = db.listPosts(recv_data->fields.at("boardname"));
  tcpsock.send(&out);
}
void on_C_readPost(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  int serial = std::stoi(recv_data->fields.at("postserial"));
  out.fields["message"] = db.readPost(serial);
  tcpsock.send(&out);
}
void on_C_deletePost(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  int userID = std::stoi(recv_data->fields.at("transaction_id"));
  int serial = std::stoi(recv_data->fields.at("postserial"));
  if (userID == -1) {
    out.fields["message"] = "Please login first.\n";
  } else {
    UL lk(login_mutex);
    std::string username = logins.at(userID);
    lk.unlock();
    out.fields["message"] = db.deletePost(serial, username);
  }
  tcpsock.send(&out);
}
void on_C_updatePost(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  int userID = std::stoi(recv_data->fields.at("transaction_id"));
  int serial = std::stoi(recv_data->fields.at("postserial"));
  if (userID == -1) {
    out.fields["message"] = "Please login first.\n";
  } else {
    UL lk(login_mutex);
    std::string username = logins.at(userID);
    lk.unlock();
    out.fields["message"] = db.updatePost(
        serial, username, std::stoi(recv_data->fields.at("istitle")),
        recv_data->fields.at("replacement"));
  }
  tcpsock.send(&out);
}
void on_C_comment(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  int userID = std::stoi(recv_data->fields.at("transaction_id"));
  int serial = std::stoi(recv_data->fields.at("postserial"));
  if (userID == -1) {
    out.fields["message"] = "Please login first.\n";
  } else {
    UL lk(login_mutex);
    std::string username = logins.at(userID);
    lk.unlock();
    out.fields["message"] =
        db.comment(serial, username, recv_data->fields.at("comment"));
  }
  tcpsock.send(&out);
}

void on_C_joinroom(TCP_socket &tcpsock, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = recv_data->fields.at("type");
  int userID = std::stoi(recv_data->fields.at("transaction_id"));

  if (userID == -1) {
    out.fields["result_code"] = "1";
  } else {
    UL lk(login_mutex);
    std::string username = logins.at(userID);
    std::string roomname = recv_data->fields.at("roomname");
    lk.unlock();
    Chatroom cr;
    bool exists = db.getRoom(roomname, cr);
    if (exists && cr.opened) {
      out.fields["result_code"] = "0";
      out.fields["port"] = std::to_string(cr.port);
      out.fields["username"] = username;
    } else {
      out.fields["result_code"] = "2";
    }
  }
  tcpsock.send(&out);
}

void on_C_list_chatroom(sockaddr_in &addr, const Data_package *recv_data) {
  Data_package out;
  out.fields["type"] = "TYPE_LIST_CHATROOM";
  if (std::stoi(recv_data->fields.at("transaction_id")) != -1) {
    out.fields["message"] = db.listChatroom();
  } else {
    out.fields["message"] = "Please login first.\n";
  }

  UDPsock.send(addr, &out);
}

void on_C_create_chatroom(TCP_socket &tcpsock, const Data_package *recv_data) {
  UL lm(login_mutex);
  Data_package out;
  out.fields["type"] = "TYPE_CREATE_CHATROOM";

  int login_id = std::stoi(recv_data->fields.at("transaction_id"));
  if (login_id == -1) {
    out.fields["result_code"] = "1";
  } else {
    std::string roomname = logins.at(login_id);
    int stat =
        db.createChatroom(std::stoi(recv_data->fields.at("port")), roomname);
    if (stat == 0) {
      out.fields["result_code"] = "0";
    } else if (stat == 2) {
      out.fields["result_code"] = "2";
    }
    out.fields["username"] = roomname;
  }
  tcpsock.send(&out);
}

void on_C_restart_chatroom(TCP_socket &tcpsock, const Data_package *recv_data) {
  UL lm(login_mutex);
  Data_package out;

  int login_id = std::stoi(recv_data->fields.at("transaction_id"));
  if (login_id == -1) {
    out.fields["result_code"] = "1";
  } else {
    std::string roomname = logins.at(login_id);
    Chatroom ch;
    bool exist = db.getRoom(roomname, ch);
    if (exist) {
      if (ch.opened) {
        out.fields["result_code"] = "3";
      } else {
        ch.opened = true;
        db.setRoom(roomname, ch);
        out.fields["result_code"] = "0";
        out.fields["username"] = roomname;
        out.fields["port"] = std::to_string(ch.port);
      }
    } else {
      out.fields["result_code"] = "2";
    }
  }
  tcpsock.send(&out);
}

void on_C_close_chatroom(TCP_socket &tcpsock, const Data_package *recv_data) {
  UL lm(login_mutex);
  Data_package out;

  std::string roomname = recv_data->fields.at("username");
  Chatroom ch;
  bool exist = db.getRoom(roomname, ch);
  if (exist) {
    ch.opened = false;
    db.setRoom(roomname, ch);
    out.fields["port"] = std::to_string(ch.port);
  }

  tcpsock.send(&out);
}

void on_C_attach(TCP_socket &tcpsock, const Data_package *recv_data) {
  
  Data_package out;
  int userID = std::stoi(recv_data->fields.at("transaction_id"));

  if (userID == -1) {
    out.fields["result_code"] = "1";
  } else {
    UL lk(login_mutex);
    std::string username = logins.at(userID);
    
    lk.unlock();
    Chatroom cr;
    bool exists = db.getRoom(username, cr);
    if (!exists) {
      out.fields["result_code"] = "2";
    } else if (!cr.opened) {
      out.fields["result_code"] = "3";
    } else {
      out.fields["result_code"] = "0";
      out.fields["port"] = std::to_string(cr.port);
      out.fields["username"] = username;
    }
  }
  tcpsock.send(&out);
}