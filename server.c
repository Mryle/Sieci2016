#include "server.h"

//Parametry serwera

int port = 12345;
//Zmienne serwera
struct sit_server_state state;

int sock;
struct sockaddr_in6 server_address;
struct sockaddr_in6 read_address;
struct pollfd pollA[1];
bool finish = false;
bool canSend = true;

char eventBuffer[EVENT_MAX_SIZE]; //Bufor dla przygotowania eventów
char buffer[SERVER_DGRAM_MAX];
int bufferSendSize = 0;


int main(int argc, char *argv[]) {
        gameInit();
        
        int opt;
        int tmp;
        while((opt = getopt(argc, argv, "W:H:p:s:t:r:")) != -1) {
                switch(opt) {
                case 'W':
                        tmp = atoi(optarg);
                        if (!validateScreenSize(tmp)) {
                                return 1;
                        }
                        state.width = tmp;
                        break;
                case 'H':
                        tmp = atoi(optarg);
                        if (!validateScreenSize(tmp)) {
                                return 1;
                        }
                        state.height = tmp;
                        break;
                case 'p':
                        port = atoi(optarg);
                        break;
                case 's':
                        tmp = atoi(optarg);
                        if (!validateRoundPerSec(tmp)) {
                                return 1;
                        }
                        state.rps = tmp;
                        break;
                case 't':
                        tmp = atoi(optarg);
                        if (!validateTurningSpeed(tmp)) {
                                return 1;
                        }
                        state.turningSpeed = tmp;
                        break;
                case 'r':
                        state.seed = atoi(optarg);
                        break;
                case '?':
                        break;
                default:
                        nfatal("%s [-W n] [-H n] [-p n] [-s n] [-t n] [-r n]", argv[0]);
                        nfatal("Unparsable option %c %s.\n", opt, optarg);
                        return 1;
                }
        }
        
        
        
        if (debug) {
                printf("DEBUGGING WIDTH:%d, HEIGHT:%d, port:%d, rps:%ld, turn:%d, seed:%ld\n",
                        state.width, state.height, port, state.rps, state.turningSpeed, state.seed);
        }
        
        
        
        if (!initialize_connection()) {
                return 1;
        }
        timeInit(1000000/state.rps);
        pollArrayInit();
        while(!finish) {
                finish &= pollLoop();
        }
        return 0;
}

bool validateScreenSize(int size) {
        if (size <= 0) {
                nfatal("Ujemna wielkość ekranu\n");
                return false;
        }
        if (size > 1000000) {
                nfatal("Zbyt duża wielkość ekranu\n");
                return false;
        }
        return true;
}

bool validateRoundPerSec(int rps) {
        if (rps <= 0) {
                nfatal("Ujemna ilość ROUND_PER_SEC\n");
                return false;
        }
        return true;
}

bool validateTurningSpeed(int turnSpeed) {
        if (turnSpeed <= 0) {
                nfatal("Niedodatnia szybkość skrętu\n");
                return false;
        }
        if (turnSpeed >= 360) {
                nfatal("Zbyut duża (>= 360) szybkość skrętu\n");
                return false;
        }
        return true;
}

void gameInit() {
        state.width = 800;
        state.height = 600;
        state.rps = 50;
        state.map = NULL;
        state.turningSpeed = 6;
        state.seed = time(NULL);
        int a = 0;
        for(a=0; a<MAX_PLAYERS; a++) {
                state.players[a].taken = false;
        }
}

uint64_t gameGetWidth() {
        return state.width;
}

uint64_t gameGetHeight() {
        return state.height;
}

char gameGetPixel(int x, int y) {
        int off = x + y * gameGetWidth();
        return *(state.map + off);
}

void gamePutPixel(int x, int y, char pixel) {
        int off = x + y * gameGetWidth();
        *(state.map + off) = pixel;
}

void gameStart() {
        state.game_id = gameRandom();
        state.gameInPlay = true;
        if (debug_main)
                printf("Game staring with id %d\n", state.game_id);
        vectorFree();
        state.event_no = 0;
        gameClearMap();
        gameClearPlayers();
        gameSortPlayers();
        sendEventNewGame();
        int a;
        for(a = 0; a < MAX_PLAYERS; a++) {
                if (playerAlive(state.players, a)) {
                        if (playerSpawn(state.players, a)) {
                                playerSendPixel(state.players, a);
                        } else {
                                playerKill(state.players, a);
                        }
                }
        }
        if (debug_main) printf("Game started\n");
}

void gameOver() {
        if (debug_main) printf("Game ending\n");
        state.gameInPlay = false;
        sendEventGameOver();
        for(int a=0; a<MAX_PLAYERS; a++) {
                state.players[a].ready = false;
        }
}

int comparePlayers(const void *a, const void *b) {
        struct sit_client_information *pl1 = (struct sit_client_information *) a;
        struct sit_client_information *pl2 = (struct sit_client_information *) b;
        if (pl1->taken && !pl2->taken) return -1;
        if (!pl1->taken && pl2->taken) return 1;
        if (!pl1->taken && !pl2->taken) return 0;
        return strcmp(pl1->name, pl2->name);
}

void gameSortPlayers() {
        qsort(state.players, sizeof(struct sit_client_information), MAX_PLAYERS,
                        comparePlayers);
}

void gameClearPlayers() {
        int a = 0;
        for(a = 0; a < MAX_PLAYERS; a++) {
                state.players[a].playerNum = PLAYER_SPECTATE;
                state.players[a].event_no = 0;
        }
}

bool gameShouldStart() {
        int count = 0, a;
        if (state.gameInPlay) return false;
        for(a = 0; a < MAX_PLAYERS; a++) {
                if (playerExists(state.players, a)
                                && state.players[a].nameLength != 0) {
                        if (!state.players[a].ready) return false;
                        count ++;
                }
        }
        return count >= 2;
}

//TODO: Not ending on 1 player alive
bool gameShouldEnd() {
        int count = 0, a;
        if (!state.gameInPlay) return false;
        for(a = 0; a < MAX_PLAYERS; a++) {
                if (playerAlive(state.players, a)) {
                        count ++;
                }
        }
        //printf("%d players still standing\n", count);
        return count < 2;
}

void gameClearMap() {
        if (state.map == NULL) {
                state.map = malloc(state.width * state.height);
        }
        memset(state.map, MAP_EMPTY, state.width * state.height);
}

uint4_b gameGetEventNo() {
        uint4_b event_no = state.event_no;
        for(int a=0; a< MAX_PLAYERS; a++) {
                if (playerExists(state.players, a))
                if (state.players[a].event_no > event_no) {
                        state.players[a].event_no = event_no;
                }
        }
        state.event_no ++;
        return event_no;
}

uint64_t gameRandom() {
        uint64_t ret = state.seed;
        state.seed = utilRandom(state.seed);
        return ret;
}

void gameSendRoutine() {
        uint1_b player = state.nextPlayerToSend;
        uint4_b startingEvent;
        do {
                startingEvent = state.players[player].event_no;
                if (playerExists(state.players, player)
                                && playerConnected(state.players, player)
                                && vectorSize() > startingEvent) {
                        playerSendRoutine(state.players, player);
                        player++;
                        if (player == MAX_PLAYERS) {
                                player = 0;
                        }
                        state.nextPlayerToSend = player;
                        break;
                }
                player++;
                if (player == MAX_PLAYERS) {
                        player = 0;
                }
        } while(state.nextPlayerToSend != player);
}

bool pollLoop() {
        pollArrayClear();
        timeStep();
        int ret = poll(pollA, 1, timeToNextHeartBeat()/1000);
        if (ret < 0) {
                syserr("Błąd przy wykonaniu poll\n");
                return false;
        } else if (ret > 0) {
                if (pollA[0].revents & (POLLIN | POLLERR)) {
                        if (!readFromClient()) {
                                return false;
                        }
                }
                if (pollA[0].revents & POLLOUT) canSend = true;
                if (canSend) {
                        gameSendRoutine();
                }
        }
        //Wysłanie wiadomości
        uint64_t heartBeats = timeIsHeartBeat();
        while(heartBeats > 0) {
                timeDoHeartBeat();
                heartBeats--;
        }
        return true;
}

void pollArrayInit() {
        pollA[0].fd = sock;
        pollA[0].events = POLLIN | POLLOUT;
        pollArrayClear();
}

void pollArrayClear() {
        pollA[0].revents = 0;
}

void timeDoHeartBeat() {
        if (debug_tick) {
                printf("HeartBeat\n");
        }
        int a;
        for(a = 0; a < MAX_PLAYERS; a++) {
                playerCheckConnection(state.players, a);
        }
        
        if (state.gameInPlay) {
                for(a = 0; a < MAX_PLAYERS; a++) {
                        if (playerExists(state.players, a)
                                        && !playerSpectate(state.players, a)) {
                                playerStep(state.players, a);
                        }
                }
                if (gameShouldEnd()) {
                        gameOver();
                }
        } else {
                if (gameShouldStart()) {
                        gameStart();
                }
        }
}

bool readFromClient() {
        socklen_t rcva_len = (socklen_t) sizeof(read_address);
        memset(buffer, 0, SERVER_DGRAM_MAX);
        int rval = recvfrom(pollA[0].fd, buffer, SERVER_DGRAM_MAX, 0,
                (struct sockaddr*) &read_address, &rcva_len);
        if (rval <= 0) {
                nfatal("Błąd przy odebraniu danych od klienta\n");
                return false;
        } else if (rval < sizeof(struct sit_server_message_st)) {
                nfatal("Wiadomość od klienta jest za krótka = %d\n", rval);
                return false;
        } else if (rval > sizeof(struct sit_server_message_st) + PLAYER_NAME_LENGTH) {
                nfatal("Wiadomość od klienta jest za długa = %d\n", rval);
                return false;
        }
        struct sit_server_message_st *mess = (struct sit_server_message_st*) buffer;
        mess->session_id = be64toh(mess->session_id);
        mess->next_expected_event_no = be32toh(mess->next_expected_event_no);
        if (debug_messages) {
                printf("Message %ld %d %d %s\n", mess->session_id, mess->turn_direction, mess->next_expected_event_no, buffer + sizeof(struct sit_server_message_st));
        }
        int player = playerFind(state.players, buffer + sizeof(struct sit_server_message_st),
                rval - sizeof(struct sit_server_message_st), mess);
        if (player < 0) {
                nfatal("Nie można przyjąć klienta\n");
                return false;
        }
        //Walidacja poprawności wiadomości gracza
        state.players[player].event_no = mess->next_expected_event_no;
        state.players[player].turn_direction = mess->turn_direction;
        state.players[player].ready |= (mess->turn_direction != 0);
        state.players[player].lastTime = timeGetSeconds();
        return true;
}

void playerInit(struct sit_client_information* players, int playerNum) {
        players[playerNum].taken = false;
}

int playerFind(struct sit_client_information* players, char *playerName, int playerNameLength, struct sit_server_message_st *mess) {
        int a = 0;
        for(a = 0; a < MAX_PLAYERS; a++) {
                int ret = playerCompareReadWith(players, a, mess);
                if (ret >= 0) return ret;
                if (ret == COMPARE_IGNORE) return ret;
                if (ret == COMPARE_NEW) {
                        playerAddNum(players, a, playerName, playerNameLength, mess);
                        return a;
                }
        }
        return playerAdd(players, playerName, playerNameLength, mess);
}

int playerCompareReadWith(struct sit_client_information* players, int playerNum, struct sit_server_message_st *mess) {
        if (!players[playerNum].taken) return COMPARE_DIFFERENT;
        if (memcmp(&players[playerNum].address, &read_address,
                        sizeof(struct sockaddr_in6)) != 0) return COMPARE_DIFFERENT;
        if (players[playerNum].session_id < mess->session_id) {
                return COMPARE_IGNORE;
        } else if (players[playerNum].session_id < mess->session_id) {
                playerDelete(players, playerNum);
                return COMPARE_NEW;
        }
        return playerNum;
}

int playerAdd(struct sit_client_information* players, char *playerName, int playerNameLength, struct sit_server_message_st *mess) {
        int a = 0;
        for(a = 0; a < playerNameLength; a++) {
                if (playerName[a] < 33 || playerName[a] > 126) {
                        if (debug_player) {
                                printf("Nazwa gracza %s jest niepoprawna\n",playerName);
                        }
                        return -1;
                }
        }
        for(a = 0; a < MAX_PLAYERS; a++) {
                if (players[a].taken &&
                                !playerNameValidate(players, a, playerName, playerNameLength+1)) {
                        if (debug_player) {
                                printf("Nazwa gracza %s zajęta przez %d\n", playerName, a);
                        }
                        return -1;
                }
        }
        for(a = 0; a < MAX_PLAYERS; a++) {
                if (playerAddNum(players, a, playerName, playerNameLength, mess))
                        return a;
        }
        return -1;
}

bool playerNameValidate(struct sit_client_information* players, int playerNum, char *playerName, int playerNameLength) {
        return !(players[playerNum].nameLength == playerNameLength
                && memcmp(playerName, players[playerNum].name, playerNameLength) == 0);
}

bool playerAddNum(struct sit_client_information* players, int playerNum, char *playerName, int playerNameLength, struct sit_server_message_st *mess) {
        if (!players[playerNum].taken) {
                players[playerNum].taken = true;
                players[playerNum].connected = true;
                players[playerNum].ready= false;
                memcpy(&players[playerNum].address, &read_address,
                        sizeof(struct sockaddr_in6));
                players[playerNum].name = malloc(playerNameLength + 1);
                memcpy(players[playerNum].name, playerName, playerNameLength);
                players[playerNum].name[playerNameLength] = 0;
                players[playerNum].nameLength = playerNameLength;
                players[playerNum].playerNum = PLAYER_SPECTATE;
                players[playerNum].session_id = mess->session_id;
                if (debug_player) {
                        printf("Dodano gracza %s jako %d\n", playerName, playerNum);
                }
                return true;
        }
        return false;
}

void playerCheckConnection(struct sit_client_information* players, int playerNum) {
        if (!playerExists(players, playerNum)) return;
        if (playerConnected(players, playerNum)) {
                if (timeGetSeconds() - players[playerNum].lastTime >= 2) {
                        players[playerNum].connected = false;
                        if (debug_player) {
                                printf("Player %d %s disconnected\n", playerNum, players[playerNum].name);
                        }
                }
        }
        if (!playerAlive(players, playerNum)
                        && !playerConnected(players, playerNum)) {
                if (debug_player) {
                        printf("Player %d %s deleted\n", playerNum, players[playerNum].name);
                }
                players[playerNum].taken = false;
                if (players[playerNum].name != NULL) {
                        free(players[playerNum].name);
                        players[playerNum].name = NULL;
                }
        }
}

void playerStep(struct sit_client_information* players, int playerNum) {
        if (!playerAlive(players, playerNum)) {
                return;
        }
        uint4_b ox = players[playerNum].posX;
        uint4_b oy = players[playerNum].posY;
        if (players[playerNum].turn_direction == -1) {
                players[playerNum].direction  -= state.turningSpeed;
                if (players[playerNum].direction < 0)
                        players[playerNum].direction += 360;
        } else if (players[playerNum].turn_direction == 1) {
                players[playerNum].direction  += state.turningSpeed;
                if (players[playerNum].direction > 360)
                        players[playerNum].direction -= 360;
        }
        double direction = players[playerNum].direction;
        while((uint4_b)players[playerNum].posX == ox
                        && (uint4_b)players[playerNum].posY == oy) {
                players[playerNum].posX += cos(direction * PI / 180.0);
                players[playerNum].posY += sin(direction * PI / 180.0);
        }
        ox = players[playerNum].posX;
        oy = players[playerNum].posY;
        if (ox >= 0 && ox < gameGetWidth()
                        && oy >= 0 && oy < gameGetHeight()
                        && gameGetPixel(ox, oy) == MAP_EMPTY) {
                playerSendPixel(state.players, playerNum);
        } else {
                playerKill(state.players, playerNum);
        }
}

void playerDelete(struct sit_client_information* players, int playerNum) {
        players[playerNum].taken = false;
        if (players[playerNum].name != NULL) {
                free(players[playerNum].name);
                players[playerNum].name = NULL;
        }
}

bool playerSpawn(struct sit_client_information* players, int playerNum) {
        players[playerNum].posX = 0.5 + (double) (gameRandom() % gameGetWidth());
        players[playerNum].posY = 0.5 + (double) (gameRandom() % gameGetHeight());
        players[playerNum].direction = gameRandom() % 360;
        return gameGetPixel(players[playerNum].posX, players[playerNum].posY) == MAP_EMPTY;
}

bool playerExists(struct sit_client_information* players, int playerNum) {
        return players[playerNum].taken;
}

bool playerSpectate(struct sit_client_information* players, int playerNum) {
        return players[playerNum].playerNum == PLAYER_SPECTATE;
}

bool playerAlive(struct sit_client_information* players, int playerNum) {
        return playerExists(players, playerNum)
                && !playerSpectate(players, playerNum);
}

bool playerConnected(struct sit_client_information* players, int playerNum) {
        return players[playerNum].connected;
}

void playerSendRoutine(struct sit_client_information* players, int playerNum) {
        sendToPlayer(players, playerNum);
}

void playerSendPixel(struct sit_client_information* players, int playerNum) {
        gamePutPixel((uint4_b)players[playerNum].posX,
                     (uint4_b)players[playerNum].posY,
                     players[playerNum].playerNum);
        sendEventPixel(players[playerNum].playerNum,
                        (uint4_b)players[playerNum].posX,
                        (uint4_b)players[playerNum].posY);
}

void playerKill(struct sit_client_information* players, int playerNum) {
        sendEventPlayerEliminated(players[playerNum].playerNum);
        if (players[playerNum].connected)
                players[playerNum].playerNum = PLAYER_SPECTATE;
        else
                players[playerNum].taken = false;
}

bool sendToPlayer(struct sit_client_information* players, int playerNum) {
        if (debug_tick) {
                printf("Sending to player %d from %d max %ld\n",
                                playerNum, players[playerNum].event_no,
                                vectorSize());
        }
        sendMessagePrepare(playerNum);
        
        socklen_t send_len = sizeof(players[playerNum].address);
        int len = sendto(sock, buffer, bufferSendSize, 0,
                (struct sockaddr*)&players[playerNum].address, send_len);
        if (debug_send_raw) {
                printf("Send %d: \n", playerNum);
                utilPrintBytes(buffer, bufferSendSize);
                printf("---\n");
        }
        canSend = false;
        if (len != bufferSendSize) {
                nfatal("Nieudane przesłanie wiadomości do %d = %d\n", playerNum, len);
                return false;
        }
        return true;
}

void sendEventPrepare() {
        memset(eventBuffer, 0, EVENT_MAX_SIZE);
        bufferSendSize = 0;
}

void sendEventPlayerEliminated(uint1_b player_number) {
        sendEventPrepare();
        uint4_b len = EVENT_HEADER_EVENT_SIZE + EVENT_PLAYER_ELIMINATED_SIZE;
        struct sit_event_header* header = (struct sit_event_header*)eventBuffer;
        header->event_type = EVENT_PLAYER_ELIMINATED;
        uint4_b eventNum = gameGetEventNo();
        header->event_no = htobe32(eventNum);
        header->len = htobe32(len);
        
        struct sit_event_player_eliminated* event =
                (struct sit_event_player_eliminated*) (eventBuffer + EVENT_HEADER_SIZE); 
        event->player_number = player_number;
        
        
        if (debug_events)
                printf("EVENT(%u): player eliminated %d (%d)", eventNum,player_number, len);
        
        sendEndFooter(len);
        sendEventComplete(len);
}

void sendEndFooter(uint4_b len) {
        struct sit_event_footer* footer =
                (struct sit_event_footer*) (eventBuffer + len + EVENT_HEADER_LEN_SIZE);
        uint4_b crc = crcCompute(eventBuffer, len + EVENT_HEADER_LEN_SIZE);
        if (debug_events)
                printf("[%u]\n", crc);
        footer->crc = htobe32(crc);
}

void sendEventGameOver() {
        sendEventPrepare();
        uint4_b len = EVENT_HEADER_EVENT_SIZE + EVENT_GAME_OVER_SIZE;
        struct sit_event_header* header = (struct sit_event_header*)eventBuffer;
        header->event_type = EVENT_GAME_OVER;
        uint4_b eventNum = gameGetEventNo();
        header->event_no = htobe32(eventNum);
        header->len = htobe32(len);
        
        //Certainly unused
        //struct sit_event_game_over* event =
        //        (struct sit_event_game_over*) eventBuffer + EVENT_HEADER_SIZE; 
        
        if (debug_events) 
                printf("EVENT(%u): Game Over (%d)\n", eventNum, len);
        sendEndFooter(len);
        sendEventComplete(len);
}

void sendEventNewGame() {
        sendEventPrepare();
        uint4_b len = EVENT_HEADER_EVENT_SIZE + EVENT_NEW_GAME_SIZE;
        uint4_b lenLeft = EVENT_MAX_SIZE - len;
        struct sit_event_header* header = (struct sit_event_header*)eventBuffer;
        header->event_type = EVENT_NEW_GAME;
        uint4_b eventNum = gameGetEventNo();
        header->event_no = htobe32(eventNum);
        struct sit_event_new_game* event =
                (struct sit_event_new_game*) (eventBuffer + EVENT_HEADER_SIZE); 
        event->maxx = htobe32(state.width);
        event->maxy = htobe32(state.height);
        
        uint1_b playerCon = 0;
        uint1_b playerNum = 0;
        
        char *names = (event->player_names);
        while(lenLeft > 0 && playerNum < MAX_PLAYERS) {
                if (!playerExists(state.players, playerNum)) break;
                if (lenLeft < state.players[playerNum].nameLength + 1) {
                        break;
                }
                if (state.players[playerNum].nameLength != 0) {
                        memcpy(names, state.players[playerNum].name, state.players[playerNum].nameLength);
                        names[state.players[playerNum].nameLength] = 0;
                        names += state.players[playerNum].nameLength + 1;
                        len += state.players[playerNum].nameLength + 1;
                        lenLeft -= state.players[playerNum].nameLength + 1;
                        if (debug_main)
                                printf("Gracz %s jako %d\n", state.players[playerNum].name, playerCon);
                        state.players[playerNum].playerNum = playerCon++; 
                } else {
                        state.players[playerNum].playerNum = PLAYER_SPECTATE;
                }
                playerNum++;
        }
        while(playerNum < MAX_PLAYERS) {
                state.players[playerNum].playerNum = PLAYER_SPECTATE;
                playerNum++;
        }
        names--;
        names[0] = 0; //Zastępujemy ostatnią spację zerem
        
        header->len = htobe32(len);
        
        if (debug_events)
                printf("EVENT(%u): New Game %d %d %s (%d)", eventNum,
                        state.width, state.height,
                        event->player_names, len);
        sendEndFooter(len);
        sendEventComplete(len);
}

void sendMessagePrepare(uint1_b playerNum) {
        uint4_b starting = state.players[playerNum].event_no;
        memset(buffer, 0, SERVER_DGRAM_MAX);
        struct sit_event_game_message *mess = (struct sit_event_game_message*) buffer;
        mess->game_id = htobe32(state.game_id);
        uint4_b left = SERVER_DGRAM_MAX - sizeof(struct sit_event_game_message);
        char* index = buffer + sizeof(struct sit_event_game_message);
        while(left > 0 && vectorSize() > starting) {
                struct vectorDatagram message = vectorGet(starting);
                if (message.size <= left) {
                        memcpy(index, message.message, message.size);
                        index += message.size;
                        left -= message.size;
                        starting++;
                } else break;
        }
        state.players[playerNum].event_no = starting;
        bufferSendSize = SERVER_DGRAM_MAX - left;
}

void sendEventPixel(uint1_b player_number, uint4_b x, uint4_b y) {
        sendEventPrepare();
        uint4_b len = EVENT_HEADER_EVENT_SIZE + EVENT_PIXEL_SIZE;
        struct sit_event_header* header = (struct sit_event_header*)eventBuffer;
        header->event_type = EVENT_PIXEL;
        uint4_b eventNum = gameGetEventNo();
        header->event_no = htobe32(eventNum);
        header->len = htobe32(len);
        struct sit_event_pixel* event =
                (struct sit_event_pixel*) (eventBuffer + EVENT_HEADER_SIZE); 
        event->player_number = player_number;
        event->x = htobe32(x);
        event->y = htobe32(y);
        
        if (debug_events)
                printf("EVENT(%u): Pixel %d %d %d (%d)", eventNum, player_number, x, y, len);
        
        sendEndFooter(len);
        sendEventComplete(len);
}

void sendEventComplete(int eventSize) {
        vectorPush(eventBuffer,
                EVENT_HEADER_LEN_SIZE + eventSize + EVENT_FOOTER_SIZE);
}

bool initialize_connection() {
        sock = socket(AF_INET6, SOCK_DGRAM, 0);
        if (sock < 0) {
                syserr("socket");
                return false;
        }
        server_address.sin6_family = AF_INET6;
        server_address.sin6_addr=in6addr_any;
        server_address.sin6_port = htons(port);

        if (bind(sock, (struct sockaddr *) &server_address, (socklen_t)sizeof(server_address)) < 0) {
                syserr("bind");
                return false;
        }
        return true;
}
