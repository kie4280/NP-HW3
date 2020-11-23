#include "socket.h"

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <thread>

#define KEY_VAL_SEP ((char)-1)
#define FIELDS_SEP ((char)-2)
#define FIELD_PAYLOAD_SEP ((char)-3)
// udp packet header: size of packet, message id, packet id, next packet id,
// flags
#define UDP_HEADER_SIZE                                                   \
  (sizeof(size_t) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + \
   sizeof(uint8_t))

// tcp packet header: size of packet, packet id, next packet id
#define TCP_HEADER_SIZE (sizeof(size_t) + sizeof(int32_t) + sizeof(int32_t))

#define PACKET_ID_NONEXT ((int32_t)-1)
#define PACKET_ID_RESEND ((int32_t)-2)
#define PACKET_ID_ACK ((int32_t)-3)  // received full message

#define UDP_FLAG_NEEDACK ((uint8_t)1)

void warn(std::string a) { std::cout << a << std::endl; }
void error(std::string a) {
  std::cout << a << std::endl;
  exit(-1);
}

size_t Serialize(std::shared_ptr<char> &out, const Data_package *ud) {
  uint32_t field_bytes = 0, payload_bytes = 0;
  size_t packet_size =
      sizeof(field_bytes) +
      sizeof(payload_bytes);  // size fields for fields and payload
  for (auto it = ud->fields.begin(); it != ud->fields.end(); ++it) {
    packet_size += (*it).first.size() + sizeof(KEY_VAL_SEP) +
                   (*it).second.size() + sizeof(FIELDS_SEP);
  }
  field_bytes = packet_size - sizeof(field_bytes) - sizeof(payload_bytes);
  packet_size += payload_bytes = ud->payload.size();
  out =
      std::shared_ptr<char>(new char[packet_size], [](auto p) { delete[] p; });
  char *memp = out.get();
  memp = (char *)mempcpy(memp, &field_bytes, sizeof(field_bytes));
  memp = (char *)mempcpy(memp, &payload_bytes, sizeof(payload_bytes));

  for (auto it = ud->fields.begin(); it != ud->fields.end(); ++it) {
    memp = (char *)mempcpy(memp, it->first.c_str(), it->first.size());
    *(memp++) = KEY_VAL_SEP;
    memp = (char *)mempcpy(memp, it->second.c_str(), it->second.size());
    *(memp++) = FIELDS_SEP;
  }
  *(memp - 1) = FIELD_PAYLOAD_SEP;
  mempcpy(memp, ud->payload.data(), ud->payload.size());

  return packet_size;
}

void Deserialize(char *in, Data_package *ud) {
  uint32_t field_bytes = 0, payload_bytes = 0;
  char *data = in;
  memcpy(&field_bytes, data, sizeof(field_bytes));
  data += sizeof(field_bytes);
  memcpy(&payload_bytes, data, sizeof(payload_bytes));
  data += sizeof(payload_bytes);
  std::string key_s, val_s;
  bool val = false;
  for (;; ++data) {
    if (*data == FIELDS_SEP || *data == FIELD_PAYLOAD_SEP) {
      ud->fields[key_s] = val_s;
      val = false;
      key_s.clear();
      val_s.clear();
    } else if (*data == KEY_VAL_SEP) {
      val = true;
    } else {
      if (val) {
        val_s.push_back(*data);
      } else {
        key_s.push_back(*data);
      }
    }
    if (*data == FIELD_PAYLOAD_SEP) {
      ++data;
      break;
    }
  }
  ud->payload.reserve(payload_bytes);
  ud->payload.assign(data, data + payload_bytes);
}

UDP_socket::UDP_socket(size_t msg_queue_size) : msg_queue_size(msg_queue_size) {
  if ((UDPsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    warn(strerror(errno));
    error("Error creating UDP socket");
  }
  instances = std::shared_ptr<std::atomic_int>(new std::atomic_int(0));
  msg_id_gen = std::shared_ptr<std::atomic_int32_t>(new std::atomic_int32_t(0));
  (*instances)++;
}

UDP_socket::UDP_socket(const UDP_socket &old) {
  UDPsock = old.UDPsock;
  closed = old.closed;
  instances = old.instances;
  msg_id_gen = old.msg_id_gen;
  (*instances)++;
}

void UDP_socket::bind(sockaddr_in addr) {
  if (::bind(UDPsock, (sockaddr *)&addr, sizeof(addr))) {
    warn(strerror(errno));
    error("Error binding to udp socket");
  }
}

void encode_UDP_packet(char *packet_out, size_t msg_size, int32_t msgid,
                       int32_t packet_id, int32_t packet_next_id, uint8_t flags,
                       char *msg_content) {
  size_t packet_size = UDP_HEADER_SIZE + msg_size;
  char *p = (char *)mempcpy(packet_out, &packet_size, sizeof(packet_size));
  p = (char *)mempcpy(p, &msgid, sizeof(msgid));
  p = (char *)mempcpy(p, &packet_id, sizeof(packet_id));
  p = (char *)mempcpy(p, &packet_next_id, sizeof(packet_next_id));
  p = (char *)mempcpy(p, &flags, sizeof(flags));
  if (msg_content != nullptr && msg_size > 0) {
    p = (char *)mempcpy(p, msg_content, msg_size);
  }
}

void decode_UDP_packet(char *packet_in, size_t *msg_size, int32_t *msgid,
                       int32_t *packet_id, int32_t *packet_next_id,
                       uint8_t *flags, char *msg_content) {
  size_t packet_size;
  char *pos = packet_in;
  memcpy(&packet_size, pos, sizeof(packet_size));
  *msg_size = packet_size - UDP_HEADER_SIZE;
  pos += sizeof(packet_size);
  memcpy(msgid, pos, sizeof(*msgid));
  pos += sizeof(*msgid);
  memcpy(packet_id, pos, sizeof(*packet_id));
  pos += sizeof(*packet_id);
  memcpy(packet_next_id, pos, sizeof(*packet_next_id));
  pos += sizeof(*packet_next_id);
  memcpy(flags, pos, sizeof(*flags));
  pos += sizeof(*flags);
  if (msg_content != nullptr && *msg_size > 0) {
    memcpy(msg_content, pos, *msg_size);
  }
}

int UDP_socket::sendpacket(sockaddr_in dest, char *data, size_t msg_size,
                           int32_t msgid, int32_t id, int32_t next_id,
                           uint8_t flags) {
  char packet[UDP_HEADER_SIZE + MAX_UDP_MSG_SIZE];
  encode_UDP_packet(packet, msg_size, msgid, id, next_id, flags, data);
  ssize_t sl = ::sendto(UDPsock, packet, msg_size + UDP_HEADER_SIZE, 0,
                        (const sockaddr *)&dest, sizeof(dest));
  if (sl < 0) {
    warn(strerror(errno));
    if (errno == EPIPE) {
      closed = true;
    }
    return -1;
  }

  return 0;
}

int UDP_socket::send(sockaddr_in dest, Data_package *data, bool retry) {
  std::shared_ptr<char> buf;
  int32_t serial_len = Serialize(buf, data);
  int index = 0;
  uint8_t flags = retry ? UDP_FLAG_NEEDACK : 0;
  int32_t msgid1 = (*msg_id_gen)++;
  int32_t packet_id = 0, packet_next_id = 1;
  bool success = false;
  while (!success) {
    while (serial_len > MAX_UDP_MSG_SIZE * index) {
      // std::cout << "send" <<std::endl;
      int32_t packet_len =
          std::min(MAX_UDP_MSG_SIZE, serial_len - MAX_UDP_MSG_SIZE * index);
      if (serial_len <= MAX_UDP_MSG_SIZE * (index + 1))
        packet_next_id = PACKET_ID_NONEXT;
      if (sendpacket(dest, buf.get() + index * MAX_UDP_MSG_SIZE, packet_len,
                     msgid1, packet_id, packet_next_id, flags) < 0) {
        return -1;
      }
      sockaddr_in src;
      socklen_t len = sizeof(src);
      char buf[MAX_UDP_MSG_SIZE];
      uint8_t flags;
      size_t msg_size;
      int32_t msgid2, id, next_id;
      if (retry) {
        int r = recvpacket(&src, &len, buf, &msg_size, &msgid2, &id, &next_id,
                           &flags, false);
        if (r < 0) {
          return -1;
        } else if (r > 0) {
          if (custom_types::UDP_MSG(src, msgid2) ==
              custom_types::UDP_MSG(dest, msgid1)) {
            if (next_id == PACKET_ID_RESEND) {
              index = id;
              packet_id = id;
              packet_next_id = packet_id + 1;
              continue;
            }
            if (id == PACKET_ID_ACK) {
              success = true;
            }

          } else {
            insert_check(&src, msg_size, msgid2, id, next_id, flags, buf);
          }
        }
      }

      index++;
      packet_id++;
      packet_next_id++;
    }

    std::chrono::time_point<std::chrono::system_clock> timer =
        std::chrono::system_clock::now();
    while (retry && !success) {
      sockaddr_in src;
      socklen_t len = sizeof(src);
      char buf[MAX_UDP_MSG_SIZE];
      uint8_t flags;
      size_t msg_size;
      int32_t msgid2, id, next_id;
      int r = recvpacket(&src, &len, buf, &msg_size, &msgid2, &id, &next_id,
                         &flags, false);
      if (r < 0) {
        return -1;
      } else if (r > 0) {
        if (custom_types::UDP_MSG(src, msgid2) ==
            custom_types::UDP_MSG(dest, msgid1)) {
          if (id == PACKET_ID_ACK) {
            success = true;
          } else {
            index = id;
            packet_id = id;
            packet_next_id = packet_id + 1;
          }
          break;
        } else {
          insert_check(&src, msg_size, msgid2, id, next_id, flags, buf);
        }
      } else {
        std::this_thread::yield();
      }

      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now() - timer)
              .count() > 200) {
        success = true;
        break;
      }
    }
  }
  return 0;
}

int UDP_socket::recvpacket(sockaddr_in *src, socklen_t *addr_len, char *data,
                           size_t *msg_size, int32_t *msgid, int32_t *id,
                           int32_t *next_id, uint8_t *flags, bool wait) {
  char buf[UDP_HEADER_SIZE + MAX_UDP_MSG_SIZE];
  ssize_t rl = ::recvfrom(UDPsock, buf, UDP_HEADER_SIZE + MAX_UDP_MSG_SIZE,
                          wait ? 0 : MSG_DONTWAIT, (sockaddr *)src, addr_len);
  if (rl < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    }
    warn(strerror(errno));
    if (errno == EPIPE) {
      closed = true;
    }
    return -1;
  }
  decode_UDP_packet(buf, msg_size, msgid, id, next_id, flags, data);

  return 1;
}

int UDP_socket::recv(sockaddr_in *src, Data_package *data, bool retry) {
  char buf[MAX_UDP_MSG_SIZE];
  uint8_t flags;
  size_t msg_size;
  int32_t msgid1, id, next_id;
  sockaddr_in placeholder;
  if (src == nullptr) {
    src = &placeholder;
  }
  socklen_t len = sizeof(*src);
  do {
    // std::cout << "recv" <<std::endl;
    for (auto it = unfinished.begin(); it != unfinished.end(); ++it) {
      if ((*it).second.next_id == PACKET_ID_NONEXT) {
        *src = (*it).first.addr;
        Deserialize((*it).second.data.data(), data);
        unfinished.erase(it);
        for (int a = 0; a < 3; ++a) {
          if (next_id == PACKET_ID_NONEXT && flags & UDP_FLAG_NEEDACK) {
            sendpacket(*src, nullptr, 0, (*it).first.msgid, PACKET_ID_ACK,
                       PACKET_ID_ACK, 0);
          }
        }

        return 1;
      }
      std::chrono::duration<double> elapsed =
          std::chrono::system_clock::now() - (*it).second.last_received;
      if (elapsed > std::chrono::milliseconds(100)) {
        sendpacket((*it).first.addr, nullptr, 0, (*it).first.msgid,
                   (*it).second.next_id, PACKET_ID_RESEND, 0);
        (*it).second.last_received = std::chrono::system_clock::now();
      }
    }

    pollfd po;
    po.fd = UDPsock;
    po.events = POLLIN;
    int stat = poll(&po, 1, 100);
    if (stat < 0) {
      error(strerror(errno));
    } else if (stat > 0 && (po.revents & POLLIN)) {
      int r = recvpacket(src, &len, buf, &msg_size, &msgid1, &id, &next_id,
                         &flags, true);

      if (r < 0) {
        return -1;
      } else if (r > 0) {
        insert_check(src, msg_size, msgid1, id, next_id, flags, buf);

        // std::cout << unfinished.size() << std::endl;
      }
    }
  } while (retry);

  return 0;
}

void UDP_socket::insert_check(sockaddr_in *src, size_t msg_size, int32_t msgid,
                              int32_t id, int32_t next_id, uint8_t flags,
                              char *data) {
  if (id == PACKET_ID_ACK) {
    return;
  }
  if (unfinished.find(custom_types::UDP_MSG(*src, msgid)) != unfinished.end()) {
    custom_types::unfinished_msg &u =
        unfinished[custom_types::UDP_MSG(*src, msgid)];
    if (id == u.next_id) {
      u.last_received = std::chrono::system_clock::now();
      u.data.insert(u.data.end(), data, data + msg_size);
      u.next_id = next_id;

    } else if (id > u.next_id) {
      sendpacket(*src, nullptr, 0, msgid, u.next_id, PACKET_ID_RESEND, 0);
    }

  } else {
    if (id == 0) {
      custom_types::unfinished_msg um;
      um.next_id = next_id;
      um.last_received = std::chrono::system_clock::now();
      um.data.insert(um.data.end(), data, data + msg_size);
      unfinished[custom_types::UDP_MSG(*src, msgid)] = std::move(um);
    } else {
      sendpacket(*src, nullptr, 0, msgid, 0, PACKET_ID_RESEND, 0);
    }
  }
}

UDP_socket::~UDP_socket() {
  if (--(*instances) == 0) {
    close(UDPsock);
    warn("UDP sock destroyed");
  }
}

TCP_socket::TCP_socket() {
  if ((TCPsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    warn(strerror(errno));
    error("Error creating TCP socket");
  }
  instances = std::shared_ptr<std::atomic_int>(new std::atomic_int(0));
  (*instances)++;
}

TCP_socket::TCP_socket(const TCP_socket &old) {
  TCPsock = old.TCPsock;
  closed = old.closed;
  connect_addr = old.connect_addr;
  instances = old.instances;
  (*instances)++;
}

TCP_socket::TCP_socket(int sock) {
  TCPsock = sock;
  instances = std::shared_ptr<std::atomic_int>(new std::atomic_int(0));
  (*instances)++;
}

int TCP_socket::send(const Data_package *data) {
  std::shared_ptr<char> buf;
  int32_t len = Serialize(buf, data);
  char *start = buf.get();
  ssize_t sendlen = ::send(TCPsock, &len, sizeof(len), MSG_NOSIGNAL);
  if (sendlen < 0) {
    warn("Error sending using TCP");
    warn(strerror(errno));
    return -1;
  }
  while (len > 0) {
    size_t msg_size = std::min(MAX_TCP_MSG_SIZE, len);

    ssize_t sendlen = ::send(TCPsock, start, msg_size, MSG_NOSIGNAL);
    if (sendlen < 0) {
      warn("Error sending using TCP");
      warn(strerror(errno));
      return -1;
    }

    len -= MAX_TCP_MSG_SIZE;
    start += MAX_TCP_MSG_SIZE;
  }

  return 0;
}

int TCP_socket::recv(Data_package *data) {
  pollfd po;
  po.fd = TCPsock;
  po.events = POLLIN;
  while (true) {
    int stat = poll(&po, 1, 100);
    if (stat < 0) {
      error(strerror(errno));
    } else if (stat > 0 && (po.revents & POLLIN)) {
      break;
    }
  }
  int32_t msg_len;
  ssize_t recvlen = ::recv(TCPsock, &msg_len, sizeof(msg_len), MSG_WAITALL);

  if (recvlen > 0) {
    std::shared_ptr<char> msg(new char[msg_len], [](auto p) { delete[] p; });
    char *start = msg.get();
    while (msg_len > 0) {
      ssize_t rl = ::recv(TCPsock, start, std::min(MAX_TCP_MSG_SIZE, msg_len),
                          MSG_WAITALL);
      if (rl > 0) {
        start += rl;
        msg_len -= rl;
        recvlen += rl;
      } else if (rl == 0) {
        closed = true;
      } else {
        warn("Error receiving using TCP");
        warn(strerror(errno));
        return -1;
      }
    }

    Deserialize(msg.get(), data);

  } else if (recvlen == 0) {
    closed = true;
  } else {
    warn("Error receiving using TCP");
    warn(strerror(errno));
    return -1;
  }

  return recvlen;
}

void TCP_socket::connect(sockaddr_in serv_addr) {
  if ((::connect(TCPsock, (const sockaddr *)&serv_addr, sizeof(serv_addr))) <
      0) {
    warn(strerror(errno));
    error("Error connecting using TCP");
  }
}

void TCP_socket::bind(sockaddr_in addr) {
  if (::bind(TCPsock, (const sockaddr *)&addr, sizeof(addr)) < 0) {
    warn(strerror(errno));
    error("Error binding to tcp socket");
  }
}

void TCP_socket::listen(int listensize) {
  if (::listen(TCPsock, listensize) < 0) {
    warn(strerror(errno));
    error("Error listening on tcp socket");
  }
}

TCP_socket TCP_socket::accept() {
  int so;
  sockaddr_in connect_addr;
  socklen_t len = sizeof(connect_addr);
  if ((so = ::accept(TCPsock, (sockaddr *)&connect_addr, &len)) < 0) {
    if (errno == EINTR) {
    } else {
      warn(strerror(errno));
      error("Error accepting tcp connection");
    }
  }
  TCP_socket sock(so);
  sock.connect_addr = connect_addr;

  return sock;
}

void TCP_socket::disconnect() { shutdown(TCPsock, SHUT_RDWR); }

TCP_socket::~TCP_socket() {
  if (--(*instances) == 0) {
    shutdown(TCPsock, SHUT_RDWR);
    close(TCPsock);
    warn("TCP sock destroyed");
  }
}

int TCP_socket::getSockDes() { return TCPsock; }

bool TCP_socket::operator==(const TCP_socket &sock) const {
  return TCPsock == sock.TCPsock;
}
