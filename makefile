###########################################################################
#     Modules PI
#     Copyright 2016 by Daniele De Marchi <demarchidaniele at gmail.com>
#   
#     Please refer to the GNU GPL V3 license for conditions
# 
##########################################################################

all : readDHT readTX23 readBMP180 readAS3935

clean :
	rm -f readDHT readTX23 readBMP180 readAS3935

install:
	cp ./readDTH /usr/local/bin
	cp ./readTX23 /usr/local/bin
	cp ./readBMP180 /usr/local/bin
	cp ./readAS3935 /usr/local/bin

readDHT : readDHT.c  shmem.c shmem.h daemon.c daemon.h 
	gcc -UDEBUG readDHT.c shmem.c daemon.c -o readDHT -lbcm2835 -lrt

readTX23 : readTX23.c  shmem.c shmem.h daemon.c daemon.h
	gcc -UDEBUG readTX23.c  shmem.c daemon.c -o readTX23  -lbcm2835 -lrt -lm

readBMP180 : readBMP180.c  shmem.c shmem.h daemon.c daemon.h
	gcc -UDEBUG readBMP180.c shmem.c daemon.c -o readBMP180 -lbcm2835 -lrt

readAS3935 :  
	gcc -UDEBUG readAS3935.c shmem.c daemon.c -o readAS3935 -lbcm2835 -lrt

