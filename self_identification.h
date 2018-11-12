#ifndef SELF_IDENTIFICATION_H_
#define SELF_IDENTIFICATION_H_

#include <string.h>
#include <stdint.h>
#include "module.h"

#define SELF_ID_IDENTIFIER_STRING "SELFID"

#define MY_ID "greenhouse1"
typedef Module SelfID;

SelfID new_self_id(uint8_t, SelfID s);

void self_id_init(SelfID s);

void self_id_read(char *);

void self_id_destroy(SelfID s);

#endif

