#include "module.h"
#include <math.h>

#define LOT_QT(capacity, id) capacity/merch[id].l_ton

int msg_id; /* msg queue id per comunicare con le navi */
struct msg_buf my_msg;
int shmid_myinfo; /* int id della shared memory contenente info del porto */  
stock *my_stock;
int sec_done = 0,
	sem_comm;  /* semafori per le banchine e sincronizzazione comunicazione con navi */
port *me;
merchandise *merch;

void signal_handler(int signum);

int *store_sup_dem(char* merch_string);

/* this is 'porto'(child of master), the program that simulates ports */
int main(int argc, char* argv[]){
	int 	i = 0,
		*value,
		sem_sync = atoi(argv[3]); /* sincronizzazione con le navi(evitare che iniziano il lavoro con dati inconsistenti) */
	struct sigaction sa;
	
	/* creazione shared memory per il porto (shm di ogni porto hanno chiave uguale al proprio pid per automatizzare la procedura) */
	shmid_myinfo = shmget(getpid(), sizeof(port), 0600 | IPC_CREAT);
	me = shmat(shmid_myinfo, NULL, 0);
	
	/* creazione shared memory per le merci del porto(stock) */
	me->shmid_stock = shmget(IPC_PRIVATE, sizeof(stock)*SO_MERCI, 0600 | IPC_CREAT);
	my_stock = shmat(me->shmid_stock, NULL, 0);
	
	/* inserimento info nella struttura dati del porto */
	me->id = getpid();
	me->posn.x = atoi(argv[1]);
	me->posn.y = atoi(argv[2]); 
	
	srand(me->id);

	/* creazione semaforo per gestione banchine */
	sem_comm = semget(me->id, 2, 0600 | IPC_CREAT);
	me->dock = rand()%SO_BANCHINE + 1; 
	semctl(sem_comm, 0, SETVAL, me->dock); /* banchine disponibili */
	semctl(sem_comm, 1, SETVAL, 1); /* sincronizzazione comunicazione con navi */
		
	/* ricezione stringa con info offerta domanda */
	value = malloc(SO_MERCI * sizeof(int));
	value = store_sup_dem(argv[5]);
	merch = shmat(atoi(argv[4]), NULL, 0);
	for(i = 0; i < SO_MERCI; i++){
		my_stock[i].id = i;
		my_stock[i].sup_dem = value[i];
	}	
	
	/* consumo risorse rilasciate da master */
	detain(sem_sync, 0);
	
	/* creazione message queue per la comunicazione con le navi */
	msg_id = msgget(me->id, 0600 | IPC_CREAT);
	
	/* definizione handler per la comunicazione con le navi */
	bzero(&sa, sizeof(sa));
	sa.sa_handler = signal_handler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL); /* SIGUSR1 -> richiesta di merci in domanda da portare */
	sigaction(SIGUSR2, &sa, NULL); /* SIGUSR2 -> richiesta di disponibilità merci domandate in offerta */ 
	sigaction(SIGURG, &sa, NULL); /* SIGURG -> richiesta di carico/scarico da parte delle navi */
	sigaction(SIGALRM, &sa, NULL); /* SIGALRM -> segnale per scadenza merce di un certo tipo */
	
	/* fine definizione informazioni porto(concedo risorsa) -> inizio simulazione */
	release(sem_sync, 1);
	
	/* fine simulazione -> rilascio risorse */
	RESET_OPERATION:
	while(detain(sem_sync, 2)){
		if(errno == EINTR)
			goto RESET_OPERATION;
		else break;
	};
	
	printf("Porto %i is terminating\n", getpid());
	/* deallocazione strutture dati condivise */ 
	msgctl(msg_id, IPC_RMID, NULL);
	semctl(sem_comm, 0, IPC_RMID);
	semctl(sem_comm, 1, IPC_RMID);
	shmctl(me->shmid_stock, IPC_RMID, NULL); 
	shmctl(shmid_myinfo, IPC_RMID, NULL);
	
	shmdt(me);
	shmdt(my_stock);
	shmdt(merch);
	
	return 0;
}

void signal_handler(int signum){ /* aggiornare tutti i supply e demand in sup dem */
	int i, k, index, flag, capacity, qt; /* ricordare di levare printf perchè AS unsafe */
	if(signum == SIGUSR1){ /* GESTIONE DOMANDA */
		msgrcv(msg_id, &my_msg, TEXT_SIZE, 1, 0); /* ricezione capacity nave */
		capacity = atoi(my_msg.mtext);
		CHECK_NEW:
		for(index = 0; index < SO_MERCI; index++)
			if(my_stock[index].not_av != -1 && my_stock[index].sup_dem < 0 && (abs(my_stock[index].sup_dem) - my_stock[index].qt_reserved) > 0) break;
		send(my_msg, msg_id, 2, index);
		if(index == SO_MERCI)
			return;
		flag = LOT_QT(capacity, index) < (abs(my_stock[index].sup_dem) - my_stock[index].qt_reserved);
		qt = flag ? LOT_QT(capacity, index) : abs(my_stock[index].sup_dem) - my_stock[index].qt_reserved;
		send(my_msg, msg_id, 3, qt);/* invio info quantità dovuta */
		msgrcv(msg_id, &my_msg, TEXT_SIZE, 4, 0);
		my_stock[index].qt_reserved += atoi(my_msg.mtext);
		if(!atoi(my_msg.mtext)){
			my_stock[index].not_av = -1; /* se la merce richiesta non è disponibile in offerta in nessun porto */
			goto CHECK_NEW;
		}
	}
	else if(signum == SIGUSR2){ /* GESTIONE OFFERTA */
		msgrcv(msg_id, &my_msg, TEXT_SIZE, 2, 0); /* ricezione dati su merce */
		index = atoi(my_msg.mtext);
		msgrcv(msg_id, &my_msg, TEXT_SIZE, 3, 0);
		qt = atoi(my_msg.mtext);
		flag = qt > (my_stock[index].sup_dem - my_stock[index].qt_reserved);
		qt = flag ? my_stock[index].sup_dem - my_stock[index].qt_reserved : qt;
		/*my_msg.mtype = 4;*/
		if(my_stock[index].not_av == -1)
			qt = 0;
		send(my_msg, msg_id, 4, qt);/* invio info quantità disponibili */
		if(qt <= 0)
			return;
		my_stock[index].qt_reserved += qt;
	}/* GESTIONE CARICO/SCARICO */
	else if(signum == SIGURG){
		msgrcv(msg_id, &my_msg, TEXT_SIZE, 5, 0);
		if(my_msg.mtext[0] == 'c'){
			/* GESTIONE CARICO */
			flag = my_msg.mtext[1] != 'x';
			msgrcv(msg_id, &my_msg, TEXT_SIZE, 2, 0);
			index = atoi(my_msg.mtext);
			msgrcv(msg_id, &my_msg, TEXT_SIZE, 6, 0);
			qt = atoi(my_msg.mtext);
			
			/* aggiunta messaggio per communicazione merce scaduta(qt_riservata non più disponibile) */
			send(my_msg, msg_id, 8, my_stock[index].qt_reserved);
			if(my_stock[index].qt_reserved){
				my_stock[index].sup_dem -= qt;
				my_stock[index].qt_reserved -= qt;
				my_stock[index].ship_rec -= qt;
			}
			if(flag)
				send(my_msg, msg_id, 7, merch[index].life);
		}
		else if(my_msg.mtext[0] == 's'){
			/* GESTIONE SCARICO */
			msgrcv(msg_id, &my_msg, TEXT_SIZE, 2, 0);
			index = atoi(my_msg.mtext);
			msgrcv(msg_id, &my_msg, TEXT_SIZE, 6, 0);
			qt = atoi(my_msg.mtext);
			
			if(sec_done < merch[index].life){
				my_stock[index].sup_dem += qt;
				my_stock[index].ship_rec += qt;
				my_stock[index].qt_reserved -= qt;
			}
			else my_stock[index].expired_ship += qt;
		}
	}
	else if(signum == SIGALRM){
		/* GESTIONE SCADENZA MERCI */
		/* ricezione messaggio con giorni passati(sec_done) */
		sec_done++;
		for(i = 0; i < SO_MERCI; i++){
			if(merch[i].life == sec_done){
				if(my_stock[i].sup_dem > 0){	
					my_stock[i].expired = my_stock[i].sup_dem;
					my_stock[i].qt_reserved = 0; 
					my_stock[i].sup_dem = 0;
				}
				else{
					my_stock[i].qt_reserved = 0;
					my_stock[i].not_av = -1;
				}
			}
		}
	}
}

int *store_sup_dem(char* merch_string){
	char* aux = malloc(9);
	int *value, i;
	value = malloc(SO_MERCI * sizeof(int));
	for(i = 0; i < SO_MERCI; i++){
		strncpy(aux, merch_string + (8*i), 8);
		value[i] = atoi(aux);
		if(!(aux[0] = '-'))
			value[i] = 0 - value[i];
	}
	return value;
}
