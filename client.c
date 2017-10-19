#include "client.h"

char defaultServerPort[] = "12345";
char defaultClientPort[] = "12346";
char defaultClientHost[] = "localhost";

c_string buff_host;
c_string buff_port;
struct pollfd pollA[2];
bool finish = false;
bool error = false;
char clientReadBuffer[CLIENT_BUFFER_SIZE+1];
int clientReadBufferS;
char clientSendBuffer[CLIENT_BUFFER_SIZE+1];
int clientSendBufferS;
char serverSendBuffer[80];
char serverReadBuffer[SERVER_DGRAM_MAX];
int serverSendBufferSize = sizeof(struct sit_server_message_st);
int ret;

struct sit_client_state state;

int main(int argc, char *argv[]) {
        stateInit();
        //Parsowanie wejścia
        if (argc < 3 || argc > 4) {
                nfatal("%s player_name game_server_host[:port] [ui_server_host[:port]]",
                                argv[0]);
                return 1;
        }
        
        if (debug) {
                fprintf(stderr, "Gracz : %s\n", argv[1]);
                fprintf(stderr, "Server: %s\n", argv[2]);
                if (argc == 4) fprintf(stderr, "UI    : %s\n", argv[3]);
        }
        
        if (strlen(argv[1]) > PLAYER_NAME_LENGTH) {
                nfatal("Za długa nazwa gracza\n");
                return 1;
        }
        
        if (!validateName(argv[1])) {
                nfatal("Niepoprawna nazwa gracza\n");
                return 1;
        }
        
        memset(serverSendBuffer,0,80);
        memcpy(serverSendBuffer + sizeof(struct sit_server_message_st),
                argv[1], strlen(argv[1]));
        serverSendBufferSize += strlen(argv[1]);
        
        //Tworzenie wejścia
        
        if (isIPv6(argv[2])) {
                if (!connect_ipv6UDP(&state.server, argv[2], NULL)) {
                        return 1;
                }
        } else {
                buff_host = strtok(argv[2], ":");
                buff_port = strtok(NULL, ":");
                if (!connect_ipv4UDP(&state.server, buff_host, buff_port)) {
                        return 1;
                }
                //if (buff_host != NULL && buff_host != argv[2]) free(buff_host);
                //if (buff_port != NULL) free(buff_port);
        }
        if (argc == 4) {
                if (isIPv6(argv[3])) {
                        if (!connect_ipv6TCP(&state.client, argv[3], NULL)) {
                                return 1;
                        }
                } else {
                        buff_host = strtok(argv[3], ":");
                        buff_port = strtok(NULL, ":");
                        if (!connect_ipv4TCP(&state.client, buff_host, buff_port)) {
                                return 1;
                        }
                        //if (buff_host != NULL && buff_host != argv[3]) free(buff_host);
                        //if (buff_port != NULL) free(buff_port);
                }
        } else {
                if (!connect_ipv4TCP(&state.client, NULL, NULL)) {
                        return 1;
                }
        }
        
        pollArrayInit(pollA, &state.client, &state.server);
        
        timeInit(CLIENT_KEEP_ALIVE_TIME);
        state.sessionId = timeGetSeconds()*(uint64_t)1000000 + timeGetMicroSeconds();
        while(!finish) {
                finish |= !pollLoop();
        }
        if (error)
                return 1;
        else
                return 0;
}

bool validateName(char* name) {
        int a = 0, len = strlen(name);
        for(a = 0; a < len; a++) {
                if (name[a] < 33 || name[a] > 126) return false;
        }
        return true;
}

void stateInit() {
        state.server.canSend = true;
        state.client.canSend = true;
}

//TODO: Nie zapychać, jesli nie możemy wysłać.
void timeDoHeartBeat() {
        if (debug_tick) {
                printf("HeartBeat\n");
        }
        if (state.server.canSend) {
                prepareServerMessage();
                finish |= !sendServerMessage();
        }
}

bool pollLoop() {
        timeStep();
        uint64_t heartBeats = timeIsHeartBeat();
        while(heartBeats > 0) {
                timeDoHeartBeat();
                heartBeats--;
        }
        pollArrayClear(pollA);
        ret = poll(pollA, 2, CLIENT_KEEP_ALIVE_TIME);
        if (ret < 0) {
                syserr("Błąd przy wykonaniu poll\n");
                return false;
        } else if (ret > 0) {
                if (pollA[POLL_CLIENT_SLOT].revents & POLLOUT) state.client.canSend = true;
                if (pollA[POLL_SERVER_SLOT].revents & POLLOUT) state.server.canSend = true;
                if (pollA[POLL_CLIENT_SLOT].revents & (POLLIN | POLLERR)) {
                        if (!readFromClient()) {
                                return false;
                        }
                }
                if (pollA[POLL_SERVER_SLOT].revents & (POLLIN | POLLERR)) {
                        //Odbierz wiadomość z servera
                        if (!readFromServer()) {
                                return false;
                        }
                }
        }
        return true;
}

void pollArrayInit(struct pollfd *pollA, struct sit_client_connection *client,
                struct sit_server_connection *server) {
        pollA[POLL_CLIENT_SLOT].fd = client->sock;
        pollA[POLL_CLIENT_SLOT].events = POLLIN | POLLOUT;
        pollA[POLL_SERVER_SLOT].fd = server->sock;
        pollA[POLL_SERVER_SLOT].events = POLLIN | POLLOUT;
}

void pollArrayClear(struct pollfd *pollA) {
        pollA[POLL_CLIENT_SLOT].revents = 0;
        pollA[POLL_SERVER_SLOT].revents = 0;
}

bool sendClientMessage() {
        int sendLen = 0, alSend = 0;
        int len = clientSendBufferS;
        while(sendLen + alSend < len) {
                sendLen = send(pollA[POLL_CLIENT_SLOT].fd, clientSendBuffer, len, 0);
                if (sendLen < 0) {
                        syserr("Błąd przy wysłaniu danych do klienta\n");
                        error=true;
                        return false;
                } else if (sendLen == 0) {
                        //syserr("Błąd przy wysłaniu danych do klienta\n");
                        nfatal("Klient zakończył działanie\n");
                        return false;
                } else if (sendLen + alSend < len) {
                        //syserr("Wysłanie niepełnych danych do klienta, ponawianie\n");
                        nfatal("Wysłanie niepełnych danych do klienta, ponawianie\n");
                }
                alSend += sendLen;
        }
        return true;
}

bool readFromClient() {
        int rval = read(pollA[POLL_CLIENT_SLOT].fd,
                        clientReadBuffer + clientReadBufferS,
                        CLIENT_BUFFER_SIZE - clientReadBufferS);
        if (rval == 0) {
                nfatal("Klient zakończył działanie\n");
                return false;
        } else if (rval < 0) {
                syserr("Nieudane odebranie danych od klienta");
                error = true;
                return false;
        } else {
                clientReadBufferS += rval;
        }
        if (debug_message_gui) printf("GUI: %s\n", clientReadBuffer);
        parseClientMessage(clientReadBuffer, &clientReadBufferS, state.clickedKeys);
        return true;
}

void parseClientMessage(char *buffer, int *buffer_s, bool *clickedKeys) {
        char lu[] = LEFT_KEY_UP;
        int llu = strlen(lu);
        char ld[] = LEFT_KEY_DOWN;
        int lld = strlen(ld);
        char ru[] = RIGHT_KEY_UP;
        int lru = strlen(ru);
        char rd[] = RIGHT_KEY_DOWN;
        int lrd = strlen(rd);
        int ind=0;
        bool corr = true;
        while(corr && ind < (*buffer_s)) {
                corr = false;
                if (ind < llu && buffer[ind] == lu[ind]) {
                        corr = true;
                        if (ind + 1 == llu) {
                                if (debug_keys) printf("Clicked: Left Key Up\n");
                                memmove(buffer, buffer+ind, CLIENT_BUFFER_SIZE - ind);
                                (*buffer_s) -= ind;
                                clickedKeys[KEYS_LEFT] = false;
                                continue;
                        }
                }
                if (ind < lld && buffer[ind] == ld[ind]) {
                        corr = true;
                        if (ind + 1 == lld) {
                                if (debug_keys) printf("Clicked: Left Key Down\n");
                                memmove(buffer, buffer+ind, CLIENT_BUFFER_SIZE - ind);
                                (*buffer_s) -= ind;
                                clickedKeys[KEYS_LEFT] = true;
                                continue;
                        }
                }
                if (ind < lru && buffer[ind] == ru[ind]) {
                        corr = true;
                        if (ind + 1 == lru) {
                                if (debug_keys) printf("Clicked: Right Key Up\n");
                                memmove(buffer, buffer+ind, CLIENT_BUFFER_SIZE - ind);
                                (*buffer_s) -= ind;
                                clickedKeys[KEYS_RIGHT] = false;
                                continue;
                        }
                }
                if (ind < lrd && buffer[ind] == rd[ind]) {
                        corr = true;
                        if (ind + 1 == lrd) {
                                if (debug_keys) printf("Clicked: Right Key Down\n");
                                memmove(buffer, buffer+ind, CLIENT_BUFFER_SIZE - ind);
                                (*buffer_s) -= ind;
                                clickedKeys[KEYS_RIGHT] = true;
                                continue;
                        }
                }
                if (!corr) {
                        memmove(buffer, buffer+1, CLIENT_BUFFER_SIZE - 1);
                        (*buffer_s) -= 1;
                        corr = true;
                }
                ind++;
        }
}

bool readFromServer() {
        int rval = read(pollA[POLL_SERVER_SLOT].fd,
                        serverReadBuffer, SERVER_DGRAM_MAX);
        if (rval <= 0) {
                syserr("Błąd przy odebraniu danych z serwera\n");
                error = true;
                return false;
        }
        char *ptr = serverReadBuffer;
        return validateServerMessage(&ptr,&rval);
}

bool validateServerMessage(char **buffer, int *messageSize) {
        if ((*messageSize) < EVENT_GAME_MESSAGE_HEADER_SIZE) {
                nfatal("Wiadomość od serwera nie mieści nagłówka\n");
                return false;
        }
        struct sit_event_game_message* mess =  (struct sit_event_game_message*) *buffer;
        //Jakieś sprawdzanie game_id?
        mess->game_id = be32toh(mess->game_id);
        //if (debug_message_server) {
        //        printf("Received message of game_id %d\n", mess->game_id);
        //}
        (*buffer) += EVENT_GAME_MESSAGE_HEADER_SIZE;
        (*messageSize) -= EVENT_GAME_MESSAGE_HEADER_SIZE;
        return validateServerEventMessages(buffer, messageSize, mess->game_id);
}

bool validateServerEventMessages(char **buffer, int *messageSize, uint32_t game_id) {
        while (*messageSize > 0) {
                if ((*messageSize) < EVENT_HEADER_LEN_SIZE) {
                        nfatal("Wiadomość nie mieści długości eventu\n");
                        return false;
                }
                struct sit_event_header *message = (struct sit_event_header*) *buffer;
                uint4_b len = be32toh(message->len);
                if ((*messageSize) < len + EVENT_HEADER_LEN_SIZE + EVENT_FOOTER_SIZE) {
                        nfatal("Zbyt mała długość eventu od serwera %d < %d\n",
                                        (*messageSize),
                                        len + EVENT_HEADER_LEN_SIZE + EVENT_FOOTER_SIZE);
                        return false;
                }
                struct sit_event_footer *footer= (struct sit_event_footer*) (*buffer + EVENT_HEADER_LEN_SIZE + len);
                footer->crc = be32toh(footer->crc);
                uint4_b crc = crcCompute((char*)*buffer, len + EVENT_HEADER_LEN_SIZE);
                if (crc != footer->crc) {
                        nfatal("Niezgodna suma CRC %u != %u\n",
                               footer->crc, crc);
                        return true;
                }
                message->len = len;
                message->event_no = be32toh(message->event_no);
                
                if (message->event_no == 0 && message->event_type == EVENT_NEW_GAME) {
                        state.gameId = game_id;
                        state.nextEventNo = 0;
                }
                
                if (game_id != state.gameId) {
                        nfatal("Otrzymano wiadomość o innym game_id i nie jest to NEW_GAME\n");
                        return true;
                }
                
                if (message->event_no == state.nextEventNo) {
                        if (!parseServerMessage(buffer, messageSize)) {
                                return false;
                        }
                        state.nextEventNo++;
                } else if (message->event_no < state.nextEventNo) {
                        if (debug_message_ignored)
                                nfatal("Otrzymano już przetworzony event. Ignorujemy (%d < %d)\n",
                                        message->event_no,state.nextEventNo);
                } else {
                        if (debug_message_ignored)
                                nfatal("Otrzymano event o za dużym numerze. Ignorujemy (%d > %d)\n",
                                        message->event_no,state.nextEventNo);
                }
                (*buffer) += message->len + EVENT_HEADER_LEN_SIZE + EVENT_FOOTER_SIZE;
                (*messageSize) -= message->len + EVENT_HEADER_LEN_SIZE + EVENT_FOOTER_SIZE;
        }
        return true;
}

bool parseGameOverMessage(char **buffer, int *messageSize) { //Cleaning players List
        //fprintf(clientSendBuffer, "GAME_OVER\n");
        /*int a = 0;
        for(a = 0; a < state.playersPlaying; a++) {
                state.players[a] = ;
        }
        state.playersPlaying = 0;*/
        return true;
}

bool parsePixelMessage(char **buffer, int *messageSize) {
        //struct sit_event_header *message = (struct sit_event_header*)serverReadBuffer;
        struct sit_event_pixel *pixel = (struct sit_event_pixel*) (*buffer + EVENT_HEADER_SIZE);
        pixel->x = be32toh(pixel->x);
        pixel->y = be32toh(pixel->y);
        if (pixel->player_number > state.playersPlaying) {
                nfatal("Gracz z komunikatu nie istnieje\n");
                error = true;
                return false;
        }
        sprintf(clientSendBuffer, "PIXEL %u %u %s\n", pixel->x, pixel->y,
                        state.players[pixel->player_number]);
        if (debug_message_server)
                printf("Received: PIXEL %d %d %s\n", pixel->x, pixel->y,
                                state.players[pixel->player_number]);
        clientSendBufferS = strlen(clientSendBuffer);
        return sendClientMessage();
}

bool parsePlayerEliminatedMessage(char **buffer, int *messageSize) {
        //struct sit_event_header *message = (struct sit_event_header*)serverReadBuffer;
        struct sit_event_player_eliminated *dead = (struct sit_event_player_eliminated*) (*buffer + EVENT_HEADER_SIZE);
        if (dead->player_number > state.playersPlaying) {
                nfatal("Gracz z komunikatu nie istnieje\n");
                error = true;
                return false;
        }
        sprintf(clientSendBuffer, "PLAYER_ELIMINATED %s\n", state.players[dead->player_number]);
        if (debug_message_server)
                printf("Received: PLAYER_ELIMINATED %s\n", state.players[dead->player_number]);\
        clientSendBufferS = strlen(clientSendBuffer);
        return sendClientMessage();
}

bool parseNewGameMessage(char **buffer, int *messageSize) {
        struct sit_event_header *message = (struct sit_event_header*) *buffer;
        struct sit_event_new_game *game = (struct sit_event_new_game*) (*buffer + EVENT_HEADER_SIZE);
        char *playerList = *buffer + EVENT_HEADER_SIZE + EVENT_NEW_GAME_SIZE;
        size_t playerListLen = message->len - EVENT_HEADER_EVENT_SIZE - EVENT_NEW_GAME_SIZE;
        //nfatal("Player list received %s\n", playerList);
        game->maxx = be32toh(game->maxx);
        game->maxy = be32toh(game->maxy);
        
        //Preparing players
        memcpy(state.playerList, playerList, playerListLen);
        if (!parsePlayerList(playerListLen)) {
                nfatal("Invalid player list - Not ended with 0\n",
                                *(playerList + playerListLen));
                error = true;
                return false;
        }
        
        //Preparing message
        sprintf(clientSendBuffer, "NEW_GAME %d %d", game->maxx, game->maxy);
        clientSendBufferS = strlen(clientSendBuffer);
        int count = 0;
        for(count = 0; count < state.playersPlaying; count++) {
                clientSendBuffer[clientSendBufferS++] = ' ';
                memcpy(clientSendBuffer + clientSendBufferS,
                        state.players[count], strlen(state.players[count]));
                clientSendBufferS += strlen(state.players[count]);
        }
        clientSendBuffer[clientSendBufferS++] = '\n';
        if (debug_message_server) {
                printf("Received: NEW_GAME %d %d (%d)\n", game->maxx, game->maxy,
                                state.playersPlaying);
                int a = 0;
                for(a = 0; a < state.playersPlaying; a++) {
                        printf("Player %d: %s\n", a, state.players[a]);
                }
        }
        return sendClientMessage();
}

bool parsePlayerList(size_t size) {
        int ind = 0;
        state.playersPlaying = 0;
        char *ptr = state.playerList;
        //utilPrintBytes(ptr, size);
        state.players[state.playersPlaying++] = ptr;
        bool next = false;
        for(ind = 1; ind < size; ind++) {
                if (next) {
                        next = false;
                        state.players[state.playersPlaying++] = ptr + ind;
                }
                if (ptr[ind]==0) next = true;
        }
        return next;
}

bool parseServerMessage(char **buffer, int *messageSize) {
        struct sit_event_header *message = (struct sit_event_header*) *buffer;
        memset(clientSendBuffer, 0, CLIENT_BUFFER_SIZE);
        switch(message->event_type) {
                case EVENT_NEW_GAME:
                        if (debug_message_server)
                                printf("Parsing New Game message %d\n",
                                                message->event_no);
                        return parseNewGameMessage(buffer, messageSize);
                case EVENT_PIXEL:
                        if (debug_message_server)
                                printf("Parsing Pixel message %d\n",
                                                message->event_no);
                        return parsePixelMessage(buffer, messageSize);
                case EVENT_PLAYER_ELIMINATED:
                        if (debug_message_server)
                                printf("Parsing Player Eliminated message %d\n",
                                                message->event_no);
                        return parsePlayerEliminatedMessage(buffer, messageSize);
                case EVENT_GAME_OVER:
                        if (debug_message_server)
                                printf("Parsing Game Over message %d\n",
                                                message->event_no);
                        return parseGameOverMessage(buffer, messageSize);
                default:
                        if (debug_message_server)
                                nfatal("Nieznany typ eventu. Ignorujemy\n");
                        return true;
        }
        return true;
}

void prepareServerMessage() {
        struct sit_server_message_st *header = (struct sit_server_message_st*) serverSendBuffer;
        header->session_id = htobe64(state.sessionId);
        header->next_expected_event_no = htobe32(state.nextEventNo);
        header->turn_direction = 0;
        if (state.clickedKeys[KEYS_LEFT] && !state.clickedKeys[KEYS_RIGHT])
                header->turn_direction = -1;
        if (!state.clickedKeys[KEYS_LEFT] && state.clickedKeys[KEYS_RIGHT])
                header->turn_direction = 1;
}

bool sendServerMessage() {
        int sendLen;
        //size_t addrLen = sizeof(server.conn.addr);
        //sendLen = sendto(pollA[POLL_SERVER_SLOT].fd, serverSendBuffer, serverSendBufferSize, sflags,(struct sockaddr *) &server.conn.addr, addrLen);
        sendLen = send(pollA[POLL_SERVER_SLOT].fd,
                        serverSendBuffer, serverSendBufferSize, 0);
        state.server.canSend = false;
        if (sendLen != serverSendBufferSize) {
                syserr("Nieudane przesłanie danych do serwera\n");
                error = true;
                return false;
        }
        return true;
}

int compareCharacters(char *buffer, const char *sample) {
        int num = 0;
        while(buffer[num] == sample[num]) num++;
        return num;
}

bool isIPv6(char *address) {
        int length = strlen(address);
        int a = 0, count = 0;
        for(a = 0; a < length; a++) {
                if (address[a] == ':') count++;
        }
        return count > 1;
}

bool connect_ipv4TCP(struct sit_client_connection *conn, char *host, char *port) {
        struct addrinfo addrHints;
        struct addrinfo *addrResult;
        int err;
        if (conn == NULL) return false;
        if (host == NULL) host = defaultClientHost;
        if (port == NULL) port = defaultClientPort;
        
        
        memset(&addrHints, 0, sizeof(struct addrinfo));
        addrHints.ai_family = AF_INET; // IPv4
        addrHints.ai_socktype = SOCK_STREAM;
        addrHints.ai_protocol = IPPROTO_TCP;
        err = getaddrinfo(host, port, &addrHints, &addrResult);
        if (err != 0) {
                fatal("getaddrinfo: %s", gai_strerror(err));
                return false;
        }

        // initialize socket according to getaddrinfo results
        conn->sock = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
        if (conn->sock < 0) {
                syserr("Błąd przy utworzeniu socketu dla klienta\n");
                return false;
        }
        int flag = 1;
        //Nagle's algorithm
        if (setsockopt(conn->sock, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int))) {
                syserr("Błąd przy ustawieniu algorytmu dla socketu klienta\n");
                return false;
        }

        // connect socket to the server
        if (connect(conn->sock, addrResult->ai_addr, addrResult->ai_addrlen) < 0) {
                syserr("Błąd przy połączeniu z klientem");
                return false;
        }

        freeaddrinfo(addrResult);
        return true;
}

bool connect_ipv6TCP(struct sit_client_connection *conn, char *host, char *port) {
        struct addrinfo addrHints;
        struct addrinfo *addrResult;
        int err;
        if (conn == NULL) return false;
        if (host == NULL) host = defaultClientHost;
        if (port == NULL) port = defaultClientPort;
        
        
        memset(&addrHints, 0, sizeof(struct addrinfo));
        addrHints.ai_family = AF_INET6;
        addrHints.ai_socktype = SOCK_STREAM;
        addrHints.ai_protocol = IPPROTO_TCP;
        int flag = 1;
        err = getaddrinfo(host, port, &addrHints, &addrResult);
        if (err != 0) {
                fatal("getaddrinfo: %s", gai_strerror(err));
                return false;
        }
        
        bool initialized = false;
        struct addrinfo *p;
        for(p = addrResult; p != NULL && !initialized; p = p->ai_next) { 
        // initialize socket according to getaddrinfo results
        conn->sock = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
                if (conn->sock < 0) {
                        syserr("Błąd przy utworzeniu socketu dla klienta\n");
                }
                
                //Nagle's algorithm
                if (setsockopt(conn->sock, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int))) {
                        syserr("Błąd przy ustawieniu algorytmu dla socketu klienta\n");
                        continue;
                }
                
                // connect socket to the server
                if (connect(conn->sock, addrResult->ai_addr, addrResult->ai_addrlen) < 0) {
                        syserr("Błąd przy połączeniu z klientem");
                        continue;
                }
                initialized = true;
        }

        freeaddrinfo(addrResult);
        return initialized;
}

bool connect_ipv4UDP(struct sit_server_connection *conn, char *host, char* port) {
        struct addrinfo addrHints;
        struct addrinfo *addrResult;
        int err;
        if (conn == NULL) return false;
        if (host == NULL) return false;
        if (port == NULL) port = defaultServerPort;
        
        memset(&addrHints, 0, sizeof(struct addrinfo));
        addrHints.ai_family = AF_INET;
        addrHints.ai_socktype = SOCK_DGRAM;
        addrHints.ai_protocol = IPPROTO_UDP;
        err = getaddrinfo(host, port, &addrHints, &addrResult);
        if (err != 0) {
                fatal("getaddrinfo: %s", gai_strerror(err));
                return false;
        }
        // initialize socket according to getaddrinfo results
        conn->sock = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
        if (conn->sock < 0) {
                syserr("Błąd przy utworzeniu socketu dla serwera\n");
                return false;
        }

        // connect socket to the server
        if (connect(conn->sock, addrResult->ai_addr, addrResult->ai_addrlen) < 0) {
                syserr("Błąd przy połączeniu z serwera");
                return false;
        }
        freeaddrinfo(addrResult);
        return true;
}

bool connect_ipv6UDP(struct sit_server_connection *conn, char *host, char* port) {
        struct addrinfo addrHints;
        struct addrinfo *addrResult;
        int err;
        if (conn == NULL) return false;
        if (host == NULL) return false;
        if (port == NULL) port = defaultServerPort;
        
        memset(&addrHints, 0, sizeof(struct addrinfo));
        addrHints.ai_family = AF_INET6;
        addrHints.ai_socktype = SOCK_DGRAM;
        addrHints.ai_protocol = IPPROTO_UDP;
        err = getaddrinfo(host, port, &addrHints, &addrResult);
        if (err != 0) {
                fatal("getaddrinfo: %s", gai_strerror(err));
                return false;
        }

        bool initialized = false;
        struct addrinfo *p;
        for(p = addrResult; p != NULL && !initialized; p = p->ai_next) { 
        // initialize socket according to getaddrinfo results
                conn->sock = socket(addrResult->ai_family,
                        addrResult->ai_socktype, addrResult->ai_protocol);
                if (conn->sock < 0) {
                        syserr("Błąd przy utworzeniu socketu dla serwera\n");
                        continue;
                }
                
                // connect socket to the server
                if (connect(conn->sock, addrResult->ai_addr, addrResult->ai_addrlen) < 0) {
                        syserr("Błąd przy połączeniu z serwerem");
                        continue;
                }
                initialized = true;
        }

        freeaddrinfo(addrResult);
        return initialized;
}
