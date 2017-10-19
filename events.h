#ifndef __EVENTS_H
#define __EVENTS_H

#include "numbers.h"

#define EVENT_GAME_MESSAGE_HEADER_SIZE 4
struct sit_event_game_message {
        uint4_b game_id;
} __attribute__((packed));

#define EVENT_MAX_SIZE 512 - 4
#define EVENT_HEADER_LEN_SIZE 4
#define EVENT_HEADER_EVENT_SIZE 5
#define EVENT_HEADER_SIZE 9
#define EVENT_FOOTER_SIZE 4
#define EVENT_STATIC_SIZE 13

struct sit_event_header {
	uint4_b len;		// Sumaryczna ilość pól event_
	uint4_b event_no;	// Rosnące numery dla partii
	uint1_b event_type;	// Typ eventu	
} __attribute__((packed));

struct sit_event_footer {
	uint4_b crc;
} __attribute__((packed));

#define EVENT_NEW_GAME 0
#define EVENT_NEW_GAME_SIZE 8
struct sit_event_new_game {
	// event_type == 0
	uint4_b maxx;
	uint4_b maxy;
	char player_names[491];
} __attribute__((packed));

#define EVENT_PIXEL 1
#define EVENT_PIXEL_SIZE 9

struct sit_event_pixel {
	// event_type == 1
	uint1_b player_number;
	uint4_b x;
	uint4_b y;
} __attribute__((packed));

#define EVENT_PLAYER_ELIMINATED 2
#define EVENT_PLAYER_ELIMINATED_SIZE 1
struct sit_event_player_eliminated {
	// event_type == 2
	uint1_b player_number;
} __attribute__((packed));

#define EVENT_GAME_OVER 3
#define EVENT_GAME_OVER_SIZE 0

struct sit_event_game_over {
	//event_type == 3
} __attribute__((packed));

struct sit_server_message_st {
        uint8_b session_id;
        int1_b turn_direction;
        uint4_b next_expected_event_no;
} __attribute__((packed));

#endif //__EVENTS_H
