.phony all:
all: p2 

# station: station.c
# 	gcc station.c -o station

p2: p2.c station.c station.h
	gcc p2.c station.c -Wall -lm -pthread -o p2
	# gcc p2.c -Wall -pthread -o p2





.PHONY clean:
clean:
	-rm -rf *.o *.exe
