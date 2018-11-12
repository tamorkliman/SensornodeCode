//"greenhouse1"
#include "self_identification.h"
#include <stdio.h>
#include "uart.h"

SelfID new_self_id(uint8_t type_num, SelfID s) {
  s.type_num = type_num;
  s.init = &self_id_init;
  s.read = &self_id_read;
  s.destroy = &self_id_destroy;
  return s;
}

void self_id_init(SelfID s){
  if (0 && s.type_num) {} // no compiler warnings
  uart_puts_P(PSTR("Self Identifier has been initialized\r\n"));
}

void self_id_read(char *data){
  strcpy(data, MY_ID);
}

void self_id_destroy(SelfID s){
  if (0 && s.type_num) {} // no compiler warnings
  uart_puts_P(PSTR("Self Identifier has been de-initialized\r\n"));
}

