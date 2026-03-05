#include "usb.h"
#include <cstring>

static uint8_t buffer[1024];

void println(float f) {
  std::string s = std::to_string(f);
  println(s);
}

void println(std::string& s) {
  if(s.size() > 1000) {
    return;
  }
  strcpy(reinterpret_cast<char *>(buffer), s.c_str());
  size_t size = s.size();
  buffer[size++] = '\n';
  buffer[size] = '\0';
  HAL_UART_Transmit(&huart4, buffer, size, 100);
}