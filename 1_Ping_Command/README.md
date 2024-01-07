
# Ping command using Transmission Control Protocol (TCP)

C program implementing ping command using Transmission Control Protocol (TCP).

This project is part of a Computing Lab (CS69201) assignment of IIT Kharagpur. (PDF of the assignment can be found in the repository)

#### Programming Languages Used
* C

#### Libraries Used
* stdio
* stdlib
* string
* unistd
* arpa/inet
* sys/time
* pthread
(Some libraries mentioned above comes as part of C)

### Role of pingClient.c
C program for TCP client, used to ping the TCP server as per input.

### Role of pingServer.c
C program for TCP server, used to listen to different client for pings. It also logs each incoming ping request details (source IP address, port, and
RTT) in file.

## Running it locally on your machine
1. Clone this repository, and cd to the project root.
2. Run python3 handler.py

## Purpose

The project was used to study the speedup achieved when three processes of scraper.py are used instead of one.