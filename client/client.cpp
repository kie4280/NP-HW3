#include <arpa/inet.h>
#include <signal.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../socket.h"

void on_C_exit();
void startClient();
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

struct sockaddr_in serv_addr;
UDP_socket UDPsock;
TCP_socket TCPsock;
int login_token = -1;

void on_sig(int sig) { exit(EXIT_SUCCESS); }

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
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(serv_port);
  in_addr ad_input;
  if (inet_aton(argv[1], &ad_input) == 0) {
    error("Invalid address");
  }

  serv_addr.sin_addr.s_addr = ad_input.s_addr;
  TCPsock.connect(serv_addr);
  Data_package pack;
  TCPsock.recv(&pack);
  std::cout << pack.fields["message"] << std::endl;
  startClient();

  return 0;
}

void on_C_exit() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_EXIT";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  exit(EXIT_SUCCESS);
}
std::string on_C_listUser() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LIST_USER";
  TCPsock.send(&send_data);
  Data_package data_recv;
  TCPsock.recv(&data_recv);
  return data_recv.fields["message"];
}
std::string on_C_whoami() {
  Data_package out;
  out.fields["type"] = "TYPE_WHOAMI";
  out.fields["transaction_id"] = std::to_string(login_token);
  UDPsock.send(serv_addr, &out);
  Data_package in;
  UDPsock.recv(nullptr, &in);
  return in.fields["message"];
}
std::string on_C_logout() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LOGOUT";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
  login_token = stoi(recv_data.fields["transaction_id"]);
  return recv_data.fields["message"];
}
std::string on_C_login(std::string username, std::string password) {
  Data_package send_data, recv_data;
  send_data.fields["type"] = "TYPE_LOGIN";
  send_data.fields["message"] = username + " " + password;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  TCPsock.recv(&recv_data);
  login_token = stoi(recv_data.fields["transaction_id"]);
  return recv_data.fields["message"];
}
std::string on_C_register(std::string username, std::string email,
                          std::string password) {
  Data_package pack;
  pack.fields["type"] = "TYPE_REGISTER";
  pack.fields["message"] = username + " " + email + " " + password;
  int stat = UDPsock.send(serv_addr, &pack);
  if (stat == -1) {
    warn("Register send failed");
  }
  Data_package data;

  if ((stat = UDPsock.recv(nullptr, &data)) > 0) {
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
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
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
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_listBoard() {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LIST_BOARD";
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_listPost(std::string boardname) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_LIST_POST";
  send_data.fields["boardname"] = boardname;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_readPost(int serial) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_READ_POST";
  send_data.fields["postserial"] = std::to_string(serial);
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_deletePost(int serial) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_DELETE_POST";
  send_data.fields["postserial"] = std::to_string(serial);
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_updatePost(int serial, bool title, std::string replacement) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_UPDATE_POST";
  send_data.fields["postserial"] = std::to_string(serial);
  send_data.fields["istitle"] = std::to_string(title);
  send_data.fields["replacement"] = replacement;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}
std::string on_C_comment(int serial, std::string comment) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_COMMENT";
  send_data.fields["postserial"] = std::to_string(serial);
  send_data.fields["comment"] = comment;
  send_data.fields["transaction_id"] = std::to_string(login_token);
  TCPsock.send(&send_data);
  Data_package recv_data;
  TCPsock.recv(&recv_data);
  return recv_data.fields["message"];
}

void startClient() {
  std::cout << "% " << std::flush;
  std::string inputstr;
  const std::regex command_reg("^([\\w-]+)\\s*(.*)$");

  while (std::getline(std::cin, inputstr)) {
    std::smatch ma;
    bool matched = std::regex_match(inputstr, ma, command_reg);
    if (!matched) {
      warn("Invalid command");
    } else {
      if (ma[1].str() == "register") {
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
      } else if (ma[1].str() == "login") {
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

      } else if (ma[1].str() == "logout") {
        if (ma[2].str().size() == 0) {
          std::string reply = on_C_logout();
          std::cout << reply << std::endl;
        } else {
          warn("Usage: logout");
        }

      } else if (ma[1].str() == "whoami") {
        if (ma[2].str().size() == 0) {
          std::string reply = on_C_whoami();
          std::cout << reply << std::endl;
        } else {
          warn("Usage: whoami");
        }

      } else if (ma[1].str() == "list-user") {
        if (ma[2].str().size() == 0) {
          std::string reply = on_C_listUser();
          std::cout << reply << std::endl;
        } else {
          warn("Usage: list-user");
        }

      } else if (ma[1].str() == "exit") {
        if (ma[2].str().size() == 0) {
          on_C_exit();
        } else {
          warn("Usage: exit");
        }

      } else if (ma[1].str() == "create-board") {
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

      } else if (ma[1].str() == "create-post") {
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
      } else if (ma[1].str() == "list-board") {
        if (ma[2].str().size() == 0) {
          std::string reply = on_C_listBoard();
          std::cout << reply << std::flush;
        } else {
          warn("Usage: list-board");
        }
      } else if (ma[1].str() == "list-post") {
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

      } else if (ma[1].str() == "read") {
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
      } else if (ma[1].str() == "delete-post") {
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
      } else if (ma[1].str() == "update-post") {
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
      } else if (ma[1].str() == "comment") {
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
      }

      else {
        warn("Invalid command");
      }
    }

    std::cout << "% " << std::flush;
  }
}
