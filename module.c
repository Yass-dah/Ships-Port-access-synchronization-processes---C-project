#include "module.h"

void send(struct msg_buf my_msg, int msg_id, int type, int value){
	my_msg.mtype = type;
	snprintf(my_msg.mtext, sizeof(my_msg.mtext), "%i", value);
	msgsnd(msg_id, &my_msg, TEXT_SIZE, 0);
}

/* funzioni per semafori */
int detain(int sem_id, int num){
	struct sembuf my_op;
	my_op.sem_num = num;
	my_op.sem_op = -1;
	return semop(sem_id, &my_op, 1);
}

int release(int sem_id, int num){
	struct sembuf my_op;
	my_op.sem_num = num;
	my_op.sem_op = 1;
	return semop(sem_id, &my_op, 1);
}
