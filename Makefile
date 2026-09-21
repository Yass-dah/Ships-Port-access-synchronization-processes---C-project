FLAGS = -std=c89 -pedantic

TARGET = master

set_module.o: set_module.c module.h Makefile
	gcc $(FLAGS) -c set_module.c
	
module.o: module.c Makefile
	gcc $(FLAGS) -c module.c

nave.o: nave.c Makefile  
	gcc $(FLAGS) -c nave.c
	
nave: nave.o module.o Makefile
	gcc nave.o module.o -o nave -lm

porto.o: porto.c Makefile
	gcc $(FLAGS) -c porto.c

porto: porto.o module.o Makefile
	gcc porto.o module.o -o porto
	 
$(TARGET).o: $(TARGET).c nave porto Makefile
	gcc $(FLAGS) -c $(TARGET).c 
	
$(TARGET): $(TARGET).o module.o set_module.o Makefile
	gcc $(TARGET).o module.o set_module.o -o $(TARGET)
	
run: $(TARGET) nave porto
	./$(TARGET)

code:
	gedit Makefile module.h master.c porto.c nave.c &
	
cleanipc: 
	ipcrm -a
