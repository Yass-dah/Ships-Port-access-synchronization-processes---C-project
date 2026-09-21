#include "set_module.h"
#include "module.h"

char* parameters[] = {  "SO_LATO(km)", "SO_PORTI", "SO_NAVI", 
			"SO_MERCI", "SO_SIZE(ton)", "SO_MIN_VITA(dd)", 
			"SO_MAX_VITA(dd)", "SO_SPEED(km/dd)", "SO_CAPACITY(ton)", 
			"SO_BANCHINE", "SO_FILL(ton)", "SO_LOADSPEED(ton/dd)", "SO_DAYS(dd)"}; /* parametri */

void set_parameters(){
	int i = 0;
	char* value = malloc(sizeof(int));
	while(i < QT_PMT){
		printf("%s: ", parameters[i]); 
		scanf("%s", value);
		setenv(parameters[i++], value, 1);
	}
}

void unset_parameters(){
	int i = 0;
	while(i < QT_PMT)
		unsetenv(parameters[i++]);
}

void set_envp_array(char* envp[]){
	int i = 0;
	while(i < QT_PMT){
		envp[i] = malloc(25);
		strcpy(envp[i], parameters[i]); 
		strcat(envp[i], "=");
		strcat(envp[i], getenv(parameters[i]));
		i++;
	}
}

char **str_merch(int l_ton[]){
	int i, j, k, value = 0, *rem_sup, *rem_dem; 
    	char **str = malloc(SO_PORTI * sizeof(char*)), *aux = malloc(9);
    	
    	rem_sup = malloc(SO_MERCI*sizeof(int));
    	rem_dem = malloc(SO_MERCI*sizeof(int));
    	for(i = 0; i < SO_MERCI; i++)
        	rem_sup[i] = rem_dem[i] = SO_FILL/l_ton[i];

    	for(i = 0; i < SO_PORTI; i++){
    		str[i] = malloc(8 * SO_MERCI + 1);
    		for(j = 0; j < SO_MERCI; j++){
    			if(i == SO_PORTI-2 && rem_sup[j]){
    				value = rem_sup[j];
    				rem_sup[j] = 0;
    				strcpy(aux, "00000000");
    			}
    			else if(i == SO_PORTI-1 && rem_dem[j]){
    				value = rem_dem[j];
    				rem_dem[j] = 0;
    				strcpy(aux, "-0000000");
    			}
    			else{
    				value =  (rand()%(((SO_FILL/l_ton[j])/SO_PORTI)*8+3))-(((SO_FILL/l_ton[j])/SO_PORTI)*4+1);		
    				if(value < 0){
    					value = value < -rem_dem[j] ? rem_dem[j] : -value;
    					rem_dem[j] -= value;
    					strcpy(aux, "-0000000");
    				}	
    				else{
    					value = value > rem_sup[j] ? rem_sup[j] : value;
    					rem_sup[j] -= value;
    					strcpy(aux, "00000000");
    				}
    			}
    			snprintf(aux + 8 - snprintf(NULL, 0, "%d", value), sizeof(aux), "%d", value);
    			strncpy(str[i] + (8*j), aux, 8); 
    		}
    	} 
    	return str;
}
