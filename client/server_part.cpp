#include <poll.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <list>
#include <mutex>
#include <thread>

#include "client/client.h"
#include "socket.h"

#define STAT_NONEXIST 0
#define STAT_RUNNING 1
#define STAT_CLOSED 2

struct ChatRecord {
  std::string who;
  std::string time;
  std::string msg;
};

std::string roomname;
std::list<ChatRecord> last_three;
std::vector<TCP_socket> connected;
std::mutex conn_mux;
std::mutex msg_mux;
int room_status;
uint32_t current_msg_id(1);
bool chat_server_ready = false;

void server_listener(int serv_port);
void server_recv_command(TCP_socket tcpsock, std::string username);
void broadcastMSG(ChatRecord &msg, std::vector<TCP_socket> &who);

TCP_socket *chat_listen_so;

std::string getTime() {
  std::time_t now = std::time(0);
  std::tm *tstruct = std::localtime(&now);
  char bb[10];
  sprintf(bb, "%02d:%02d", tstruct->tm_hour, tstruct->tm_min);
  return bb;
}

void startServer(int port, std::string username) {
  roomname = username;
  std::thread a(server_listener, port);
  a.detach();
}

void server_listener(int serv_port) {
  room_status = STAT_RUNNING;
  sockaddr_in chat_serv_addr;
  chat_serv_addr.sin_family = AF_INET;
  chat_serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  chat_serv_addr.sin_port = htons(serv_port);
  try {
    chat_listen_so = new TCP_socket();
    chat_listen_so->bind(chat_serv_addr);
    chat_listen_so->listen();
    chat_server_ready = true;
    cv.notify_all();
    while (true) {
      TCP_socket so;
      chat_listen_so->accept(so);
      Data_package recv_data;
      so.recv(&recv_data);
      std::string type = recv_data.fields.at("type");
      if (type == "TYPE_JOIN_CHATROOM") {
        std::string username = recv_data.fields.at("username");
        if (username != roomname) {
          ChatRecord ch;
          ch.who = "sys";
          ch.time = getTime();
          ch.msg = username + " join us.";
          conn_mux.lock();
          broadcastMSG(ch, connected);
          connected.push_back(so);
          conn_mux.unlock();
        } else {
          conn_mux.lock();
          connected.push_back(so);
          conn_mux.unlock();
        }

        std::thread b(server_recv_command, so, username);
        b.detach();
      } else if (type == "TYPE_CLOSE_CHATROOM") {
        delete chat_listen_so;
        chat_listen_so = nullptr;
        chat_server_ready = false;
        break;
      }
    }

  } catch (std::exception e) {
    warn(e.what());
  }
}

void broadcastMSG(ChatRecord &msg, std::vector<TCP_socket> &who) {
  for (TCP_socket so : who) {
    Data_package send_data;
    send_data.fields["type"] = "TYPE_ROOM_MSG";
    send_data.fields["who"] = msg.who;
    send_data.fields["time"] = msg.time;
    send_data.fields["msg"] = msg.msg;
    so.send(&send_data);
  }
}

void server_recv_command(TCP_socket tcpsock, std::string username) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_SERVER_MSG";
  std::string recent;
  msg_mux.lock();
  for (ChatRecord ch : last_three) {
    recent += ch.who + "[" + ch.time + "] " + ch.msg + "\n";
  }
  msg_mux.unlock();
  send_data.fields["message"] =
      "***************************\n"
      "**​Welcome to the chatroom​**\n"
      "***************************\n" +
      recent;
  tcpsock.send(&send_data);

  while (true) {
    pollfd pd;
    pd.events = POLLIN;
    pd.fd = tcpsock.getSockDes();
    int s = poll(&pd, 1, 20);
    if (s < 0) {
      error(strerror(errno));
    } else if (s > 0 && (pd.revents & POLLIN)) {
      Data_package recv_data;
      int len = tcpsock.recv(&recv_data);

      if (len == 0) {
        UL l(conn_mux);
        for (decltype(connected)::const_iterator it = connected.begin();
             it != connected.end(); ++it) {
          if (tcpsock == *it) {
            connected.erase(it);
            break;
          }
        }
        break;
      }
      std::string type = recv_data.fields.at("type");
      if (type == "TYPE_LEAVE_CHATROOM") {
        UL l(conn_mux);
        for (decltype(connected)::const_iterator it = connected.begin();
             it != connected.end(); ++it) {
          if (tcpsock == *it) {
            connected.erase(it);
            break;
          }
        }
        if (username != roomname) {
          ChatRecord ch;
          ch.who = "sys";
          ch.time = getTime();
          ch.msg = username + " leave us.";
          broadcastMSG(ch, connected);
          Data_package exit_cmd;
          exit_cmd.fields["type"] = "TYPE_EXIT_ROOM";
          exit_cmd.fields["action"] = "leave";
          tcpsock.send(&exit_cmd);
        } else {
          ChatRecord ch;
          ch.who = "sys";
          ch.time = getTime();
          ch.msg = "the chatroom is close.";
          broadcastMSG(ch, connected);
          Data_package close_cmd, exit_cmd;
          close_cmd.fields["type"] = "TYPE_EXIT_ROOM";
          close_cmd.fields["action"] = "close room";
          tcpsock.send(&close_cmd);
          exit_cmd.fields["type"] = "TYPE_EXIT_ROOM";
          exit_cmd.fields["action"] = "leave";
          for (TCP_socket so : connected) {
            so.send(&exit_cmd);
          }
          connected.clear();
        }
        break;
      } else if (type == "TYPE_ROOM_MSG") {
        ChatRecord ch;
        ch.time = getTime();
        ch.who = username;
        ch.msg = recv_data.fields.at("message");
        msg_mux.lock();
        last_three.push_back(ch);
        if (last_three.size() > 3) {
          last_three.pop_front();
        }

        msg_mux.unlock();
        conn_mux.lock();
        broadcastMSG(ch, connected);
        conn_mux.unlock();
      } else if (type == "TYPE_DETACH") {
        UL l(conn_mux);
        if (username == roomname) {
          for (decltype(connected)::const_iterator it = connected.begin();
               it != connected.end(); ++it) {
            if (tcpsock == *it) {
              connected.erase(it);
              break;
            }
          }
          Data_package exit_cmd;
          exit_cmd.fields["type"] = "TYPE_EXIT_ROOM";
          exit_cmd.fields["action"] = "leave";
          tcpsock.send(&exit_cmd);
        } else {
          ChatRecord ch;
          ch.who = username;
          ch.time = getTime();
          ch.msg = "detach";
          broadcastMSG(ch, connected);
        }
      }
    }
  }
}
