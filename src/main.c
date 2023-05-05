#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "hardware/timer.h"
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

char temp_unit = 'C';

static err_t tcp_server_close(void *arg) {
  TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
  err_t err = ERR_OK;
  if (state->client_pcb != NULL) {
    tcp_arg(state->client_pcb, NULL);
    tcp_poll(state->client_pcb, NULL, 0);
    tcp_sent(state->client_pcb, NULL);
    tcp_recv(state->client_pcb, NULL);
    tcp_err(state->client_pcb, NULL);
    err = tcp_close(state->client_pcb);
    if (err != ERR_OK) {
      printf("Close failed %d, calling abort\n", err);
      tcp_abort(state->client_pcb);
      err = ERR_ABRT;
    }
    state->client_pcb = NULL;
  }
  if (state->server_pcb) {
    tcp_arg(state->server_pcb, NULL);
    tcp_close(state->server_pcb);
    state->server_pcb = NULL;
  }
  return err;
}

/**
 * Called when the client acknowledges the sent data
 *
 * @param arg	    the state struct
 * @param tpcb	  the connection PCB for which data has been acknowledged
 * @param len	    the amount of bytes acknowledged
 */
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
  state->sent_len += len;
  if (state->sent_len>=BUF_SIZE) {
    printf("Waiting for client");
    state->recv_len=0;
  }
  return ERR_OK;
}

/**
 * Function to send the data to the client
 *
 * @param arg	        the state struct
 * @param tcp_pcb     the client PCB
 * @param data	      the data to send
 */
err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb, uint8_t data[])
{
  TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
  memset(state->buffer_sent,0,BUF_SIZE);
  memcpy(state->buffer_sent,data,strlen(data));
  state->sent_len =0;
  printf("writing to client\n");
  err_t err = tcp_write(tpcb,state->buffer_sent,BUF_SIZE,TCP_WRITE_FLAG_COPY);
  if (err!=ERR_OK) {
    printf("Write failed %d\n", err);
    return ERR_VAL;
  } else {
    tcp_output(tpcb); //flushes only if there's no error
  }
  
  return ERR_OK;
}

/**
 * Parse a message and call the relevant functions
 * 
 * @param state     the state struct
 * @param tpcb      the client pcb
 * @param msg       the message received
 */
static void parseMsg(TCP_SERVER_T *state, struct tcp_pcb *tpcb, char* msg){
  printf("Received message: %sEOT\n", msg); //interesting behaviour: if you write a long message to the buffer, it persists until the next message so abcdef followed by TEMP, would read TEMP\nf
  char res[BUF_SIZE];
  memset(res, 0, BUF_SIZE);
  if(strncmp(msg, cmd_time, 4) == 0){
    uint64_t currentTime = read_time();
    snprintf(res, BUF_SIZE, "System time is now %lld\n", currentTime);
  } else if(strncmp(msg, cmd_temp, 4) == 0){
    float temp = read_temperature(temp_unit);
    snprintf(res, BUF_SIZE, "Temperature is %f %s\n", temp, &temp_unit);
  } else if(strncmp(msg, cmd_set, 3) == 0){
    char* split = strtok(msg, " ");
    split = strtok(NULL, " ");
    if(strncmp(split, "C", 1) == 0){
      temp_unit = 'C';
    } else if(strncmp(split, "F", 1) == 0){
      temp_unit = 'F';
    } else {
      tcp_server_send_data(state, tpcb, msg_invalid_unit);
      return;
    }
    snprintf(res, BUF_SIZE, "Temperature unit is now %s\n", &(temp_unit));

  } else {
    memcpy(res, msg_invalid_command, strlen(msg_invalid_command));
  }

  tcp_server_send_data((void*) state, tpcb, res);

}


/**
 * The method called when data is received at the host and send the message to the parsing function
 *
 * @param arg	  the state struct
 * @param tpcb	  the connection PCB which received data
 * @param p	  the received data
 * @param err	  an error code if there has been an error receiving
 */
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
  if (!p) {
    printf("no received");
    return ERR_VAL;
  }

  if (p->tot_len) {
    printf("Received %d bytes\n", p->tot_len);
    printf("Received string: %sEOT\n", ((char*) p->payload)); //casting to char pointer means we can print as a string as it will read until 0.
    const u16_t bufferLeft = BUF_SIZE - state->recv_len;  //u16_t is unsigned 16 bit integer and typdef for uint16_t
    state->recv_len += pbuf_copy_partial(p,state->buffer_recv + state->recv_len, p->tot_len<=bufferLeft ? p->tot_len:bufferLeft,0);
    printf("Amount actually tried to read %d\n",p->tot_len<=bufferLeft ? p->tot_len:bufferLeft); //copy partial may have copied less.

    tcp_recved(tpcb,p->len); //called when data is processed. this allows for advertising of larger window. why tot_len and not recv_len? due to the buffer size, if total length is not read, the extra is lost
    //p->len is not used as that may be the pbuf storing just the first letter. we want the whole chain. 

    
  }
  pbuf_free(p);

  parseMsg(state,tpcb,(char*) state->buffer_recv); //may cause bug

  return ERR_OK;
}

/**
 * The function for tcp poll callbacks
 *
 * @param arg	  the state struct
 * @param tpcb	  the relevant TCP Protcol Control Block
 */ 
static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
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
    printf("TCP error %d\n", err);
  }
}

/**
 * The function called when a client connects; set all callback functions
 *
 * @param arg		the state struct
 * @param client_pcb	the new connection PCB
 * @param err		the error code (if present)
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {

  TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
  
  printf("client connected");
  state->client_pcb = client_pcb;  //todo:more error checking
  tcp_arg(client_pcb,state);
  tcp_recv(client_pcb, tcp_server_recv);
  tcp_sent(client_pcb, tcp_server_sent);
  tcp_err(client_pcb, tcp_server_err);
  
  return tcp_server_send_data(arg, state->client_pcb, msg_welcome);
}

/**
 * Start the TCP server
 * Allocates a new state struct, set an IP, binds to a port, set the PCB to listen, set the PCB arguments and set the accept callback function
 */
TCP_SERVER_T* init_server(){
  TCP_SERVER_T *state = calloc(1,sizeof(TCP_SERVER_T));

  if(!state){
    printf("Failed to allocate the state\n");
    gpio_put(LED, 1);
    return NULL;
  }


  printf("Starting TCP server at %s:%u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);

  // Creates s new TCP protocol control block but doesn't place it on any of the TCP PCB lists. The PCB is not put on any list until it is bound
  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if(!pcb) {
    printf("Failed to create PCB\n");
    gpio_put(LED, 1);
    return NULL;
  }

  // Binds the connection to a local port number
  err_t err = tcp_bind(pcb, NULL, TCP_PORT);
  
  if(err){
    printf("Failed to bind to port\n");
    gpio_put(LED, 1);
    return NULL;
  }

  // Set the state to LISTEN. Returns a more memory efficient PCB
  state->server_pcb = tcp_listen_with_backlog(pcb, 1);
  if(!state->server_pcb){
    printf("Failed to listen\n");
    if(pcb){
      tcp_close(pcb);
    }
    gpio_put(LED, 1);
    return NULL;
  }

  // The current listening block and the parameter to pass to all callback functions
  tcp_arg(state->server_pcb, state);
  // Specifies the function to be called whenever a listening connection has been connected to by a host
  tcp_accept(state->server_pcb, tcp_server_accept);


  return state;
}



void listen(TCP_SERVER_T *state){
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

  // Connect to WiFi
  
  if(cyw43_arch_init_with_country(CYW43_COUNTRY_UK)){
    printf("Failed to initialise\n");
    return 1;
  }

  cyw43_arch_enable_sta_mode();

  if(cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000)){
    printf("Failed to connect\n");
    gpio_put(LED, 1);
    return 1;
  }

  printf("Connected to WiFi\n");

  TCP_SERVER_T *state = init_server();
  
  if(!state){
    printf("Shutting down\n");
    return 1;
  }
  
  listen(state);

  tcp_server_close(state);
  free(state);
  cyw43_arch_deinit();
  return 0;
}
