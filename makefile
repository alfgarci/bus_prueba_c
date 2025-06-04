all: principal servidor_ncurses cliente cliente_maleducado bus

principal: principal.c comun.h
	cc principal.c -o principal -Wall

servidor_ncurses: servidor_ncurses.c comun.c definiciones.h comun.h
	cc servidor_ncurses.c comun.c -o servidor_ncurses -lncurses -Wall

cliente: cliente.c comun.c comun.h
	cc cliente.c comun.c -o cliente -Wall

cliente_maleducado: cliente_maleducado.c comun.c comun.h
	cc cliente_maleducado.c comun.c -o cliente_maleducado -Wall

bus: bus.c comun.c comun.h 
	cc bus.c comun.c -o bus -Wall

