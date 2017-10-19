#ifndef __CLIENT_H
#define __CLIENT_H

#define _DEFAULT_SOURCE
#include <assert.h>
#include <unistd.h>
#include <endian.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <signal.h>
#include <poll.h>


#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef NDEBUG
        const bool debug = false;
        const bool debug_keys = false;
        const bool debug_tick = false;
        const bool debug_message_gui = false;
        const bool debug_message_server = false;
        const bool debug_message_ignored = false;
#else
        const bool debug = false;
        const bool debug_keys = false;
        const bool debug_tick = false;
        const bool debug_message_gui = false;
        const bool debug_message_gui = false;
        const bool debug_message_ignored = false;
#endif

#define POLL_CLIENT_SLOT 0
#define POLL_SERVER_SLOT 1
#define PLAYER_NAME_LENGTH 64
#define CLIENT_KEEP_ALIVE_TIME 20000
#define KEYS_LEFT 0
#define KEYS_RIGHT 1
#define CLIENT_BUFFER_SIZE 256
#define LEFT_KEY_DOWN "LEFT_KEY_DOWN\n"
#define LEFT_KEY_UP "LEFT_KEY_UP\n"
#define RIGHT_KEY_DOWN "RIGHT_KEY_DOWN\n"
#define RIGHT_KEY_UP "RIGHT_KEY_UP\n"
#define SERVER_DGRAM_MAX 512

#include "err.h"
#include "events.h"
#include "numbers.h"
#include "util.h"

typedef char* c_string;

// Używany do połączenia z serwerem i klientem
struct sit_client_connection {
        int sock;
        bool canSend;
};

struct sit_server_connection {
        int sock;
        bool canSend;
};

struct sit_client_state {
        struct sit_client_connection client;
        struct sit_server_connection server;
        bool clickedKeys[2];
        uint8_b sessionId;
        uint4_b nextEventNo;
        uint4_b gameId;
        c_string players[43];
        char playerList[512];
        int playersPlaying;
};

bool validateName(char* name);

//state
void stateInit();

//Connection function declarations
bool isIPv6(char *address);
bool connect_ipv4TCP(struct sit_client_connection *conn, char *node, char *service);
bool connect_ipv6TCP(struct sit_client_connection *conn, char *node, char *service);
bool connect_ipv4UDP(struct sit_server_connection *conn, char *host, char* port);
bool connect_ipv6UDP(struct sit_server_connection *conn, char *host, char* port);

//Poll function declarations
bool shouldSendMessageServer(struct timeval *timeLast, struct timeval *timeCurrent);
void pollArrayInit(struct pollfd *pollA, struct sit_client_connection *client,
                struct sit_server_connection *server);
void pollArrayClear(struct pollfd *pollA);
bool pollLoop();

//Reading Client function declarations
bool readFromClient();
void parseClientMessage(char *buffer, int *buffer_s, bool *clickedKeys);

//Sending Client function declarations
bool sendClientMessage();

//Reading server function declarations
bool readFromServer();
bool validateServerMessage(char **buffer, int *messageSize);
bool validateServerEventMessages(char **buffer, int *messageSize, uint32_t game_id);
bool parseGameOverMessage();
bool parsePixelMessage();
bool parsePlayerEliminatedMessage();
bool parseNewGameMessage();
bool parsePlayerList(size_t size);
bool parseServerMessage();

//Sending server function declarations
void prepareServerMessage();
bool sendServerMessage();

//Utility functions
int compareCharacters(char *buffer, const char *sample);

#endif //__CLIENT_H
