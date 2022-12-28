// Copyright (C) 2003  Davis E. King (davis@dlib.net)
// License: Boost Software License
// Extracted from dlib

#include "UrlDecode.h"

inline unsigned char from_hex(unsigned char ch) {
  if (ch <= '9' && ch >= '0')
    ch -= '0';
  else if (ch <= 'f' && ch >= 'a')
    ch -= 'a' - 10;
  else if (ch <= 'F' && ch >= 'A')
    ch -= 'A' - 10;
  else
    ch = 0;
  return ch;
}

const std::string urldecode(const std::string &str) {
  using namespace std;
  string result;
  string::size_type i;
  for (i = 0; i < str.size(); ++i) {
    if (str[i] == '+') {
      result += ' ';
    } else if (str[i] == '%' && str.size() > i + 2) {
      const unsigned char ch1 = from_hex(str[i + 1]);
      const unsigned char ch2 = from_hex(str[i + 2]);
      const unsigned char ch = (ch1 << 4) | ch2;
      result += ch;
      i += 2;
    } else {
      result += str[i];
    }
  }
  return result;
}