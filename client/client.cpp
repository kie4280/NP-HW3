#include "client/client.h"

#include <arpa/inet.h>
#include <signal.h>

#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <regex>
#include <string>

#include "socket.h"

// variables
struct sockaddr_in bbs_serv_addr;
UDP_socket bbs_UDPsock;
TCP_socket bbs_TCPsock;
TCP_socket *chat_TCPsock;
int login_token = -1;
int mode = MODE_BBS;
std::string username;
std::condition_variable cv;
std::mutex sl;

// function declaration
void sendMessage(std::string inputstring);
void recvMessage();
void on_sig(int sig) { exit(EXIT_SUCCESS); }

// begin of main program

int main(int argc, char **argv) {
  struct sigaction sa;
  sa.sa_handler = on_sig;
  if (sigaction(SIGTERM, &sa, nullptr) < 0) {
    error(strerror(errno));
  }
  if (sigaction(SIGINT, &sa, nullptr) < 0) {
    error(strerror(errno));
  }
  if (argc < 3) {
    error("Usage: client <ip address> <portnum>");
  }
  int serv_port;
  if ((serv_port = strtol(argv[2], nullptr, 10)) == 0L) {
    error("Invalid port number");
  }
  bbs_serv_addr.sin_family = AF_INET;
  bbs_serv_addr.sin_port = htons(serv_port);
  in_addr ad_input;
  if (inet_aton(argv[1], &ad_input) == 0) {
    error("Invalid address");
  }

  bbs_serv_addr.sin_addr.s_addr = ad_input.s_addr;
  bbs_TCPsock.connect(bbs_serv_addr);
  Data_package pack;
  bbs_TCPsock.recv(&pack);
  std::cout << pack.fields["message"] << std::endl;
  startClient();

  return 0;
}

void startClient() {
  std::cout << "% " << std::flush;
  std::string inputstr;
  const std::regex command_reg("^([\\w-]+)\\s*(.*)$");

  while (std::getline(std::cin, inputstr)) {
    std::smatch ma;
    bool matched = std::regex_match(inputstr, ma, command_reg);

    if (mode == MODE_BBS && ma[1].str() == "register") {
      const std::regex args_reg("^([\\w\\-.]+)\\s+([\\w@.]+)\\s+(\\S+)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        std::string reply =
            on_C_register(argsm[1].str(), argsm[2].str(), argsm[3].str());
        std::cout << reply << std::endl;
      } else {
        warn("Usage: register <username> <email> <password>");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "login") {
      const std::regex args_reg("^([\\w\\-.]+)\\s+(\\S+)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        std::string reply = on_C_login(argsm[1].str(), argsm[2].str());
        std::cout << reply << std::endl;
      } else {
        warn("Usage: login <username> <password>");
      }

    } else if (mode == MODE_BBS && ma[1].str() == "logout") {
      if (ma[2].str().size() == 0) {
        std::string reply = on_C_logout();
        std::cout << reply << std::endl;
      } else {
        warn("Usage: logout");
      }

    } else if (mode == MODE_BBS && ma[1].str() == "whoami") {
      if (ma[2].str().size() == 0) {
        std::string reply = on_C_whoami();
        std::cout << reply << std::endl;
      } else {
        warn("Usage: whoami");
      }

    } else if (mode == MODE_BBS && ma[1].str() == "list-user") {
      if (ma[2].str().size() == 0) {
        std::string reply = on_C_listUser();
        std::cout << reply << std::endl;
      } else {
        warn("Usage: list-user");
      }

    } else if (mode == MODE_BBS && ma[1].str() == "exit") {
      if (ma[2].str().size() == 0) {
        on_C_exit();
      } else {
        warn("Usage: exit");
      }

    } else if (mode == MODE_BBS && ma[1].str() == "create-board") {
      const std::regex args_reg("^([^\\s]+)\\s*$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        std::string reply = on_C_createBoard(argsm[1].str());
        std::cout << reply << std::flush;
      } else {
        warn("Usage: create-board <name>");
      }

    } else if (mode == MODE_BBS && ma[1].str() == "create-post") {
      const std::regex args_reg(
          "^([^\\s]+)\\s+--title (.+) --content (.+)\\s*"
          "$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        std::string content = argsm[3].str();
        size_t pos = content.find("<br>");
        while (pos != std::string::npos) {
          content.replace(pos, strlen("<br>"), "\n");
          pos = content.find("<br>");
        }
        std::string reply =
            on_C_createPost(argsm[1].str(), argsm[2].str(), content);
        std::cout << reply << std::flush;
      } else {
        warn(
            "Usage: create-post <board-name> --title <title> --content "
            "<content>");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "list-board") {
      if (ma[2].str().size() == 0) {
        std::string reply = on_C_listBoard();
        std::cout << reply << std::flush;
      } else {
        warn("Usage: list-board");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "list-post") {
      const std::regex args_reg("^([^\\s]+)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        std::string reply = on_C_listPost(argsm[1].str());
        std::cout << reply << std::flush;
      } else {
        warn("Usage: list-post <board-name>");
      }

    } else if (mode == MODE_BBS && ma[1].str() == "read") {
      const std::regex args_reg("^([0-9]+)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        std::string reply = on_C_readPost(std::stoi(argsm[1].str()));
        std::cout << reply << std::flush;
      } else {
        warn("Usage: read <post-S/N>");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "delete-post") {
      const std::regex args_reg("^([0-9]+)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        std::string reply = on_C_deletePost(std::stoi(argsm[1].str()));
        std::cout << reply << std::flush;
      } else {
        warn("Usage: delete-post <post-S/N>");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "update-post") {
      const std::regex args_reg("^([0-9]+) --(title|content) (.*)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        bool istitle = argsm[2].str() == "title";
        std::string replacement = argsm[3].str();
        size_t pos = replacement.find("<br>");
        while (!istitle && pos != std::string::npos) {
          replacement.replace(pos, strlen("<br>"), "\n");
          pos = replacement.find("<br>");
        }
        std::string reply =
            on_C_updatePost(std::stoi(argsm[1].str()), istitle, replacement);
        std::cout << reply << std::flush;
      } else {
        warn("Usage: update-post <post-S/N> --title/content <new>");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "comment") {
      const std::regex args_reg("^([0-9]+) (.*)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        std::string reply =
            on_C_comment(std::stoi(argsm[1].str()), argsm[2].str());
        std::cout << reply << std::flush;
      } else {
        warn("Usage: comment <post-S/N> <comment>");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "create-chatroom") {
      const std::regex args_reg("^([0-9]+)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        int result_code = on_C_createChatroom(argsm[1].str(), username);
        if (result_code == 0) {
          warn("start to create chatroom...");
          int port = std::stoi(argsm[1].str());
          sockaddr_in addr;
          addr.sin_family = AF_INET;
          addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
          addr.sin_port = htons(port);
          startServer(port, username);
          UL uu(sl);
          cv.wait(uu, [] { return chat_server_ready; });
          startChat(addr);
        } else if (result_code == 1) {
          warn("Please login first.");
        } else if (result_code == 2) {
          warn("User has already created the chatroom");
        }
      } else {
        warn("Usage: create-chatroom <port>");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "list-chatroom") {
      if (ma[2].str().size() == 0) {
        std::string reply = on_C_listChatroom();
        std::cout << reply << std::flush;
      } else {
        warn("Usage: list-chatroom");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "join-chatroom") {
      const std::regex args_reg("^(.+)$");
      std::smatch argsm;
      std::string args = ma[2].str();
      bool matched = std::regex_match(args, argsm, args_reg);
      if (matched) {
        sockaddr_in chat_addr;

        int reply = on_C_joinChatroom(argsm[1].str(), chat_addr, username);
        if (reply == 1) {
          warn("Please login first.");
        } else if (reply == 2) {
          warn("The chatroom does not exist or the chatroom is close.");
        } else if (reply == 0) {
          startChat(chat_addr);
        }

      } else {
        warn("Usage: join-chatroom <chatroom_name>");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "restart-chatroom") {
      if (ma[2].str().size() == 0) {
        sockaddr_in addr;
        int status = on_C_restartChatroom(username, addr);
        if (status == 0) {
          warn("start to create chatroom...");
          startServer(ntohs(addr.sin_port), username);
          UL uu(sl);
          cv.wait(uu, [] { return chat_server_ready; });
          startChat(addr);
        } else if (status == 1) {
          warn("Please login first");
        } else if (status == 2) {
          warn("Please create-chatroom first.");
        } else if (status == 3) {
          warn("Your chatroom is still running.");
        }
      } else {
        warn("Usage: restart-chatroom");
      }
    } else if (mode == MODE_BBS && ma[1].str() == "attach") {
      if (ma[2].str().size() == 0) {
        sockaddr_in addr;
        int status = on_C_restartChatroom(username, addr);
        if (status == 0) {
          warn("start to create chatroom...");
          addr.sin_addr.s_addr = htonl(INADDR_ANY);
          startChat(addr);
        } else if (status == 1) {
          warn("Please login first");
        } else if (status == 2) {
          warn("Please create-chatroom first.");
        } else if (status == 3) {
          warn("Your chatroom is still running.");
        }
      } else {
        warn("Usage: attach");
      }
    } else if (mode == MODE_CHAT && ma[1].str() == "leave-chatroom") {
      if (ma[2].str().size() == 0) {
        Data_package send_data;
        send_data.fields["type"] = "TYPE_LEAVE_CHATROOM";
        send_data.fields["username"] = username;
        chat_TCPsock->send(&send_data);

      } else {
        warn("Usage: leave-chatroom");
      }

    } else if (mode == MODE_CHAT && ma[1].str() == "detach") {
      Data_package send_data;
      send_data.fields["type"] = "TYPE_DETACH";
      send_data.fields["username"] = username;
      chat_TCPsock->send(&send_data);

    }

    else {
      if (mode == MODE_BBS) {
        warn("Invalid command");
      } else if (mode == MODE_CHAT) {
        sendMessage(inputstr);
      }
    }
    if (mode == MODE_BBS) {
      std::cout << "% " << std::flush;
    }
  }
}

void startChat(sockaddr_in chat_addr) {
  mode = MODE_CHAT;
  chat_TCPsock = new TCP_socket();
  chat_TCPsock->connect(chat_addr);
  Data_package send_data, recv_data;
  send_data.fields["type"] = "TYPE_JOIN_CHATROOM";
  send_data.fields["username"] = username;
  chat_TCPsock->send(&send_data);
  std::thread a(recvMessage);
  a.detach();
}

void sendMessage(std::string inputstring) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_ROOM_MSG";
  send_data.fields["message"] = inputstring;
  send_data.fields["username"] = username;
  chat_TCPsock->send(&send_data);
}

void recvMessage() {
  try {
    while (true) {
      Data_package recv_data;
      chat_TCPsock->recv(&recv_data);
      std::string type = recv_data.fields.at("type");
      if (type == "TYPE_EXIT_ROOM") {
        if (recv_data.fields.at("action") == "close room") {
          Data_package send_data, room_data;
          send_data.fields["type"] = "TYPE_CLOSE_CHATROOM";
          send_data.fields["username"] = username;
          bbs_TCPsock.send(&send_data);
          bbs_TCPsock.recv(&room_data);
          TCP_socket csock;
          sockaddr_in addr;
          addr.sin_family = AF_INET;
          addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
          addr.sin_port = htons(std::stoi(room_data.fields.at("port")));
          csock.connect(addr);
          Data_package cmd;
          cmd.fields["type"] = "TYPE_CLOSE_CHATROOM";
          csock.send(&cmd);
        }
        warn("Welcome back to the BBS");
        std::cout << "% " << std::flush;
        mode = MODE_BBS;
        delete chat_TCPsock;
        chat_TCPsock = nullptr;
        throw std::runtime_error("exit");
      } else if (type == "TYPE_SERVER_MSG") {
        std::cout << recv_data.fields.at("message") << std::flush;
      } else if (type == "TYPE_ROOM_MSG") {
        std::string from = recv_data.fields.at("who");
        if (from != username) {
          std::cout << from << "[" << recv_data.fields.at("time") << "] "
                    << recv_data.fields.at("msg") << std::endl;
        }
      }
    }

  } catch (const std::exception &e) {
    if (strcmp(e.what(), "exit") == 0) {
      warn("recv msg thread exit");
    } else {
      warn(e.what());
    }
  }
}
