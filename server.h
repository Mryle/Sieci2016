#ifndef __SERVER_H
#define __SERVER_H

#define _DEFAULT_SOURCE
#include <assert.h>
#include <unistd.h>
#include <endian.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <signal.h>
#include <poll.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <getopt.h>

#ifndef NDEBUG
        const bool debug = false;
        const bool debug_player = false;
        const bool debug_tick = false;  //messy
        const bool debug_main = false;
        const bool debug_messages = false;      //messy
        const bool debug_events = false;
        const bool debug_send_raw = false;       //Messy as well
#else
        const bool debug = false;
        const bool debug_player = false;
        const bool debug_tick = false;
        const bool debug_main = false;
        const bool debug_messages = false;
        const bool debug_events = false;
        const bool debug_send_raw = false;
#endif

#define PI 3.14159265
#define MAX_PLAYERS 42
#define PLAYER_SPECTATE 42
#define MAP_EMPTY MAX_PLAYERS + 1
#define PLAYER_NAME_LENGTH 64
#define SERVER_DGRAM_MAX 512
#define SERVER_POLL_TIME 100
#define COMPARE_IGNORE -2
#define COMPARE_DIFFERENT -1
#define COMPARE_NEW -3

#include "err.h"
#include "events.h"
#include "numbers.h"
#include "util.h"

typedef char* c_string;

// Używany do połączenia Serwera z klientem
struct sit_client_information {
        //Informacje dotyczące stanu gracza
        bool taken;
        uint1_b playerNum;
        c_string name;
        int nameLength;
        //Informacje dotyczące stanu w grze
        int1_b turn_direction;
        double posX;
        double posY;
        int direction;
        //Informacje dotyczące połączenia
        struct sockaddr_in6 address;
        uint8_b session_id;
        uint4_b event_no; //Eventy do przesłania
        uint64_t lastTime; //Czas od ostatniego przesłania wiadomości
        bool connected;
        bool ready;
        void* eventQueue; //nieprzesłane eventy
};

struct sit_server_state {
        uint4_b width;
        uint4_b height;
        uint64_t rps;
        int turningSpeed;
        uint64_t seed;
        uint4_b event_no;
        
        uint4_b game_id;
        bool gameInPlay;
        char *map;
        struct sit_client_information players[MAX_PLAYERS];
        uint1_b nextPlayerToSend;
};

//Walidacja parametrów
bool validateScreenSize(int size);
bool validateRoundPerSec(int rps);
bool validateTurningSpeed(int turnSpeed);


//game
void gameInit();
void gameStart();
        void gameClearPlayers();
        void gameSortPlayers();
        void gameClearMap();
void gameOver();
uint64_t gameRandom();
uint64_t gameGetWidth();
uint64_t gameGetHeight();
char gameGetPixel(int x, int y);
void gamePutPixel(int x, int y, char pixel);
uint4_b gameGetEventNo();
void gameSendRoutine();
bool gameShouldStart();
bool gameShouldEnd();

//Poll
bool pollLoop();
void pollArrayInit();
void pollArrayClear();

//Connection
bool initialize_connection();
bool readFromClient();

//send
bool sendToPlayer(struct sit_client_information* players, int playerNum);
void sendMessagePrepare(uint1_b playerNum);
void sendEventPrepare();
void sendEventPixel(uint1_b player_number, uint4_b x, uint4_b y);
void sendEventNewGame();
void sendEventPlayerEliminated(uint1_b player_number);
void sendEventGameOver();
void sendEndFooter(uint4_b len);
void sendEventComplete(int eventSize);

//Players
void playerInit(struct sit_client_information* players, int playerNum);
int playerFind(struct sit_client_information* players, char *playerName, int playerNameLength, struct sit_server_message_st *mess);
int playerCompareReadWith(struct sit_client_information* players, int playerNum, struct sit_server_message_st *mess);
int playerAdd(struct sit_client_information* players, char *playerName, int playerNameLength, struct sit_server_message_st *mess);
bool playerAddNum(struct sit_client_information* players, int playerNum, char *playerName, int playerNameLength, struct sit_server_message_st *mess);
void playerDelete(struct sit_client_information* players, int playerNum);
bool playerNameValidate(struct sit_client_information* players, int playerNum, char *playerName, int playerNameLength);
void playerCheckConnection(struct sit_client_information* players, int playerNum);
bool playerSpectate(struct sit_client_information* players, int playerNum);
bool playerExists(struct sit_client_information* players, int playerNum);
bool playerSpawn(struct sit_client_information* players, int playerNum);
void playerStep(struct sit_client_information* players, int playerNum);
void playerKill(struct sit_client_information* players, int playerNum);
bool playerAlive(struct sit_client_information* players, int playerNum);
bool playerConnected(struct sit_client_information* players, int playerNum);
void playerSendPixel(struct sit_client_information* players, int playerNum);
void playerSendRoutine(struct sit_client_information* players, int playerNum);
#endif //__CLIENT_H
