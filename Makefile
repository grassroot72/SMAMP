# SMAM makefile

CC = gcc
CFLAGS = -O2 -Wall `pkg-config --cflags libmongoc-1.0`
LDFLAGS = -lzmq -lczmq -luuid `pkg-config --libs libmongoc-1.0`

BROKER_OBJS = mdp_broker.o
MM_WORKER_OBJS = mdp_worker.o mdp_client.o mm_worker.o
MM_CLIENT_OBJS = mdp_client.o mm_client.o
MONGODB_WORKER_OBJS = mdp_worker.o mongodb_worker.o
TITANIC_OBJS = mdp_worker.o mdp_client.o titanic.o
TICLIENT_OBJS = mdp_client.o ticlient.o

BROKER_EXE = mdp_broker
MM_WORKER_EXE = mm_worker
MM_CLIENT_EXE = mm_client
MONGODB_WORKER_EXE = mongodb_worker
TITANIC_EXE = titanic
TICLIENT_EXE = ticlient


all: $(BROKER_EXE) $(MM_WORKER_EXE) $(MM_CLIENT_EXE) $(TITANIC_EXE) $(TICLIENT_EXE) $(MONGODB_WORKER_EXE)

%.o: %.c
	$(CC) -c $< $(CFLAGS)

$(BROKER_EXE): $(BROKER_OBJS)
	$(CC) -o $@ $(BROKER_OBJS) $(LDFLAGS)

$(MM_WORKER_EXE): $(MM_WORKER_OBJS)
	$(CC) -o $@ $(MM_WORKER_OBJS) $(LDFLAGS)

$(MM_CLIENT_EXE): $(MM_CLIENT_OBJS)
	$(CC) -o $@ $(MM_CLIENT_OBJS) $(LDFLAGS)

$(MONGODB_WORKER_EXE): $(MONGODB_WORKER_OBJS)
	$(CC) -o $@ $(MONGODB_WORKER_OBJS) $(LDFLAGS)

$(TITANIC_EXE): $(TITANIC_OBJS)
	$(CC) -o $@ $(TITANIC_OBJS) $(LDFLAGS)

$(TICLIENT_EXE): $(TICLIENT_OBJS)
	$(CC) -o $@ $(TICLIENT_OBJS) $(LDFLAGS)

clean:
	rm -f *.o $(BROKER_EXE) $(MM_WORKER_EXE) $(MM_CLIENT_EXE) $(MONGODB_WORKER_EXE) $(TITANIC_EXE) $(TICLIENT_EXE)
