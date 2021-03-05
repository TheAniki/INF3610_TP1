
/*
*********************************************************************************************************
*                                                 uC/OS-III
*                                          The Real-Time Kernel
*                                               PORT Windows
*
*
*            		          					Guy BOIS
*                                  Polytechnique Montreal, Qc, CANADA
*                                                  02/2021
*
* File : routeur.c
*
*********************************************************************************************************
*/

#include "routeur.h"

#include  <cpu.h>
#include  <lib_mem.h>
#include  <os.h>
#include  "os_app_hooks.h"
#include  "app_cfg.h"

// À utiliser pour suivre le remplissage et le vidage des fifos
// Mettre en commentaire et utiliser la fonction vide suivante si vous ne voulez pas de trace
#define safeprintf(fmt, ...)															\
{																						\
	OSMutexPend(&mutPrint, 0, OS_OPT_PEND_BLOCKING, &ts, &perr);						\
	printf(fmt, ##__VA_ARGS__);															\
	OSMutexPost(&mutPrint, OS_OPT_POST_NONE, &perr);									\
}

// À utiliser pour ne pas avoir les traces de remplissage et de vidage des fifos
//#define safeprintf(fmt, ...)															\
//{			}



///////////////////////////////////////////////////////////////////////////////////////
//								Routines d'interruptions
///////////////////////////////////////////////////////////////////////////////////////

/*
À venir dans la partie 2
*/



/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/

int main(void)
{
	OS_ERR  os_err;

	CPU_IntInit();

	Mem_Init();                                                 // Initialize Memory Managment Module                   
	CPU_IntDis();                                               // Disable all Interrupts                               
	CPU_Init();                                                 // Initialize the uC/CPU services                       

	OSInit(&os_err);

	create_application();

	OSStart(&os_err);

	return 0;
}

void create_application() {
	int error;

	error = create_events();
	if (error != 0)
		printf("Error %d while creating events\n", error);

	error = create_tasks();
	if (error != 0)
		printf("Error %d while creating tasks\n", error);
}

int create_tasks() {
	int i;

	for (i = 0; i < NB_OUTPUT_PORTS; i++)
	{
		Port[i].id = i;
		switch (i)
		{
		case 0:
			Port[i].name = "Port 0";
			break;
		case 1:
			Port[i].name = "Port 1";
			break;
		case 2:
			Port[i].name = "Port 2";
			break;
		default:
			break;
		};
	}

	// Creation des taches
	OS_ERR err;

	OSTaskCreate(&TaskGenerateTCB, "TaskGenerate", TaskGenerate, (void*)0, TaskGeneratePRIO, &TaskGenerateSTK[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	OSTaskCreate(&TaskComputingTCB, "TaskComputing", TaskComputing, (void*)0, TaskComputingPRIO, &TaskComputingSTK[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 1024, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	OSTaskCreate(&TaskForwardingTCB, "TaskForwarding", TaskForwarding, (void*)0, TaskForwardingPRIO, &TaskForwardingSTK[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	// Pour éviter d'avoir 3 fois le même code on a un tableau pour lequel chaque entrée appel TaskOutputPort avec des paramètres différents
	for (i = 0; i < NB_OUTPUT_PORTS; i++) {
		OSTaskCreate(&TaskOutputPortTCB[i], "OutputPort", TaskOutputPort, &Port[i], TaskOutputPortPRIO, &TaskOutputPortSTK[i][0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);
	};

	OSTaskCreate(&TaskStatsTCB, "TaskStats", TaskStats, (void*)0, TaskStatsPRIO, &TaskStatsSTK[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	return 0;
}

int create_events() {
	OS_ERR err;
	int i;

	// Creation des semaphores
	// Pas de sémaphore pour la partie 1

	// Creation des mutex
	OSMutexCreate(&mutRejete, "mutRejete", &err);
	OSMutexCreate(&mutPrint, "mutPrint", &err);
	OSMutexCreate(&mutAlloc, "mutAlloc", &err);

	// Creation des files externes  - vous pourrez diminuer au besoin la longueur des files
	OSQCreate(&lowQ, "lowQ", 1024, &err);
	OSQCreate(&mediumQ, "mediumQ", 1024, &err);
	OSQCreate(&highQ, "highQ", 1024, &err);

	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////
//									TASKS
///////////////////////////////////////////////////////////////////////////////////////

/*
 *********************************************************************************************************
 *											  TaskGeneratePacket
 *  - Génère des paquets et les envoie dans le fifo d'entrée.
 *  - À des fins de développement de votre application, vous pouvez *temporairement* modifier la variable
 *    "shouldSlowthingsDown" à true pour ne générer que quelques paquets par seconde, et ainsi pouvoir
 *    déboguer le flot de vos paquets de manière plus saine d'esprit. Cependant, la correction sera effectuée
 *    avec cette variable à false.
 *********************************************************************************************************
 */
void TaskGenerate(void* data) {
	srand(42);
	OS_ERR err, perr;
	CPU_TS ts;
	bool isGenPhase = false; 		//Indique si on est dans la phase de generation ou non
	const bool shouldSlowThingsDown = false;		//Variable à modifier
	int packGenQty = (rand() % 250);
	while (true) {
		if (isGenPhase) {
			OSMutexPend(&mutAlloc, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
			Packet* packet = malloc(sizeof(Packet));
			OSMutexPost(&mutAlloc, OS_OPT_POST_NONE, &err);

			packet->src = rand() * (UINT32_MAX / RAND_MAX);
			packet->dst = rand() * (UINT32_MAX / RAND_MAX);
			packet->type = rand() % NB_PACKET_TYPE;

			for (int i = 0; i < ARRAY_SIZE(packet->data); ++i)
				packet->data[i] = (unsigned int)rand();
			packet->data[0] = nbPacketCrees;

			nbPacketCrees++;
			OSMutexPend(&mutPrint, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
			//if (shouldSlowThingsDown) {
			printf("GENERATE : ********Generation du Paquet # %d ******** \n", nbPacketCrees);
			printf("ADD %x \n", packet);
			printf("	** src : %x \n", packet->src);
			printf("	** dst : %x \n", packet->dst);
			printf("	** type : %d \n", packet->type);
			OSMutexPost(&mutPrint, OS_OPT_POST_NONE, &err);

			//}

			OSTaskQPost(&TaskComputingTCB, packet, sizeof(packet), OS_OPT_POST_FIFO + OS_OPT_POST_NO_SCHED, &err);

			safeprintf("Nb de paquets dans le fifo d'entrée - apres production de TaskGenenerate: %d \n", TaskComputingTCB.MsgQ.NbrEntries);

			if (err == OS_ERR_Q_MAX || err== OS_ERR_MSG_POOL_EMPTY) {

				OSMutexPend(&mutAlloc, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
				safeprintf("GENERATE: Paquet rejete a l'entree car la FIFO est pleine !\n");
				free(packet);
				OSMutexPost(&mutAlloc, OS_OPT_POST_NONE, &err);
				packet_rejete_fifo_pleine_inputQ++;
			}

			if (shouldSlowThingsDown) {
				OSTimeDlyHMSM(0, 0, 0, 200 + rand() % 600, OS_OPT_TIME_HMSM_STRICT, &err);
			}
			else {
				OSTimeDlyHMSM(0, 0, 0, 1, OS_OPT_TIME_HMSM_STRICT, &err);

				if ((nbPacketCrees % packGenQty) == 0) //On genère jusqu'à 250 paquets par phase de géneration
				{
					isGenPhase = false;
				}
			}
		}
		else
		{
			OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_HMSM_STRICT, &err); // 2 sec
			isGenPhase = true;
			do { packGenQty = (rand() % 256); } while (packGenQty == 0); // 255 paquets

			safeprintf("GENERATE: Generation de %d paquets durant les %d prochaines millisecondes\n", packGenQty, packGenQty * 2);
		}
	}
}

/*
 *********************************************************************************************************
 *											  TaskStop
 *  -Stoppe le routeur une fois que 100 paquets ont été rejetés pour mauvais CRC
 *  -Ne doit pas stopper la tâche d'affichage des statistiques.
 *********************************************************************************************************
 */
 // Partie 2 (oubliez ça pour l'instant)



 /*
  *********************************************************************************************************
  *											  TaskReset
  *  -Remet le compteur de mauvaises sources à 0
  *  -Si le routeur était arrêté, le redémarre
  *********************************************************************************************************
  */
  // Partie 2 (oubliez ça pour l'instant)

  /*
   *********************************************************************************************************
   *											  TaskComputing
   *  -Vérifie si les paquets sont conformes (CRC,Adresse Source)
   *  -Dispatche les paquets dans des files (HIGH,MEDIUM,LOW)
   *
   *********************************************************************************************************
   */
void TaskComputing(void* pdata) {
	OS_ERR err, perr;
	CPU_TS ts;
	OS_MSG_SIZE msg_size;
	Packet* packet = NULL;
	OS_TICK actualticks = 0;
	while (true) {
		//		1) Appel de fonction à compléter, 2) compléter safeprint et 3) compléter err_msg
		packet = OSTaskQPend(0, OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);//***
		safeprintf("Nb de paquets dans le fifo d'entrée - apres consommation de TaskComputing: %d \n", TaskComputingTCB.MsgQ.NbrEntries);//***
		err_msg("TaskComputing: Erreur sur la recherche du prochain paquet", err); //***

		// ****************************************************************** //
			/* On simule un temps de traitement avec ce compteur bidon.
			 * Cette boucle devrait prendre entre 2 et 4 ticks d'OS (considérez
			 * exactement 3 ticks pour la question dans l'énoncé).
			 */
			 //		Code de l'attente active à compléter, utilisez la constante WAITFORComputing 
		actualticks = OSTimeGet(&err);//***
		while (WAITFORComputing + actualticks > OSTimeGet(&err)) {}//***
		// ****************************************************************** //

		//Verification de l'espace d'addressage
		if ((packet->src > REJECT_LOW1 && packet->src < REJECT_HIGH1) ||
			(packet->src > REJECT_LOW2 && packet->src < REJECT_HIGH2) ||
			(packet->src > REJECT_LOW3 && packet->src < REJECT_HIGH3) ||
			(packet->src > REJECT_LOW4 && packet->src < REJECT_HIGH4)) {
			OSMutexPend(&mutRejete, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
			++nbPacketSourceRejete;

			OSMutexPend(&mutPrint, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
			printf("\n--TaskComputing: Source invalide (Paquet rejete) (total : %d)\n", nbPacketSourceRejete);
			printf("\n--Il s agit du paquet\n");
			printf("	** src : %x \n", packet->src);
			printf("	** dst : %x \n", packet->dst);
			OSMutexPost(&mutPrint, OS_OPT_POST_NONE, &err);

			OSMutexPost(&mutRejete, OS_OPT_POST_NONE, &err);

			OSMutexPend(&mutAlloc, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
			free(packet);
			OSMutexPost(&mutAlloc, OS_OPT_POST_NONE, &err);
		}
		else {

			//Dispatche les paquets selon leur type
			switch (packet->type) {
			case PACKET_VIDEO:
				//			1) Appel de fonction à compléter et 2) compléter safeprint
				OSQPost(&highQ, packet, sizeof(Packet), OS_OPT_POST_FIFO, &err); // ***
				safeprintf("Nb de paquets dans la queue de haute priorité - apres production de TaskComputing: %d \n", highQ.MsgQ.NbrEntries);//***
				break;

			case PACKET_AUDIO:
				//			1) Appel de fonction à compléter et 2) compléter safeprint
				OSQPost(&mediumQ, packet, sizeof(Packet), OS_OPT_POST_FIFO, &err); // ***
				safeprintf("Nb de paquets dans la queue de moyenne priorité - apres production de TaskComputing: %d \n", mediumQ.MsgQ.NbrEntries);//***
				break;

			case PACKET_AUTRE:
				//			1) Appel de fonction à compléter et 2) compléter safeprint
				OSQPost(&lowQ, packet, sizeof(Packet), OS_OPT_POST_FIFO, &err); // ***
				safeprintf("Nb de paquets dans la queue de faible priorité - apres production de TaskComputing: %d \n", lowQ.MsgQ.NbrEntries);//***
				break;

			default:
				break;
			}
			if (err == OS_ERR_Q_MAX || err == OS_ERR_MSG_POOL_EMPTY ) {
				safeprintf("TaskComputing : QFULL.\n");
				OSMutexPend(&mutAlloc, 0, OS_OPT_PEND_BLOCKING, &ts, &err);//***
				free(packet);//***
				packet_rejete_3Q++;//***
				OSMutexPost(&mutAlloc, OS_OPT_POST_NONE, &err);//***
			}

		}
	}
}


/*
 *********************************************************************************************************
 *											  TaskForwarding
 *  -traite la priorité des paquets : si un paquet de haute priorité est prêt,
 *   on l'envoie à l'aide de la fonction dispatch_packet, sinon on regarde les paquets de moins haute priorité
 *********************************************************************************************************
 */
void TaskForwarding(void* pdata) {
	OS_ERR perr, err = OS_ERR_NONE;
	CPU_TS ts;
	OS_MSG_SIZE msg_size;
	Packet* packet = NULL;

	while (1) {
		/* Si paquet vidéo prêt */
//		1) Appel de fonction à compléter et 2) compléter safeprint
		packet = OSQPend(&highQ, 0, OS_OPT_PEND_NON_BLOCKING, &msg_size, &ts, &err);//***
		safeprintf("Nb de paquets dans la queue de haute priorité - apres consommation de TaskFowarding: %d \n", highQ.MsgQ.NbrEntries);//***
		if (err == OS_ERR_NONE) {
			/* Envoi du paquet */
			++nbPacketTraites;//***
			safeprintf("\n--TaskForwarding:  paquets %d envoyes\n\n", nbPacketTraites);
			dispatch_packet(packet);
		}
		else {

			/* Si paquet audio prêt */
//			1) Appel de fonction à compléter et 2) compléter safeprint
			packet = OSQPend(&mediumQ, 0, OS_OPT_PEND_NON_BLOCKING, &msg_size, &ts, &err);//***
			safeprintf("Nb de paquets dans la queue de moyenne priorité - apres consommation de TaskFowarding: %d \n", mediumQ.MsgQ.NbrEntries);//***
			if (err == OS_ERR_NONE) {
				/* Envoi du paquet */
				++nbPacketTraites;//***
				safeprintf("\n--TaskForwarding: paquets %d envoyes\n\n", nbPacketTraites);
				dispatch_packet(packet);
			}
			else {
				/* Si paquet autre prêt */
//				1) Appel de fonction à compléter et 2) compléter safeprint	
				packet = OSQPend(&lowQ, 0, OS_OPT_PEND_NON_BLOCKING, &msg_size, &ts, &err);//***	
				safeprintf("Nb de paquets dans la queue de faible priorité - apres consommation de TaskFowarding: %d \n", lowQ.MsgQ.NbrEntries);//***
				if (err == OS_ERR_NONE) {
					/* Envoi du paquet */
					++nbPacketTraites;//***
					safeprintf("\n--TaskForwarding: paquets %d envoyes\n\n", nbPacketTraites);
					dispatch_packet(packet);
				}
			}
		}
	}
}

/*
 *********************************************************************************************************
 *											  Fonction Dispatch
 *  -Envoie le paquet passé en paramètre vers la mailbox correspondante à son adressage destination
 *********************************************************************************************************
 */
void dispatch_packet(Packet* packet) {
	OS_ERR perr, err = OS_ERR_NONE; //***
	CPU_TS ts;
	OS_MSG_SIZE msg_size;

	/* Test sur la destination du paquet */
	if (packet->dst >= INT1_LOW && packet->dst <= INT1_HIGH) {

		safeprintf("\n--Paquet dans Output Port no 0\n");
		//		Appel de fonction à compléter
		OSTaskQPost(&TaskOutputPortTCB[0], packet, sizeof(Packet), OS_OPT_POST_FIFO, &err);//***
	}
	else {
		if (packet->dst >= INT2_LOW && packet->dst <= INT2_HIGH) {
			safeprintf("\n--Paquet dans Output Port no 1\n");
			//			Appel de fonction à compléter
			OSTaskQPost(&TaskOutputPortTCB[1], packet, sizeof(Packet), OS_OPT_POST_FIFO, &err);//***
		}
		else {
			if (packet->dst >= INT3_LOW && packet->dst <= INT3_HIGH) {
				safeprintf("\n--Paquet dans OutputPort no 2\n");
				//					Appel de fonction à compléter
				OSTaskQPost(&TaskOutputPortTCB[2], packet, sizeof(Packet), OS_OPT_POST_FIFO, &err);//***
			}
			else {
				if (packet->dst >= INT_BC_LOW && packet->dst <= INT_BC_HIGH) {
					Packet* others[2];
					int i;
					for (i = 0; i < ARRAY_SIZE(others); ++i) {
						OSMutexPend(&mutAlloc, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
						others[i] = malloc(sizeof(Packet));
						OSMutexPost(&mutAlloc, OS_OPT_POST_NONE, &err);
						memcpy(others[i], packet, sizeof(Packet));
					}
					safeprintf("\n--Paquet BC dans Output Port no 0 à 2\n");
					//						Appels de fonction à compléter
					OSTaskQPost(&TaskOutputPortTCB[0], packet, sizeof(Packet), OS_OPT_POST_FIFO, &err);//***
					OSTaskQPost(&TaskOutputPortTCB[1], others[0], sizeof(Packet), OS_OPT_POST_FIFO, &err);//***
					OSTaskQPost(&TaskOutputPortTCB[2], others[1], sizeof(Packet), OS_OPT_POST_FIFO, &err);//***
				}
			}
		}
	}
	if (err == OS_ERR_Q_MAX || err == OS_ERR_MSG_POOL_EMPTY ) {
		/*Destruction du paquet si la mailbox de destination est pleine*/

		OSMutexPend(&mutAlloc, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
		safeprintf("\n--TaskForwarding: Erreur mailbox full\n");
		free(packet);
		packet_rejete_output_port_plein++;
		OSMutexPost(&mutAlloc, OS_OPT_POST_NONE, &err);

	}
}

/*
 *********************************************************************************************************
 *											  TaskPrint
 *  -Affiche les infos des paquets arrivés à destination et libere la mémoire allouée
 *********************************************************************************************************
 */
void TaskOutputPort(void* data) {
	OS_ERR err, perr;
	CPU_TS ts;
	OS_MSG_SIZE msg_size;
	Packet* packet = NULL;
	Info_Port info = *(Info_Port*)data;
	while (1) {
		/*Attente d'un paquet*/
//		1) Appel de fonction à compléter, 2) compléter err_msg 
		packet = OSTaskQPend(0, OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);//***
		err_msg("PRINT : erreur dans la recherche du packet", err); //***

		OSMutexPend(&mutPrint, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
		/*impression des infos du paquets*/
		printf("\nPaquet recu en %d \n", info.id);
		printf("    >> src : %x \n", packet->src);
		printf("    >> dst : %x \n", packet->dst);
		printf("    >> type : %d \n", packet->type);
		OSMutexPost(&mutPrint, OS_OPT_POST_NONE, &err);

		/*Libération de la mémoire*/
		OSMutexPend(&mutAlloc, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
		free(packet);
		OSMutexPost(&mutAlloc, OS_OPT_POST_NONE, &err);
	}

}

/*
 *********************************************************************************************************
 *                                              TaskStats
 *  -Est déclenchée lorsque le gpio_isr() libère le sémaphore
 *  -Lorsque déclenchée, imprime les statistiques du routeur à cet instant
 *********************************************************************************************************
 */
void TaskStats(void* pdata) {
	OS_ERR err, perr;
	CPU_TS ts;
	OS_TICK actualticks;

	OSTaskSuspend(&TaskGenerateTCB, &err);
	OSTaskSuspend(&TaskComputingTCB, &err);
	OSTaskSuspend(&TaskForwardingTCB, &err);

	for (int i = 0; i < NB_OUTPUT_PORTS; i++) {
		OSTaskSuspend(&TaskOutputPortTCB[i], &err);
	};

	OSStatTaskCPUUsageInit(&err);
	OSStatReset(&err);

	OSTaskResume(&TaskGenerateTCB, &err);
	OSTaskResume(&TaskComputingTCB, &err);
	OSTaskResume(&TaskForwardingTCB, &err);
	for (int i = 0; i < NB_OUTPUT_PORTS; i++) {
		OSTaskResume(&TaskOutputPortTCB[i], &err);
	}

	while (1) {
		OSMutexPend(&mutPrint, 0, OS_OPT_PEND_BLOCKING, &ts, &err);

		printf("\n------------------ Affichage des statistiques ------------------\n\n");

		// À compléter en utilisant la numérotation de 1 à 15  dans l'énoncé du laboratoire
		// 1)  Nb de paquets total créés
		printf("1- Nb de paquets total crees : %d \n", nbPacketCrees);

		// 2)  Nb de paquets total traités 
		printf("2- Nb de paquets total traites : %d \n\n", nbPacketTraites);

		// 3)  Nb de paquets rejetés pour mauvaise source (adresse)
		printf("3- Nb de paquets rejetes pour mauvaise source (adresse) : %d \n", nbPacketSourceRejete);

		// 4)  Nb de paquets rejetés dans la fifo d’entrée 
		printf("4- Nb de paquets rejetes dans la fifo d entree : %d \n", packet_rejete_fifo_pleine_inputQ);

		printf("4.5- Nb de paquets rejetes dans les Q : %d\n", packet_rejete_3Q);

		// 5)  Nb de paquets rejetés dans l’interface de sortie 
		printf("5- Nb de paquets rejetes dans l interface de sortie : %d \n\n", packet_rejete_output_port_plein);

		// 6)  Nb de paquets maximum dans le fifo d'entrée
		printf("6- Nb de paquets maximum dans le fifo d entree : %d \n", TaskStatsTCB.MsgQ.NbrEntriesMax); 

		// 7)  Nb de paquets maximum dans highQ 
		printf("7- Nb de paquets maximum dans highQ : %d \n", highQ.MsgQ.NbrEntriesMax);

		// 8)  Nb de paquets maximum dans mediumQ 
		printf("8- Nb de paquets maximum dans mediumQ : %d \n", mediumQ.MsgQ.NbrEntriesMax);

		// 9)  Nb de paquets maximum dans lowQ 
		printf("9- Nb de paquets maximum dans lowQ : %d \n\n", lowQ.MsgQ.NbrEntriesMax);

		// 10) Pourcentage de temps CPU Max de TaskGenerate 
		printf("10- Pourcentage de temps CPU Max de TaskGenerate : %u\% \n", TaskGenerateTCB.CPUUsageMax);

		// 11) Pourcentage de temps CPU Max TaskComputing 
		printf("11- Pourcentage de temps CPU Max de TaskComputing : %u\% \n", TaskComputingTCB.CPUUsageMax);

		// 12)  Pourcentage de temps CPU Max TaskFowarding 
		printf("12- Pourcentage de temps CPU Max de TaskFowarding : %u\% \n", TaskForwardingTCB.CPUUsageMax);

		// 13) Pourcentage de temps CPU Max TaskOutputPort no 1 
		printf("13- Pourcentage de temps CPU Max de TaskOutputPort no 1 : %u\% \n", TaskOutputPortTCB[0].CPUUsageMax);

		// 14) Pourcentage de temps CPU Max TaskOutputPort no 2 
		printf("14- Pourcentage de temps CPU Max de TaskOutputPort no 2 : %u\% \n", TaskOutputPortTCB[1].CPUUsageMax);

		// 15) Pourcentage de temps CPU Max TaskOutputPort no 3 
		printf("15- Pourcentage de temps CPU Max de TaskOutputPort no 3 : %u\% \n", TaskOutputPortTCB[2].CPUUsageMax);

		printf("16- Pourcentage de temps CPU  : %d \n", OSStatTaskCPUUsage / 100);
		printf("17- Pourcentage de temps CPU Max : %d \n", OSStatTaskCPUUsageMax / 100);
		printf("18- Message free : %d \n", OSMsgPool.NbrFree);
		printf("19- Message used : %d \n", OSMsgPool.NbrUsed);
		printf("20- Message used max : %d \n", OSMsgPool.NbrUsedMax);

		OSMutexPost(&mutPrint, OS_OPT_POST_NONE, &err);

		Suspend_Delay_Resume_All(10);

		OSTimeDlyHMSM(0, 0, 10, 0, OS_OPT_TIME_HMSM_STRICT, &err);
	}
}

/*
 *********************************************************************************************************
 *                                              Suspend_Delay_Resume
 *  -Utilise lors de l'initialisation de la tache statistique
 *  -Permet aussi d'arrêter l'exécution durant l'exécution
 *********************************************************************************************************
 */

void Suspend_Delay_Resume_All(int nb_sec) {

	OS_ERR err;
	int i;

	OSTaskSuspend(&TaskGenerateTCB, &err);
	OSTaskSuspend(&TaskComputingTCB, &err);
	OSTaskSuspend(&TaskForwardingTCB, &err);

	for (i = 0; i < NB_OUTPUT_PORTS; i++) {
		OSTaskSuspend(&TaskOutputPortTCB[i], &err);
	};

	OSTimeDlyHMSM(0, 0, nb_sec, 0, OS_OPT_TIME_HMSM_STRICT, &err);
	
	OSTaskResume(&TaskGenerateTCB, &err);
	OSTaskResume(&TaskComputingTCB, &err);
	OSTaskResume(&TaskForwardingTCB, &err);
	for (i = 0; i < NB_OUTPUT_PORTS; i++) {
		OSTaskResume(&TaskOutputPortTCB[i], &err);
	}

}


void err_msg(char* entete, uint8_t err)
{
	if (err != 0)
	{
		printf(entete);
		printf(": Une erreur est retournée : code %d \n", err);
	}
}
