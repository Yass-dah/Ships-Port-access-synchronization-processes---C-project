#ifndef _SET_MODULE_H_
#define _SET_MODULE_H_

/* Configurazione principale per i parametri: Variabili d'ambiente.
   funzione per leggere i parametri da stdin e memorizzarli in variabili d'ambiente */
void set_parameters();
/* funzione per eliminare variabili d'ambiente precedentemente allocate */
void unset_parameters();
/* funzione per alloccare array contenente variabili d'ambiente(parametri) */
void set_envp_array(char* envp[]);
/* funzione per generare offerta e domanda merci come stringa */
char **str_merch(int l_ton[]);

#endif
