#ifndef DEFS_H
#define DEFS_H

#include "pico/stdlib.h"

// Constants
#define LED 16
#define TCP_PORT 4444
#define BUF_SIZE 2048
#define POLL_TIME_S 10
#define TEST_ITERATIONS 10

#define TEMPERATURE_UNITS 'C'

// Structs
typedef struct TCP_SERVER_T_ {
  struct tcp_pcb *server_pcb;
  struct tcp_pcb *client_pcb;
  uint8_t buffer_sent[BUF_SIZE];
  uint8_t buffer_recv[BUF_SIZE];
  int sent_len;
  int recv_len;
} TCP_SERVER_T;

// Messages
static char* msg_welcome = "Welcome to this lwIP TCP server\n- Send 'TEMP' to receive the current temperature\n- Send 'TIME' to receive the current time\n- Send 'SET' followed by 'C' or 'F' to set the temperature unit\n";
static char* msg_invalid_unit = "Invalid unit!\nAccepted units are 'C' and 'F'\n";
static char* msg_unit_set = "New temperature unit set\n";
static char* msg_invalid_command = "Unrecognised command!\n";

static char* cmd_temp = "TEMP";
static char* cmd_time = "TIME";
static char* cmd_set = "SET";

#endif /* DEFS_H */
