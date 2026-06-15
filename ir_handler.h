#ifndef _IR_HANDLER_H_
#define _IR_HANDLER_H_

#include <stdint.h>
#include <stdbool.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <ir_LG.h>
#include <IRac.h>
#include "ac_state.h"

/* =========================
 * IR Objects (defined in ir_handler.c)
 * ========================= */
extern IRLgAc ac;
extern IRrecv irrecv;

/* =========================
 * IR Handler Functions
 * ========================= */
void ir_handler_init(void);
void ir_handler_send(void);
bool ir_handler_decode(decode_results *results);

#endif  // _IR_HANDLER_H_
