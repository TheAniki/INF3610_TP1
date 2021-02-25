/*
 * router.h
 *
 *  Created on: 26 July 2020
 *      Author: Guy BOIS
 */

#ifndef SRC_ROUTEUR_H_
#define SRC_ROUTEUR_H_

#include <os.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define TASK_STK_SIZE 8192

/* ************************************************
 *                TASK PRIOS
 **************************************************/

#define          TaskGeneratePRIO   			15
#define			 TaskStatsPRIO 					13
#define          TaskComputingPRIO  			21
#define          TaskForwardingPRIO 			22
#define          TaskOutputPortPRIO     		20

#define			 WAITFORComputing 3


// Routing info.

#define NB_OUTPUT_PORTS 3

#define INT1_LOW      0x00000000
#define INT1_HIGH     0x3FFFFFFF
#define INT2_LOW      0x40000000
#define INT2_HIGH     0x7FFFFFFF
#define INT3_LOW      0x80000000
#define INT3_HIGH     0xBFFFFFFF
#define INT_BC_LOW    0xC0000000
#define INT_BC_HIGH   0xFFFFFFFF

// Reject source info.
#define REJECT_LOW1   0x10000000
#define REJECT_HIGH1  0x17FFFFFF
#define REJECT_LOW2   0x50000000
#define REJECT_HIGH2  0x57FFFFFF
#define REJECT_LOW3   0x60000000
#define REJECT_HIGH3  0x67FFFFFF
#define REJECT_LOW4   0xD0000000
#define REJECT_HIGH4  0xD7FFFFFF

typedef struct{
	int id;
	char* name;
} Info_Port;

Info_Port  Port[NB_OUTPUT_PORTS];

typedef enum {
	PACKET_VIDEO,
	PACKET_AUDIO,
	PACKET_AUTRE,
	NB_PACKET_TYPE
} PACKET_TYPE;

typedef struct {
    unsigned int src;
    unsigned int dst;
    PACKET_TYPE type;
    unsigned int data[13];
} Packet;

// Stacks
static CPU_STK TaskGenerateSTK[TASK_STK_SIZE];

static CPU_STK TaskComputingSTK[TASK_STK_SIZE];

static CPU_STK TaskForwardingSTK[TASK_STK_SIZE];

static CPU_STK TaskOutputPortSTK[NB_OUTPUT_PORTS][TASK_STK_SIZE];

static CPU_STK TaskStatsSTK[TASK_STK_SIZE];

static OS_TCB TaskGenerateTCB;
static OS_TCB TaskStatsTCB;
static OS_TCB TaskComputingTCB;
static OS_TCB TaskForwardingTCB;
static OS_TCB TaskOutputPortTCB[NB_OUTPUT_PORTS];

/* ************************************************
 *                  Queues
 **************************************************/

OS_Q lowQ;
OS_Q mediumQ;
OS_Q highQ;

/* ************************************************
 *                  Semaphores
 **************************************************/

// Pas de sémaphore pour la partie 1 

/* ************************************************
 *                  Mutexes
 **************************************************/
OS_MUTEX mutRejete;
OS_MUTEX mutPrint;
OS_MUTEX mutAlloc;


/*DECLARATION DES COMPTEURS POUR STATISTIQUES*/
int nbPacketCrees = 0;								// Nb de packets total créés
int nbPacketTraites = 0;							// Nb de paquets envoyés sur une interface
int nbPacketSourceRejete = 0;						// Nb de packets rejetés pour mauvaise source
int packet_rejete_fifo_pleine_inputQ = 0;			// Utilisation de la fifo d'entrée
int packet_rejete_output_port_plein = 0;			// Utilisation des MB


/* ************************************************
 *              TASK PROTOTYPES
 **************************************************/

void TaskGenerate(void *data); 
void TaskComputing(void *data);
void TaskForwarding(void *data);
void TaskOutputPort(void *data);
void TaskStats(void* data);

void dispatch_packet (Packet* packet);

void create_application();
int create_tasks();
int create_events();
void err_msg(char* ,uint8_t);
void Suspend_Delay_Resume_All(int nb_sec);

#endif /* SRC_ROUTEUR_H_ */
