#include "module.h"
#include "set_module.h"
			
/* this is 'master'(parent of nave and porto), the main program that manage the simulation */
char* nave_args[] = {"nave", NULL, NULL, NULL, NULL, NULL};/* [1]=x_pos [2]=y_pos [3]=sem_print [4]=porti[0](pid) [5]= key_merch*/
char* porto_args[] = {"porto", NULL, NULL, NULL, NULL, NULL};/* [1]=x_pos [2]=y_pos [3]=sem_sync [4]= key_merch [5]= lotti merce assegnati*/ 
char* envp[QT_PMT]; /* array contenente 9variabili d'ambiente formattate per essere passate ai figli di master */

merchandise *merch;
struct msg_buf my_msg;
int sec_done = 0,
	key_merch,
	sem_sync, /* semaforo per sincronizzazione tra navi e porti */
	*id_shm_porti,/* array contenente ID delle shared memories delle info sui porti */
	*shmid_nave;
pid_t  *porti, *navi;

/* funzione per controllare se una certa posizione è già stata generata per un porto */
char occurred(position *array, int index);
/* stampa info */
void print_stats();
/* handler per i vari segnali(visualizzazione dati) */
void signal_handler(int signum);

int main(){
	int 	i, 
		j,
		value,
		*tons_lot; /* array contenente tonnellate per lotto di ogni merce */
	char **string_merch;
	struct sigaction sa;
	struct sembuf my_op;
	position *porti_posn, *navi_posn;
	pid_t aux;
	port *porto = malloc(sizeof(port));
	stock *stock;
	
	/* creazione variabili d'ambiente per memorizzare parametri e l'array per il passaggio di quest'ultimi ai figli */
	set_parameters();
	set_envp_array(envp);
	
	/* allocazione memoria per le varie strutture dati dopo che sono stati forniti i parametri */
	porti_posn = malloc(sizeof(position) * SO_PORTI);
	navi_posn = malloc(sizeof(position) * SO_NAVI); 
	porti = malloc(sizeof(pid_t) * SO_PORTI);
	navi = malloc(sizeof(pid_t) * SO_NAVI);
	shmid_nave = malloc(sizeof(int) * SO_NAVI);
	
	srand(time(NULL));
	
	/* allocazione info merci come shared memory */
	tons_lot = malloc(sizeof(int)*SO_MERCI);
	key_merch = shmget(IPC_PRIVATE, SO_MERCI * sizeof(merch), 0600 | IPC_CREAT);
	merch = shmat(key_merch, NULL, 0);
	for(i = 0; i < SO_MERCI; i++){
		do
    			value = rand()%SO_SIZE+1;
    		while(SO_FILL%value != 0);
    		merch[i].l_ton = tons_lot[i] = value;
		merch[i].life = rand()%(SO_MAX_VITA-SO_MIN_VITA+1) + SO_MIN_VITA;
	}
	/* generazione stringa con info merci, ovvero argv[5] dei porti */
	string_merch = malloc(SO_PORTI * sizeof(char*));
	string_merch = str_merch(tons_lot);
	
	printf("-=-=-=-MERCI-=-=-=-\n");
	printf("id\tlot_ton\tlife\n");
	for(i = 0; i < SO_MERCI; i++)
		printf("%i\t%i\t%i\n", i, merch[i].l_ton, merch[i].life);
	
	/* creazione semafori */
	sem_sync = semget(IPC_PRIVATE, 3, 0600); 
	semctl(sem_sync, 0, SETVAL, 0); /* start sync */
	semctl(sem_sync, 1, SETVAL, 0); /* start sync */
	semctl(sem_sync, 2, SETVAL, 0); /* esclusivamente per la RESET_OPERATION */
	
	/* definizione handler per la comunicazione con le navi */
	bzero(&sa, sizeof(sa));
	sa.sa_handler = signal_handler; /* settare sa_flags = SA_RESTART in caso di problemi */
	sa.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &sa, NULL); /* SIGALRM -> segnale per scadenza merce di un certo tipo */
	
	/* inizializzazione flag struct dell'operazione su semafori */
	my_op.sem_flg = 0;
	
	/* allocazione memoria per gli argomenti da passare ai porti */ 
	for(i = 1; i <= 4; i++)
		porto_args[i] = malloc(16);
	sprintf(porto_args[3], "%i", sem_sync);
	sprintf(porto_args[4], "%i", key_merch);
	
	/* creazione SO_PORTI processi porto */
	i = 0;
	while(i < SO_PORTI){
		do{
			porti_posn[i].x = rand()%SO_LATO; 
			porti_posn[i].y = rand()%SO_LATO; /* Port Position */ 
		}while(occurred(porti_posn, i));
		sprintf(porto_args[1], "%f", porti_posn[i].x);
		sprintf(porto_args[2], "%f", porti_posn[i].y);
		porto_args[5] = string_merch[i];
		if(!(porti[i++] = fork()))
			execve("porto", porto_args, envp);
	}
	
	/* settaggio operazione su semaforo risorse porti(1> bloccante) */
	printf("Attesa fine creazione porti...\n");
	srand(time(NULL));
	
	semctl(sem_sync, 0, SETVAL, SO_PORTI);
	
	printf("Creazione navi...\n");
	/* settaggio operazione su semaforo risorse porti(1> bloccante) */
	my_op.sem_num = 1;
	my_op.sem_op = -1;
	for(i = 0; i < SO_PORTI; i++)
		semop(sem_sync, &my_op, 1); 
	
	printf("INIZIO SIMULAZIONE...\n");
	
	/* allocazione memoria per gli args dei childs nave */
	for(i = 1; i <= 5; i++)
		nave_args[i] = malloc(16); 
	sprintf(nave_args[3], "%i", sem_sync);
	sprintf(nave_args[4], "%i", porti[0]); /* nave_args[3] avrà il pid del primo processo nave */
	sprintf(nave_args[5], "%i", key_merch);
	
	/* creazione SO_NAVI processi nave */
	i = 0;
	while(i < SO_NAVI){
		navi_posn[i].x = rand()%SO_LATO + rand()%10/10.0 + rand()%10/100.0;
		navi_posn[i].y = rand()%SO_LATO + rand()%10/10.0 + rand()%10/100.0; 
		sprintf(nave_args[1], "%f", navi_posn[i].x);
		sprintf(nave_args[2], "%f", navi_posn[i].y);
		if(!(navi[i++] = fork()))
			execve("nave", nave_args, envp);
	}
	
	/* settaggio operazione su semaforo risorse navi(1> bloccante) */
	my_op.sem_num = 0;
	for(i = 0; i < SO_NAVI; i++)
		semop(sem_sync, &my_op, 1);
	
	for(i = 0; i < SO_NAVI; i++)
		shmid_nave[i] = shmget(navi[i], 0, 0);
	raise(SIGALRM);
	
	/* attesa che tutti i processi nave finiscano */
	for(i = 0; i < SO_NAVI; i++){
		aux = waitpid(navi[i], NULL, 0);
		if(errno == EINTR)
			i--;
	}
	printf("Processi nave terminati.\n");
		
	/* incremento risorse disponibili(porti) */
	semctl(sem_sync, 2, SETVAL, SO_PORTI);

	/* attesa che tutti i processi porto finiscano */
	for(i = 0; i < SO_PORTI; i++)
		aux = waitpid(porti[i], NULL, 0);
	printf("Processi porto terminati.\n");
		
	/* deallocazione semafori */
	semctl(sem_sync, 0, IPC_RMID); 
	semctl(sem_sync, 1, IPC_RMID); 
	shmctl(key_merch, 0, IPC_RMID);
	
	for(i = 0; i < SO_NAVI; i++)
		shmctl(shmid_nave[i], IPC_RMID, NULL);
	
	shmdt(merch);
	
	unset_parameters(); /* eliminazione variabili d'ambiente(parametri) */
	printf("Simulazione terminata.\n");
	return 0;
}

char occurred(position *array, int index){
	int i = 0;
	for(i = 0; i < index; i++)
		if(array[i].x == array[index].x && array[i].y == array[index].y) return 1;
	return 0;
}

void print_stats(){
	int i, j, shmid_porto, banchine;
	stock *stock = malloc(sizeof(stock) * SO_MERCI);
	ship *nave = malloc(sizeof(ship));
	port *porto = malloc(sizeof(port));
	sec_done++;
	/* stampa */
	printf("-=-=-=- SHIPS -=-=-=-\n");
	printf("id\t\tstatus\n");
	for(i = 0; i < SO_NAVI; i++){
		nave = shmat(shmid_nave[i], NULL, SHM_RDONLY);
		printf("%i\t\t%i\n", navi[i], nave->status);
	}
	printf("PORTI: \n");
	for(i = 0; i < SO_PORTI; i++){
		shmid_porto = shmget(porti[i], sizeof(port), 0600 | IPC_CREAT);
		porto = shmat(shmid_porto, NULL, SHM_RDONLY);
		stock = shmat(porto->shmid_stock, NULL, SHM_RDONLY);
		banchine = semget(porto->id, 2, 0600 | IPC_CREAT);
		printf("-=-=-=-=-=-=-=-=-=-=-=-=-=- PORTO %i(%f, %f) -=-=-=-=-=-=-=-=-=-=-=-=-=-\n", porto->id, porto->posn.x, porto->posn.y);
		printf("id merce\toff/dom\t\tship/rec\texpired\t\texpired_ship\n");
		for(j = 0; j < SO_MERCI; j++){
			printf("%i\t\t%i\t\t%i\t\t", stock[j].id, stock[j].sup_dem, stock[j].ship_rec);
			printf("%i\t\t%i\n", stock[j].expired, stock[j].expired_ship);
		}
		printf("BANCHINE DISPONIBILI: %i  TOTALI: %i\n", semctl(banchine, 0, GETVAL), porto->dock); /* wait until the add of a banchine variable in port */
		printf("DAY %i\n", sec_done);
	}
}

void signal_handler(int signum){
	/* settaggio signal handler per segnale SIGALRM */
	if(signum = SIGALRM){
		int i, j;
		print_stats();
		for(i = 0; i < SO_PORTI; i++){
			if(sec_done <= SO_MAX_VITA)
				kill(porti[i], SIGALRM);
		}
		/* Termine simulazione */ 
		if(sec_done >= SO_DAYS){
			for(i = 0; i < SO_NAVI; i++)
				kill(navi[i], SIGTERM);
		}
		else alarm(1);
	}
}
