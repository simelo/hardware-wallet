
#include "droplet.h"

char *sprint_coins(uint64_t coins, int precision_exp, size_t sz, char *msg) {
  uint64_t div, mod;
  char* eos = msg + sz;
  char* ptr = eos;
  // EOS
  *ptr = 0;
  // Skip least significant decimal digits
  for (--ptr, div = coins, mod = 0; mod == 0 && precision_exp > 0; --precision_exp) {
    mod = div % 10;
    div = div / 10;
  }
  if (precision_exp > 0) {
    *ptr = '0' + mod;
    --ptr;
    if ((--sz) == 0) {
      return NULL;
    }
  }
  // Print decimal digits
  for (; div > 0 && precision_exp > 0 && sz > 0; --precision_exp, --sz, --ptr) {
    mod = div % 10;
    div = div / 10;
    *ptr = '0' + mod;
  }
  if (sz == 0) {
    // No space left in buffer
    return NULL;
  }
  if (ptr != eos + 1) {
    // Not an integer value
    *ptr = '.';
    if ((--sz) == 0) {
      return NULL;
    }
    --ptr;
  }
  // A fraction of 1 SKY
  if (div == 0) {
    *ptr = '0';
    return ptr;
  }
  // Print integer part
  for (; div > 0 && sz > 0; --sz, --ptr) {
    mod = div % 10;
    div = div / 10;
    *ptr = '0' + mod;
  }
  if (div > 0) {
    // No space left in buffer
    return NULL;
  }
  return ++ptr;
}