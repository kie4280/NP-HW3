#include <arpa/inet.h>
#include <signal.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "client/client.h"
#include "socket.h"

void on_C_exit() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_EXIT";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  exit(EXIT_SUCCESS);
}
std::string on_C_listUser() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LIST_USER";
  bbs_TCPsock.send(&send_data);
  Data_package data_recv;
  bbs_TCPsock.recv(&data_recv);
  return data_recv.fields["message"];
}
std::string on_C_whoami() {
  Data_package out;
  out.fields["type"] = "TYPE_WHOAMI";
  out.fields["transaction_id"] = std::to_string(login_token);
  bbs_UDPsock.send(bbs_serv_addr, &out);
  Data_package in;
  bbs_UDPsock.recv(nullptr, &in);
  return in.fields["message"];
}
std::string on_C_logout() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LOGOUT";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  login_token = stoi(recv_data.fields["transaction_id"]);
  return recv_data.fields["message"];
}
std::string on_C_login(std::string username, std::string password) {
  Data_package send_data, recv_data;
  send_data.fields["type"] = "TYPE_LOGIN";
  send_data.fields["message"] = username + " " + password;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  bbs_TCPsock.recv(&recv_data);
  login_token = stoi(recv_data.fields["transaction_id"]);
  return recv_data.fields["message"];
}
std::string on_C_register(std::string username, std::string email,
                          std::string password) {
  Data_package pack;
  pack.fields["type"] = "TYPE_REGISTER";
  pack.fields["message"] = username + " " + email + " " + password;
  int stat = bbs_UDPsock.send(bbs_serv_addr, &pack);
  if (stat == -1) {
    warn("Register send failed");
  }
  Data_package data;

  if ((stat = bbs_UDPsock.recv(nullptr, &data)) > 0) {
  } else if (stat == 0) {
    error("Connection closed");
  } else {
    error(strerror(errno));
  }
  return data.fields["message"];
}

std::string on_C_createBoard(std::string boardname) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_CREATE_BOARD";
  send_data.fields["boardname"] = boardname;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_createPost(std::string boardname, std::string title,
                            std::string content) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_CREATE_POST";
  send_data.fields["boardname"] = boardname;
  send_data.fields["title"] = title;
  send_data.fields["content"] = content;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_listBoard() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LIST_BOARD";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_listPost(std::string boardname) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LIST_POST";
  send_data.fields["boardname"] = boardname;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_readPost(int serial) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_READ_POST";
  send_data.fields["postserial"] = std::to_string(serial);
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_deletePost(int serial) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_DELETE_POST";
  send_data.fields["postserial"] = std::to_string(serial);
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_updatePost(int serial, bool title, std::string replacement) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_UPDATE_POST";
  send_data.fields["postserial"] = std::to_string(serial);
  send_data.fields["istitle"] = std::to_string(title);
  send_data.fields["replacement"] = replacement;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_comment(int serial, std::string comment) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_COMMENT";
  send_data.fields["postserial"] = std::to_string(serial);
  send_data.fields["comment"] = comment;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}

int on_C_createChatroom(std::string port, std::string &username) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_CREATE_CHATROOM";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  send_data.fields["port"] = port;
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  int result_code = std::stoi(recv_data.fields.at("result_code"));
  if (result_code == 0) {
    username = recv_data.fields.at("username");
  }
  return result_code;
}

std::string on_C_listChatroom() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LIST_CHATROOM";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_UDPsock.send(bbs_serv_addr, &send_data);
  Data_package recv_data;
  bbs_UDPsock.recv(nullptr, &recv_data);
  return recv_data.fields["message"];
}

int on_C_joinChatroom(std::string roomname, sockaddr_in &chat_addr,
                      std::string &username) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_JOIN_CHATROOM";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  send_data.fields["roomname"] = roomname;
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  int status = std::stoi(recv_data.fields["result_code"]);
  if (status == 0) {
    username = recv_data.fields.at("username");
    chat_addr.sin_family = AF_INET;
    chat_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    chat_addr.sin_port = htons(std::stoi(recv_data.fields.at("port")));
  }

  return status;
}

int on_C_restartChatroom(std::string &username, sockaddr_in &chat_addr) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_RESTART_CHATROOM";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  bbs_TCPsock.send(&send_data);
  Data_package recv_data;
  bbs_TCPsock.recv(&recv_data);
  int status = std::stoi(recv_data.fields["result_code"]);
  if (status == 0) {
    username = recv_data.fields.at("username");
    chat_addr.sin_family = AF_INET;
    chat_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    chat_addr.sin_port = htons(std::stoi(recv_data.fields.at("port")));
  }

  return status;
}
