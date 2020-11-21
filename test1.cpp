#include <iostream>

#include "socket.h"

int main() {
  Data_package data;
  data.fields["er"] = "dfg";
  const char *a = "sdfsdf";
  data.payload.assign(a, a + sizeof(a));
  std::shared_ptr<char> b;
  Serialize(b, &data);
  Data_package out;
  Deserialize(b.get(), &out);

  size_t bs;
  size_t *c = &bs;
  size_t v1 = 10000, v2 = 100000;
  *c = v1 - v2;
  std::cout << bs << std::endl;

  return 0;
}