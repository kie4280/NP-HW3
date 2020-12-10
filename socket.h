#ifndef HW1_SOCKET_H
#define HW1_SOCKET_H

#include <netinet/in.h>

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#define MAX_UDP_MSG_SIZE 1024
#define MAX_TCP_MSG_SIZE 1000000

struct Data_package {
  std::vector<char> payload;
  std::unordered_map<std::string, std::string> fields;
};

namespace custom_types {
struct UDP_MSG {
  UDP_MSG(const sockaddr_in &in, const int32_t msgid) {
    addr = in;
    UDP_MSG::msgid = msgid;
  }

  UDP_MSG(const UDP_MSG &old) {
    addr = old.addr;
    msgid = old.msgid;
  }

  bool operator==(const UDP_MSG &co) const {
    if (co.addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
        co.addr.sin_port == addr.sin_port && co.msgid == msgid) {
      return true;
    }
    return false;
  }

 public:
  sockaddr_in addr;
  int32_t msgid;
};

class UDP_MSG_HASH {
 public:
  size_t operator()(const UDP_MSG &com) const {
    return std::hash<uint32_t>()(com.addr.sin_addr.s_addr) ^
           (std::hash<uint16_t>()(com.addr.sin_port) << 16) ^
           std::hash<int32_t>()(com.msgid);
  }
};

struct unfinished_msg {
  int32_t next_id = 0;
  std::chrono::time_point<std::chrono::system_clock> last_received;
  std::vector<char> data;
};
}  // namespace custom_types

size_t Serialize(std::shared_ptr<char> &out, const Data_package *data);
void Deserialize(char *in, Data_package *data);
void decode_UDP_packet(char *packet_in, size_t *msg_size, int32_t *msgid,
                       int32_t *packet_id, int32_t *packet_next_id,
                       uint8_t flags, char *msg_content);
void encode_UDP_packet(char *packet_out, size_t msg_size, int32_t msgid,
                       int32_t packet_id, int32_t packet_next_id, uint8_t flags,
                       char *msg_content);

int reliableUDPsend();
int reliableUDPreceive();

class TCP_socket {
 public:
  TCP_socket();
  TCP_socket(const TCP_socket &);
  ~TCP_socket();

  int send(const Data_package *);
  int recv(Data_package *);
  void connect(sockaddr_in);
  void disconnect();
  void bind(sockaddr_in);
  void listen(int listensize = 20);
  bool accept(TCP_socket &sock);

  int getSockDes();
  bool isOpen();
  bool operator==(const TCP_socket &sock) const;
  TCP_socket &operator=(const TCP_socket &sock);

 private:
  int TCPsock;
  bool closed = false;
  sockaddr_in connect_addr;
  std::shared_ptr<std::atomic_int> instances;
  TCP_socket(int);
};

class UDP_socket {
 public:
  UDP_socket(size_t msg_queue_size = 100);
  UDP_socket(const UDP_socket &);
  ~UDP_socket();
  int send(sockaddr_in dest, Data_package *data, bool retry = true);
  int recv(sockaddr_in *src, Data_package *data, bool retry = true);
  void bind(sockaddr_in);

 private:
  int UDPsock;
  bool closed = false;
  std::future<int> recv_future;
  std::shared_ptr<std::atomic_int> instances;
  std::shared_ptr<std::atomic_int32_t> msg_id_gen;
  size_t msg_queue_size;
  std::unordered_map<custom_types::UDP_MSG, custom_types::unfinished_msg,
                     custom_types::UDP_MSG_HASH>
      unfinished;
  int sendpacket(sockaddr_in dest, char *data, size_t msg_size, int32_t msgid,
                 int32_t id, int32_t next_id, uint8_t flags);
  int recvpacket(sockaddr_in *src, socklen_t *addr_len, char *data,
                 size_t *msg_size, int32_t *msgid, int32_t *id,
                 int32_t *next_id, uint8_t *flags, bool wait = true);
  void insert_check(sockaddr_in *src, size_t msg_size, int32_t msgid,
                    int32_t id, int32_t next_id, uint8_t flags, char *data);
};

void warn(std::string a);
void error(std::string a);

#endif