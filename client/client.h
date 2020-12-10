#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "socket.h"

// #define ROOM_OP_LEAVE "leave"
// #define ROOM_OP_CLOSE "close"
#define MODE_BBS 1
#define MODE_CHAT 2

extern struct sockaddr_in bbs_serv_addr;
extern UDP_socket bbs_UDPsock, chat_UDPsock;
extern TCP_socket bbs_TCPsock, chat_TCPsock;
extern int login_token;

void on_C_exit();
void startClient();
void startChat(sockaddr_in chat_addr, std::string username);
void startServer(int port);
std::string getTime();
std::string on_C_listUser();
std::string on_C_whoami();
std::string on_C_logout();
std::string on_C_login(std::string username, std::string password);
std::string on_C_register(std::string username, std::string email,
                          std::string password);
std::string on_C_createBoard(std::string boardname);
std::string on_C_createPost(std::string boardname, std::string title,
                            std::string content);
std::string on_C_listBoard();
std::string on_C_listPost(std::string boardname);
std::string on_C_readPost(int serial);
std::string on_C_deletePost(int serial);
std::string on_C_updatePost(int serial, bool title, std::string replacement);
std::string on_C_comment(int serial, std::string comment);
int on_C_createChatroom(std::string port, std::string &username);
std::string on_C_listChatroom();
int on_C_joinChatroom(std::string roomname, sockaddr_in &chat_addr,
                      std::string &username);

#endif