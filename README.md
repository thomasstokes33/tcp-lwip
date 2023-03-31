# TCP Server for System Information

A challenge for [COMP2215](https://www.southampton.ac.uk/courses/modules/comp2215)

## What is the challenge?

The aim of this task is to learn the [lwIP](https://www.nongnu.org/lwip/2_1_x/index.html) stack. This is designed for use on embedded devices and works on the Pico W. Some functions for getting system data (current timestamp and temperature (in °C or F)) are implemented in the `utils.h` file.

The challenge is to implement a TCP server on the Pico W which listens for commands and returns the relevant data. All commands and messages are outlined in the `defs.h` file; the current commands are `TEMP`, `TIME` and `SET C/F`.

Netcat is very useful to send commands to the server. By default, debug messages are sent over UART and a status LED is used.

## How does the lwIP stack work? 

All data for the connections are stored in the `TCP_SERVER_H` struct. Connections are controlled with a PCB (Protocol Control Block) which is why the `TCP_SERVER_H` struct is passed to all the callback functions. Comments outline the purpose of each function.
