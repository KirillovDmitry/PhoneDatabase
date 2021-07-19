CC=g++
CFLAGS1 = -std=c++11
CFLAGS2 = -lboost_thread -lboost_system -lpthread
SRV = ./server
CLN = ./client
LIB = ./lib

all: client server

client: $(CLN)/client.o
	$(CC) $(CFLAGS1) $(CFLAGS2) $(CLN)/client.o -o client.out

server: $(SRV)/record.o  $(SRV)/server.o
	$(CC) $(CFLAGS1) $(CFLAGS2) $(SRV)/server.o $(SRV)/record.o -o server.out

test: $(SRV)/record.o $(SRV)/test.o
	$(CC) $(CFLAGS1) $(CFLAGS2) $(SRV)/test.o $(SRV)/record.o -o $(SRV)/test

$(CLN)/client.o: $(CLN)/client.cpp $(LIB)/csv.h $(LIB)/httplib.h $(LIB)/join_threads.h $(LIB)/timer.h
	$(CC) $(CFLAGS1) $(CFLAGS2) -c $(CLN)/client.cpp -o $(CLN)/client.o

$(SRV)/server.o: $(SRV)/server.cpp $(SRV)/data.inl $(SRV)/data.h $(SRV)/thread_safe_map.h $(SRV)/thread_safe_map.inl $(SRV)/error.h $(SRV)/hash.h $(SRV)/record.o $(LIB)/csv.h $(LIB)/httplib.h $(LIB)/join_threads.h $(LIB)/timer.h
	$(CC) $(CFLAGS1) $(CFLAGS2) -c $(SRV)/server.cpp -o $(SRV)/server.o

$(SRV)/test.o: $(SRV)/test.cpp $(SRV)/data.inl $(SRV)/data.h $(SRV)/thread_safe_map.h $(SRV)/thread_safe_map.inl $(SRV)/error.h $(SRV)/hash.h $(SRV)/record.o
	$(CC) $(CFLAGS1) $(CFLAGS2) -c $(SRV)/test.cpp -o $(SRV)/test.o

$(SRV)/record.o: $(SRV)/record.cpp
	$(CC) $(CFLAGS1) -c $(SRV)/record.cpp -o $(SRV)/record.o

clean:
	rm -rf $(SRV)/*.o
	rm -rf $(CLN)/*.o
	rm -rf ./*.o
	