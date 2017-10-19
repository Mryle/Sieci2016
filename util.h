#ifndef __UTIL_H
#define __UTIL_H

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "events.h"


//Util
void utilPrintBytes(char* buffer, int size);
//Random
uint64_t utilRandom(uint64_t seed);
//ComputingCRC
uint4_b crcCompute(char *buffer, size_t size);
//Time:
void timeInit(uint64_t tick); //tick in microseconds
void timeStep();        //Used to get time
uint64_t timeGetSeconds();
uint64_t timeGetMicroSeconds();
uint64_t timeIsHeartBeat();         
uint64_t timeDifferenceFromLastHeartBeat();
uint64_t timeToNextHeartBeat(); //Zwraca czas do nastepnego kroku w mikro
uint64_t timePerHeartBeat();    //Zwraca liczbę ticków od ostatniego razu.
void timeSetHeartBeatUs(uint64_t tick);
//Time: Abstract
void timeDoHeartBeat(); //A:Function, will be executed on every heartbeat

//DatagramVector
struct vectorDatagram {
        int size;
        char* message;
} __attribute__((packed));

struct vectorDatagram vectorGet(int index);
int vectorPush(char* message, int size);
size_t vectorSize();
void vectorFree();

//IntQueue
void* queueCreate();
int queueTop(void* queue);
int queueEmpty(void* queue);
void queuePush(void* queue, int num);
void queueDelete(void* queue);
#endif //__UTIL_H

/*
Notatki:
	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);
	clock_gettime(CLOCK_REALTIME, &end);
	uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
	uint64_t delta_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
*/