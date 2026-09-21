#include "module.h"
#include <time.h>
#include <aio.h>
#include <math.h>

#define DISTANCE(a, b) sqrt(pow(b.x-a.x, 2) + pow(b.y-a.y, 2))
#define TON(qt, id) qt * merch[id].l_ton

ship *me;
pid_t *porti;
port **porti_info;
merchandise *merch;
struct msg_buf my_msg; /* buffer per la coda di messaggi */

int shmid_myinfo, sem_sync;  /* int id della shared memory contenente info della nave */  

void change_posn(int status, double x, double y);

void travel(int index_destination);

void load(int index_port, int tons, int id_merce);

void signal_handler(int signum);

/* this is 'nave'(child of master), the program that simulate ships */
int main(int argc, char* argv[]){
	int 	i,
		j,
		k,
		aux,
		capacity = SO_CAPACITY,
		qt_due = 0,
		qt_avl = 0,
		id_merce,
		target,
		sem_comm, /* id semafori per comunicare con i porti */
		(*suppliers)[2], /* puntatore ad array contententi info sui fornitori [0]->pid fornitore [1]-> quantità fornibile */
		msg_id, /* msg queue id per comunicare con i porti */
		*shmid_porti; /* id shared memories dei vari porti */ 
	struct sigaction sa;
	merchandise *merch;
	loads my_load;
	
	sem_sync = atoi(argv[3]);
	
	/* creazione shared memory per il porto (shm di ogni porto hanno chiave uguale al proprio pid per automatizzare la procedura) */
	shmid_myinfo = shmget(getpid(), sizeof(ship), 0600 | IPC_CREAT);
	me = shmat(shmid_myinfo, NULL, 0);
	
	/* inserimento info nella struttura dati del porto */
	me->id = getpid();
	change_posn(1, atof(argv[1]), atof(argv[2])); /* settaggio posizione iniziale nave con stato */
	
	/* definizione handler per la comunicazione con le navi */
	bzero(&sa, sizeof(sa));
	sa.sa_handler = signal_handler;
	sigaction(SIGTERM, &sa, NULL); /* SIGALRM -> segnale per scadenza merce di un certo tipo */
	
	/* stampa info base nave */
	printf("Hi, i'm nave %i and this is my position: (%f, %f)\n", me->id, me->posn.x, me->posn.y);
	
	/* allocazione memoria per id shm porti e struttura dati per i porti */
	shmid_porti = malloc(sizeof(int) * SO_PORTI);
	porti_info = malloc(sizeof(port*) * SO_PORTI); 
	
	/* allocazione memoria e registrazione pid dei porti */
	porti = malloc(sizeof(pid_t) * SO_PORTI);
	porti[0] = atoi(argv[4]);
	for(i = 1; i <= SO_PORTI; i++)
		porti[i] = porti[0] + i; 
	
	/* allocazione shared memory info merci */
	merch = shmat(atoi(argv[5]), NULL, 0);
	
	/* shared memories contenenti info sui porti */		
	for(i = 0; i < SO_PORTI; i++){
		shmid_porti[i] = shmget(porti[i], sizeof(port), 0600 | IPC_CREAT);
		porti_info[i] = shmat(shmid_porti[i], NULL, SHM_RDONLY);
	}
	
	/* concedo risorsa */
	release(sem_sync, 0);
	
	/* inizio simulazione */
	DEAL:
	/* allocazione memoria per un ipotetico primo fornitore */	
	suppliers = malloc(2 * sizeof(int)); /* malloc: corrupted top size */
	for(i = 0; i < SO_PORTI && capacity; i++){
		/* ricezione id semaforo per sincronizzazione in sezione critica esterna */
		sem_comm = semget(porti[i], 2, 0600 | IPC_CREAT);
		/* consumo risorsa */
		detain(sem_comm, 1); /* processo  bloccato in sezione critica e non lascia la risorsa */
							/* SEZIONE CRITICA */
		kill(porti[i], SIGUSR1);
		/* ricezione id coda di messaggi per la comunicazione con il porto e invio messaggio(tipo 1) */
		msg_id = msgget(porti[i], 0600 | IPC_CREAT);
		send(my_msg, msg_id, 1, capacity);
	 	
	 	CHECK_NEW:
		msgrcv(msg_id, &my_msg, TEXT_SIZE, 2, 0);
		id_merce = atoi(my_msg.mtext);
		if(id_merce == SO_MERCI){
			/* rilascio risorsa */
			release(sem_comm, 1);
			continue;
		}
		msgrcv(msg_id, &my_msg, TEXT_SIZE, 3, 0);
		qt_due = atoi(my_msg.mtext);
		for(k = (i+1)%SO_PORTI, qt_avl = 0, j = 0; k != i; k = (k+1)%SO_PORTI){
			if(qt_due == qt_avl || !capacity)
				break;
			/* ricezione id semaforo per sincronizzazione in sezione critica interna */
			sem_comm = semget(porti[k], 2, 0600 | IPC_CREAT);
			/* consumo risorsa */
			detain(sem_comm, 1);
			
			msg_id = msgget(porti[k], 0600 | IPC_CREAT);
			kill(porti[k], SIGUSR2);
			send(my_msg, msg_id, 2, id_merce);
			send(my_msg, msg_id, 3, qt_due - qt_avl);
			msgrcv(msg_id, &my_msg, TEXT_SIZE, 4, 0);
			/* rilascio risorsa */
			release(sem_comm, 1);
			if(atoi(my_msg.mtext) <= 0)
				continue;
			qt_avl += atoi(my_msg.mtext);
			/* riallocazione memoria per un ipotetico prossimo fornitore */
			suppliers = realloc(suppliers, (j+1) * sizeof(double)); /* riallocazione memoria */
			suppliers[j][0] = k;
			suppliers[j++][1] = atoi(my_msg.mtext);	
		}
		/* ripristino id semaforo precedente per sincronizzazione in sezione critica esterna */
		sem_comm = semget(porti[i], 2, 0600 | IPC_CREAT);
		capacity -= qt_avl;
		msg_id = msgget(porti[i], 0600 | IPC_CREAT);
		send(my_msg, msg_id, 4, qt_avl);
							/* FINE SEZIONE CRITICA */
		if(qt_avl){
			target = i;
			/* rilascio risorsa */
			release(sem_comm, 1);
			break;
		}
		else goto CHECK_NEW;
	}
	
	if(i == SO_PORTI)
		goto END;
	
	for(i = 0; i < j; i++){
		/* viaggio per ritirare merce */
		travel(suppliers[i][0]);
		
		/* consumo risorsa (accesso a banchina porto fornitore) */
		sem_comm = semget(porti[suppliers[i][0]], 2, 0600 | IPC_CREAT);
		detain(sem_comm, 0);
		change_posn(2, porti_info[suppliers[i][0]]->posn.x, porti_info[suppliers[i][0]]->posn.y);
		
		/* consumo risorsa */
		detain(sem_comm, 1);
							/* SEZIONE CRITICA */
		/* invio segnale per accesso a banchina per carico merci */
		kill(porti[suppliers[i][0]], SIGURG);
		
		msg_id = msgget(porti[suppliers[i][0]], 0600 | IPC_CREAT);
		my_msg.mtype = 5;
		my_msg.mtext[0] = 'c'; 
		if(i > 0)
			my_msg.mtext[1] = 'x';
		msgsnd(msg_id, &my_msg, TEXT_SIZE, 0);
		send(my_msg, msg_id, 2, id_merce);
		send(my_msg, msg_id, 6, suppliers[i][1]);
		
		/* ricezione info su qt_reserved se ancora disponibile o scaduta */
		msgrcv(msg_id, &my_msg, TEXT_SIZE, 8, 0);
		if(!atoi(my_msg.mtext))
			goto RESET;	
		
		if(i == 0){
			/* creazione carico */
			my_load.id = id_merce;
			msgrcv(msg_id, &my_msg, TEXT_SIZE, 7, 0);
			my_load.life = atoi(my_msg.mtext);
		}
		
		/* carico merce */
		load(suppliers[i][0], TON(suppliers[i][1], id_merce), id_merce);
		
		/* incremento quantità del carico della merce 'id_merce' */
		my_load.status = 1;
		my_load.lot_qt += suppliers[i][1];
							/* FINE SEZIONE CRITICA */
		/* rilascio risorsa */
		release(sem_comm, 1);
		
		/* rilascio risorsa (abbandono banchina porto fornitore) */
		release(sem_comm, 0);
		me->status = 1;
	}
	
	/* viaggio per consegnare merce */
	travel(target);
	
	/* consumo risorsa (accesso a banchina porto destinatario) */
	sem_comm = semget(porti[target], 2, 0600 | IPC_CREAT);
	detain(sem_comm, 0);	
	change_posn(2, porti_info[target]->posn.x, porti_info[target]->posn.y);
	
	/* consumo risorsa */
	detain(sem_comm, 1);
							/* SEZIONE CRITICA */
	kill(porti[target], SIGURG);
	msg_id = msgget(porti[target], 0600 | IPC_CREAT);
	
	my_msg.mtype = 5;
	my_msg.mtext[0] = 's'; 
	msgsnd(msg_id, &my_msg, TEXT_SIZE, 0);
	
	send(my_msg, msg_id, 2, id_merce);
	
	send(my_msg, msg_id, 6, my_load.lot_qt);
	
	/* scarico merce */
	load(target, TON(my_load.lot_qt, my_load.id), id_merce);
	
	RESET:
	/* ripristino info */
	me->status = 0;
	capacity = SO_CAPACITY;
	free(suppliers);
	bzero(&my_load, sizeof(loads));
						/* FINE SEZIONE CRITICA */
	/* rilascio risorsa */
	release(sem_comm, 1);
		
	/* rilascio risorsa (abbandono banchina porto destinatario) */
	release(sem_comm, 0);
	goto DEAL; /* per ripetere la trattazione con le navi */
	END:
	
	/* rilascio risorsa */
	release(sem_sync, 0);

	raise(SIGTERM);
	/* PROBLEMA CON DEALLOCAZIONE MEMORIA CONDIVISA NEL CASO DI TERMINAZIONE AUTOMATICA */
}

void change_posn(int status, double x, double y){
	me->status = status;	
	me->posn.x = x;
	me->posn.y = y;
}

void travel(int index_dest){
	struct timespec move;
	/* inserimento valori della timespec move */
	move.tv_sec = DISTANCE(me->posn, porti_info[index_dest]->posn)/SO_SPEED;
	move.tv_nsec = ((DISTANCE(me->posn, porti_info[index_dest]->posn)/SO_SPEED) - move.tv_sec)*1000000000;	
 	/* dormita processo con la nanosleep(viaggio dai fornitori) */
	nanosleep(&move, NULL);
}

void load(int index_port, int tons, int id_merce){
	struct timespec move;
	/* dormita processo con la nanosleep(scarico merce) */
	move.tv_sec = (tons/SO_LOADSPEED);
	move.tv_nsec = ((double)tons/(double)SO_LOADSPEED-move.tv_sec)*1000000000;
	nanosleep(&move, NULL);
}

void signal_handler(int signum){
	int i;
	if(signum == SIGTERM){
		/* rilascio risorsa */
		release(sem_sync, 0);
		/* scollegamento shm */
		for(i = 0; i < SO_PORTI; i++)
			shmdt(porti_info[i]);
		shmdt(me);
		shmdt(merch);
		raise(SIGINT);
	}
}

