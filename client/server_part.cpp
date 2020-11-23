#include <poll.h>

#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#include "client/client.h"
#include "socket.h"

#define STAT_NONEXIST 0
#define STAT_RUNNING 1
#define STAT_CLOSED 2

typedef std::unique_lock<std::mutex> UL;

struct ChatRecord {
  std::string who;
  std::string time;
  std::string msg;
};

std::string roomname;
std::deque<ChatRecord> latest_msg, last_three;
std::vector<TCP_socket> connected;
std::mutex conn_mux;
std::mutex msg_mux;
int room_status;
uint32_t current_msg_id(1);

void server_listener(int serv_port);
void server_recv_command(TCP_socket tcpsock);
void server_broadcast_worker();
void broadcastMSG(ChatRecord &msg, std::vector<TCP_socket> &who);

TCP_socket chat_listen_so;

std::string getTime() {
  std::time_t now = std::time(0);
  std::tm *tstruct = std::localtime(&now);
  return std::to_string(tstruct->tm_hour + 1) + ":" +
         std::to_string(tstruct->tm_min);
}

void startServer(int port) {
  std::thread a(server_listener, port);
  std::thread b(server_broadcast_worker);
  a.detach();
  b.detach();
}

void server_listener(int serv_port) {
  room_status = STAT_RUNNING;
  sockaddr_in chat_serv_addr;
  chat_serv_addr.sin_family = AF_INET;
  chat_serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  chat_serv_addr.sin_port = htons(serv_port);
  try {
    chat_listen_so.bind(chat_serv_addr);
    chat_listen_so.listen();
    while (true) {
      TCP_socket so = chat_listen_so.accept();
      std::thread b(server_recv_command, so);
      b.detach();
    }

  } catch (std::exception e) {
    warn(e.what());
  }
}

void broadcastMSG(ChatRecord &msg, std::vector<TCP_socket> &who) {
  for (TCP_socket so : who) {
    std::string message = msg.who + "[" + msg.time + "] : " + msg.msg;
    Data_package send_data;
    send_data.fields["message"] = message;
    so.send(&send_data);
  }
}

void server_broadcast_worker() {

}

void server_recv_command(TCP_socket tcpsock) {
  Data_package send_data;
  send_data.fields["type"] = "TYPE_MSG";
  send_data.fields["message"] =
      "***************************\n"
      "**​Welcome to the chatroom​**\n"
      "***************************\n";
  tcpsock.send(&send_data);
  Data_package recv_data;
  tcpsock.recv(&recv_data);
  ChatRecord ch;
  std::string username = recv_data.fields["username"];
  ch.who = "sys";
  ch.time = getTime();
  ch.msg = username + " join us.";
  if (username != roomname) {
    broadcastMSG(ch, connected);
  }

  conn_mux.lock();
  connected.push_back(tcpsock);
  conn_mux.unlock();

  while (true) {
    pollfd pd;
    pd.events = POLLIN;
    pd.fd = tcpsock.getSockDes();
    int s = poll(&pd, 1, 20);
    if (s < 0) {
      error(strerror(errno));
    } else if (s > 0 && (pd.revents & POLLIN)) {
      Data_package data_recv;
      int len = tcpsock.recv(&data_recv);
      std::string type = data_recv.fields["type"];
      if (type == ROOM_OP_LEAVE || len == 0) {
        ChatRecord ch;
        ch.msg = "sys";
        ch.time = getTime();
        ch.msg = username + " leave us.";
        for (decltype(connected)::const_iterator it = connected.begin();
             it != connected.end(); ++it) {
          if (tcpsock == *it) {
            connected.erase(it);
            break;
          }
        }
        if (username != roomname) {
          broadcastMSG(ch, connected);
        }

        break;
      } else if (type == "ROOM_MSG") {
        ChatRecord ch;
        ch.time = getTime();
        ch.who = username;
        ch.msg = recv_data.fields["message"];
        msg_mux.lock();
        latest_msg.push_back(ch);
        msg_mux.unlock();
      } else if (type == "") {}
    }
  }
}
