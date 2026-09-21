#ifndef _MODULE_H_
#define _MODULE_H_

#define _GNU_SOURCE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/msg.h>
#include <signal.h>

#define QT_PMT 13 /* quantità parametri */

/* etichette per avere parametri */
#define SO_LATO atoi(getenv("SO_LATO(km)"))
#define SO_PORTI atoi(getenv("SO_PORTI"))
#define SO_NAVI atoi(getenv("SO_NAVI"))
#define SO_MERCI atoi(getenv("SO_MERCI"))
#define SO_SIZE atoi(getenv("SO_SIZE(ton)"))
#define SO_MIN_VITA atoi(getenv("SO_MIN_VITA(dd)"))
#define SO_MAX_VITA atoi(getenv("SO_MAX_VITA(dd)"))
#define SO_SPEED atoi(getenv("SO_SPEED(km/dd)"))
#define SO_CAPACITY atoi(getenv("SO_CAPACITY(ton)"))
#define SO_BANCHINE atoi(getenv("SO_BANCHINE"))
#define SO_FILL atoi(getenv("SO_FILL(ton)")) 
#define SO_LOADSPEED atoi(getenv("SO_LOADSPEED(ton/dd)"))
#define SO_DAYS atoi(getenv("SO_DAYS(dd)"))

#define STR(x) #x

enum status_ship {mare_vuota, mare_carico, porto};
			
typedef struct merchandise{
	int l_ton;
	int life;
} merchandise;

typedef struct loads{
	int id;
	int lot_qt;
	int life;
	char status;
} loads;

typedef struct position{
	double x;
	double y;
} position;

typedef struct stock{
	int id;
	int sup_dem; /* offerta e domanda saranno un unico campo(variabile) che può avere sia valori positivi(offerta) che negativi(domanda) */
	int ship_rec;
	int qt_reserved;
	int expired;
	int expired_ship;
	char not_av;
} stock;

typedef struct port{
	int id;
	position posn;
	int dock;
	int shmid_stock;
} port;

typedef struct ship{
	int id;
	position posn;
	char status;
	loads load;
} ship;

#define TEXT_SIZE 45

struct msg_buf {
	long mtype; /* [1]capacity [2]id merce [3]qt_domanda [4]qt_offerta [5]flag [6]qt_carico [7]peso lotto [8]qt_reserved */            
	char mtext[TEXT_SIZE];    
};

void send(struct msg_buf my_msg, int msg_id, int type, int value);

int detain(int sem_id, int num);

int release(int sem_id, int num);
    
#endif
