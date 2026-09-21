# Ships & Port platforms access synchronization processes - C project

Il mio progetto, programmato prevalentemente in inglese(per preferenza personale) e commentato in italiano, è programmato esclusivamente in c(non contiene altri programmi o script vari in altri linguaggi) ed è costituito da 3 programmi eseguibili:
- `master.c`
- `porto.c`
- `nave.c`
  
2 moduli contenenti funzioni necessarie e dati necessari a coloro che li usano:
- `module.h`(con la relativa implementazione.c)
- `set_module.h`(con la relativa implementazione.c) 
e un `Makefile`.

Ho previsto dei limiti tra i processi nave, porto e master, e questi sono:
- I processi nave non possono leggere informazioni ne di altri navi 	e ne dei porti e i porti 	non possono leggere informazioni ne di 	altri porti ne di navi, hanno in comune la 	struttura merch in cui 	sono presenti informazioni sulle merci reperibili.

### Generazione merci(basata su lotti)
(L'Assegnazione di merci in tutto il progetto è basata sui lotti ma tiene comunque presente di tutti i requisiti richiesti riguardo alla soddisfazione della merce come tonnellate)-> & le navi non supereranno in ogni caso SO_CAPACITY specificato in tonnellate.

` Valori negativi di sup_dem = richiesta `

` Valori positivi di sup_dem = offerta `

----------------------------------------------------------------------------
(Invariante di codice) X ogni tipo di merce:

	 l_ton x sup_dem positivi(di ogni porto) = SO_FILL
	
	 l_ton x sup_dem negativi(di ogni porto) = SO_FILL 
----------------------------------------------------------------------------
Il punto principale del progetto, ovvero la generazione di merci, è gestita principalmente dal master, poichè ha sotto occhio tutti i porti e le navi sarà in grado di gestire l'assegnazione di tonnellate di merci ai vari porti in modo che l'offerta e la domanda soddisfino SO_FILL senza problemi di arrotondamento(problemi evitati grazie al calcolo di divisbilità tra l_ton e SO_FILL). 
La creazione di queste è gestita da str_merch che crea stringhe facilmente passabili ai porti come args che poi creano le loro strutture dati my_stock(contenenti info sulle merci).

### Comunicazione
Ho massimizzato il più possibile il grado di concorrenza fra processi e ho utilizzato diverse code di messaggi che mettono in comunicazione porti e navi (Ulteriori informazioni riguardo a queste sono presenti nei commenti del codice).

### Stampa e timing
La stampa avviene ogni giorno(secondo) nel master con segnali che vengono spediti con la alarm e il signal handler apposito che gestisce ogni giorno la stampa del report e la segnalazione ai porti di controllare merce possibilmente scaduta. Grazie alla variabile globale sec_done siamo in grado anche di terminare la simulazione dopo `SO_DAYS`. 

` NB: la domanda nel nostro caso non è mai uguale a 0 poichè abbiamo un campo aggiuntivo not_av con cui riusciamo a gestire la terminazione automatica della simulazione nel caso le navi non 	la stampa di info sulle navi avviene nonostante ci siano navi che abbiano già terminato lavoro(nave in mare senza carico). `

### Lettura info
L'identificativo dei porti e navi è il proprio pid. Le navi hanno `status = <0, 1, 2>` che sta per `<mare senza carico, mare con carico, in porto>`.
Il porto ha una struttura dati `STOCK` che lista informazioni delle merci presenti:
- `sup_dem`: offerta in positivo e domanda in negativo
- `ship_rec`: merce ricevuta in positivo e spedita in negativo
- `qt_reserved`:` (sup_dem > 0) -> quantità riservata da ricevere ` , `(sup_dem < 0) -> quantità riservata da spedire `
- `expired`: quantità che era in offerta ma adesso scaduta 
- `expired_ship`: quantità arrivata ma scaduta in nave
- `not_av`: uguale a `-1` -> indica che la merce richiesta non è reperibile
(Tutti questi campi vanno letti come lotti e non come tonnellate se si vuole risalire al valore di tonnellate moltiplicarli per l_ton del tipo di merce gradita)
