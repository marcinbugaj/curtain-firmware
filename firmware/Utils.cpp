#include "Utils.h"

#include <cstdio>
#include <cstdlib>

[[noreturn]] void error_out(const char *msg) {
  printf("%s\n", msg);
  fflush(stdout);
  abort();
}