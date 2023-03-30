#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "lwip/err.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

// credentials.h stores the ssid and password as char[]. You must make this youself for obvious reasons
#include "credentials.h"
#include "defs.h"
#include "utils.h"

static err_t tcp_server_close(void *arg) {
  STATE_T *state = (STATE_T*)arg;
  err_t err = ERR_OK;
  if (state->server->client_pcb != NULL) {
    tcp_arg(state->server->client_pcb, NULL);
    tcp_poll(state->server->client_pcb, NULL, 0);
    tcp_sent(state->server->client_pcb, NULL);
    tcp_recv(state->server->client_pcb, NULL);
    tcp_err(state->server->client_pcb, NULL);
    err = tcp_close(state->server->client_pcb);
    if (err != ERR_OK) {
      printf("close failed %d, calling abort\n", err);
      tcp_abort(state->server->client_pcb);
      err = ERR_ABRT;
    }
    state->server->client_pcb = NULL;
  }
  if (state->server->server_pcb) {
    tcp_arg(state->server->server_pcb, NULL);
    tcp_close(state->server->server_pcb);
    state->server->server_pcb = NULL;
  }
  return err;
}

/**
 * Convert status codes into meaningful output
 *
 * @param arg	    the state struct
 * @param status    the status code
 */
static err_t tcp_server_result(void *arg, int status) {
  STATE_T *state = (STATE_T*)arg;
  if (status == 0) {
    printf("test success\n");
  } else {
    printf("test failed %d\n", status);
  }
  return tcp_server_close(arg);
}

/**
 * Called when the client acknowledges the sent data
 *
 * @param arg	  the state struct
 * @param tpcb	  the connection PCB for which data has been acknowledged
 * @param len	  the amount of bytes acknowledged
 */
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  STATE_T *state = (STATE_T*)arg;
  printf("tcp_server_sent %u\n", len);
  state->server->sent_len += len;

  if (state->server->sent_len >= BUF_SIZE) {

    // We should get the data back from the client
    state->server->recv_len = 0;
    printf("Waiting for buffer from client\n");
  }

  return ERR_OK;
}

/**
 * Function to send the data to the client
 *
 * @param arg	      the state struct
 * @param tcp_pcb     the client PCB
 * @param data	      the data to send
 */
err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb, u_int8_t data[])
{
  STATE_T *state = (STATE_T*)arg;
 
  // TODO: Change this from strlen to something better
  memcpy(state->server->buffer_sent, data, strlen(data));

  state->server->sent_len = 0;
  printf("Writing %ld bytes to client\n", BUF_SIZE);
  // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
  // can use this method to cause an assertion in debug mode, if this method is called when
  // cyw43_arch_lwip_begin IS needed
  cyw43_arch_lwip_check();

  // Write data for sending but does not send it immediately
  // To force writing we can call tcp_output after tcp_write
  err_t err = tcp_write(tpcb, state->server->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    printf("Failed to write data %d\n", err);
    return tcp_server_result(arg, -1);
  }
  return ERR_OK;
}

/**
 * The method called when data is received at the host
 *
 * @param arg	  the state struct
 * @param tpcb	  the connection PCB which received data
 * @param p	  the received data
 * @param err	  an error code if there has been an error receiving
 */
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  STATE_T *state = (STATE_T*)arg;
  if (!p) {
    return tcp_server_result(arg, -1);
  }

  // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
  // can use this method to cause an assertion in debug mode, if this method is called when
  // cyw43_arch_lwip_begin IS needed
  cyw43_arch_lwip_check();
  if (p->tot_len > 0) {
    printf("tcp_server_recv %d/%d err %d\n", p->tot_len, state->server->recv_len, err);

    // Receive the buffer
    const uint16_t buffer_left = BUF_SIZE - state->server->recv_len;
    state->server->recv_len += pbuf_copy_partial(p, state->server->buffer_recv + state->server->recv_len,
					 p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);

    // Called once data has been processed to advertise a larger window
    tcp_recved(tpcb, p->tot_len);
  }
  pbuf_free(p);

  // Have we have received the whole buffer
  if (state->server->recv_len == BUF_SIZE) {

    // check it matches
    //if (memcmp(state->server->buffer_sent, state->server->buffer_recv, BUF_SIZE) != 0) {
    //  printf("buffer mismatch\n");
    //  return tcp_server_result(arg, -1);
    //}
    printf("tcp_server_recv buffer ok\n");

    return ERR_OK;
  }
  return ERR_OK;
}

/**
 * The function for tcp poll callbacks
 *
 * @param arg	  the state struct
 * @param tpcb	  the relevant TCP Protcol Control Block
 */ 
static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
  STATE_T *state = (STATE_T*)arg;

  printf("Poll function called - reading temperature and time\n");

  state->data->temp = read_temperature(state->data->temp_unit);
  state->data->time = read_time();

  return ERR_OK;
}

/**
 * The function called when a critical error occurs
 *
 * @param arg	  the state struct
 * @param err	  the error code
 */
static void tcp_server_err(void *arg, err_t err) {
  if (err != ERR_ABRT) {
    printf("tcp_client_err_fn %d\n", err);
    tcp_server_result(arg, err);
  }
}

/**
 * The function called when a client connects
 *
 * @param arg		the state struct
 * @param client_pcb	the new connection PCB
 * @param err		the error code (if present)
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
  STATE_T *state = (STATE_T*)arg;
  printf("accept");
  if (err != ERR_OK || client_pcb == NULL) {
    printf("Failed to accept\n");
    tcp_server_result(arg, err);
    return ERR_VAL;
  }
  printf("clear");

  printf("Client connected\n");

  state->server->client_pcb = client_pcb;
  tcp_arg(client_pcb, state);

  // Specifies the callback function that should be called when data has been acknowledged by the client
  tcp_sent(client_pcb, tcp_server_sent);

  // Specifies the callback function that should be called when data has arrived
  tcp_recv(client_pcb, tcp_server_recv);

  // Specifies the polling interval and the callback function that should be called to poll the application
  tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);

  // Specifies the callback function called if a fatal error has occurred
  tcp_err(client_pcb, tcp_server_err);

  return tcp_server_send_data(arg, state->server->client_pcb, welcome_msg);
}

/**
 * Initialises the TCP server
 *
 * @param state	    the state struct
 */
bool tcp_server_open(STATE_T *state){
  printf("Starting TCP server at %s:%u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);

  // Creates s new TCP protocol control block but doesn't place it on any of the TCP PCB lists. The PCB is not put on any list until it is bound
  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if(!pcb) {
    printf("Failed to create PCB\n");
    gpio_put(LED, 1);
    return false;
  }

  // Binds the connection to a local port number
  err_t err = tcp_bind(pcb, NULL, TCP_PORT);

  if(err){
    printf("Failed to bind to port\n");
    gpio_put(LED, 1);
    return false;
  }

  // Set the state to LISTEN. Returns a more memory efficient PCB
  state->server->server_pcb = tcp_listen_with_backlog(pcb, 1);
  if(!state->server->server_pcb){
    printf("Failed to listen\n");
    if(pcb){
      tcp_close(pcb);
    }
    gpio_put(LED, 1);
    return false;
  }

  // The current listening block and the parameter to pass to all callback functions
  tcp_arg(state->server->server_pcb, state);

  // Specifies the function to be called whenever a listening connection has been connected to by a host
  tcp_accept(state->server->server_pcb, tcp_server_accept);

  return true;
}

/**
 * Runs all the functions to set up and start the TCP server
 */
STATE_T* init_server(){
  STATE_T *state = calloc(1,sizeof(STATE_T));

  if(!state){
    printf("Failed to allocate the state\n");
    gpio_put(LED, 1);
    return NULL;
  }

  if(!tcp_server_open(state)){
    tcp_server_result(state, -1);
    gpio_put(LED, 1);
  }

  return state;
}

void listen(STATE_T *state){
  printf("Listening for connections\n");
  // Loop until a connection
  while(true){
    gpio_put(LED, 1);
    busy_wait_ms(1000);
    gpio_put(LED, 0);
    busy_wait_ms(1000);
  }
}

int main(){
  stdio_init_all();

  gpio_init(LED);
  gpio_set_dir(LED, GPIO_OUT);

  if(cyw43_arch_init_with_country(CYW43_COUNTRY_UK)){
    printf("Failed to initialise\n");
    return 1;
  }

  printf("Initialised\n");

  cyw43_arch_enable_sta_mode();

  if(cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000)){
    printf("Failed to connect\n");
    gpio_put(LED, 1);
    return 1;
  }

  printf("Connected\n");

  STATE_T *state = init_server();
  
  if(!state){
    printf("Shutting down\n");
    return 1;
  }
  
  listen(state);

  free(state);
  cyw43_arch_deinit();
  return 0;
}
