#ifndef DEFS_H
#define DEFS_H

#include "pico/stdlib.h"

// Constants
#define LED 16
#define TCP_PORT 4322
#define BUF_SIZE 2048
#define POLL_TIME_S 5
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

typedef struct DATA_T_ {
  float temp;
  uint64_t time;
  char temp_unit;
} DATA_T;

typedef struct STATE_T_ {
  TCP_SERVER_T* server;
  DATA_T* data;
} STATE_T;

// Messages
static char welcome_msg[] = "Welcome to this lwIP TCP server\n- Send 'TEMP' to receive the current temperature\n- Send 'TIME' to receive the current time\n";

static char cmd_temp[] = "TEMP";
static char cmd_time[] = "TIME";
static char cmd_set[] = "SET";

#endif /* DEFS_H */
