# Simple Microservice Architecture using Majordomo Protocol (SMAMP)

This is a PoC(Proof of Concept) project to show that microservices can be
achieved using ZeroMQ's Majordomo Pattern.


## Background Knowledge about the Majordomo Pattern of ZeroMQ

Before starting to introduce Majordomo Pattern, let's see what ZeroMQ can do.

ZeroMQ is a messaging library, which allows you to design a complex communication system
without much effort.

It has the following features:

  - Connect your code in any language, on any platform
  - Carries messages across inproc, IPC, TCP, TIPC, multicast
  - Smart patterns like pub-sub, push-pull, and router-dealer
  - High-speed asynchronous I/O engines, in a tiny library
  - Backed by a large and active open source community
  - Supports every modern language and platform
  - Build any architecture: centralized, distributed, small, or large

Some well-known companies are using ZeroMQ, for example, AT&T, Cisco, EA, NASA,
Samsung Electronics, Microsoft.

### *Majordomo Pattern - Service-Oriented Reliable Queuing*
<img src="./Screenshot/Majordomo.jpg" width="480px">

Majordomo involves 3 parts, a client, a broker and a worker. It is a
Service-Oriented Reliable Queuing protocol. It adds a "service name" to
requests that the client sends, and asks workers to register for specific
services. The broker, as its name implies, takes requests from clients and
forwards the requests to the respective workers with the specific
"service name".

### *Titanic Pattern - Disconnected Reliability*
<img src="./Screenshot/Titanic.jpg" width="480px">

Majordomo is a "reliable" message broker, but in reality, for all the
enterprise messaging systems, persistence is a 'must' feature. However,
performance drops a lot due to this persistence feature, it should be
acknowledged accordingly.

Titanic Pattern can be treated as Majordomo Pattern with persistence.
The office ZeroMQ guide only includes a Titanic Pattern example for ZeroMQ 3.x,
which is obsolete. Here in this PoC project, I implemented Titanic Pattern
based on ZeroMQ 4.x version.

## SMAMP
<img src="./Screenshot/SMAMP.jpg" width="480px">

SMAMP uses 2 brokers. One of them forwards the clients' requests to application
workers, and the other connects to the both application workers and DB workers.
In SMAMP, the application broker acts as an ***application layer*** which dispatches
the specific requests to the respective workers which provide specific microservices.
The DB broker acts as an ***infrastructure layer*** which relays the requests from
the microservices to the SQL or NoSQL workers.

SMAMP aims to align with the **Domain-Driven Design** principle. In this PoC project,
I use a "MM service" to simulate some functions in the Material Management Module
of the ERP software. I use MongoDB as the database server which is connected to
the "NoSQL service" worker. Inside the "MM service", the **CRUD** operations of
MongoDB are implemented.

### *Feature Extending*

It is quite flexible to add more features to SMAMP.

  - Apply Titanic Pattern to microservices or DB services or to both.
  - Add Redis as cache to microservices or DB services or to both.

### *Performance Turning*

As the **X-Axis** of **AFK Scale Cube** suggested, to improve performance of SMAMP,
multiple workers which provide the same microservices can be put up to balance the
workload.

In addition to use multiple workers, Redis server can be added as the cache.
For DB services, Redis server can also be applied to improve **read** performace.

Another way of improving performance is to dispatch **read** and **write** requests
to the respective workers through "**read**" and "**write**" brokers.


If Titanic Pattern is used in SMAMP, some recommendations can be adopted.

Pieter Hintjens gave the following suggestions,

>*If you want to use Titanic in real cases, you'll rapidly be asking
"how do we make this faster?"*

>*Here's what I'd do, starting with the example implementation:*

>  - Use a single disk file for all data, rather than multiple files. Operating
systems are usually better at handling a few large files than many smaller ones.
>  - Organize that disk file as a circular buffer so that new requests can be
written contiguously (with very occasional wraparound). One thread, writing full
speed to a disk file, can work rapidly.
>  - Keep the index in memory and rebuild the index at startup time, from the
disk buffer. This saves the extra disk head flutter needed to keep the index
fully safe on disk. You would want an fsync after every message, or every N
milliseconds if you were prepared to lose the last M messages in case of a system
failure.
>  - Use a solid-state drive rather than spinning iron oxide platters.
>  - Pre-allocate the entire file, or allocate it in large chunks, which allows
the circular buffer to grow and shrink as needed. This avoids fragmentation and
ensures that most reads and writes are contiguous.

>*And so on. What I'd not recommend is storing messages in a database,
not even a "fast" key/value store, unless you really like a specific database
and don't have performance worries. You will pay a steep price for the
abstraction, ten to a thousand times over a raw disk file.*

>*If you want to make Titanic even more reliable, duplicate the requests to
a second server, which you'd place in a second location just far away enough
to survive a nuclear attack on your primary location, yet not so far that you
get too much latency.*

>*If you want to make Titanic much faster and less reliable, store requests
and replies purely in memory. This will give you the functionality of a
disconnected network, but requests won't survive a crash of the Titanic
server itself.*

Apart from what Pieter suggested, optimising the code to manipulate linux
Page Cache can also be considered to improve the IO performace. This technique
is already used in Apache Kafka.

## Demo Environment

The demo environment requires a MongoDB database. In this PoC project, the MongoDB
replica set is used. The replica set is created using Docker.

3 containers are used in this MongoDB replica set architecture.
<img src="./Screenshot/MongoDB_replica_set.png" width="480px">


### *Setting up the network*

To see the current network, run the command:

```
$ docker network ls
NETWORK ID          NAME                DRIVER              SCOPE
8ea1387e2450        bridge              bridge              local
d88d9e592f04        host                host                local
328759274a19        none                null                local
```

In this PoC project, I create the network for replica set using the following command:

```
$ docker network create my-mongo-cluster
```

The new network should now be added to the list of networks:

```
$ docker network ls
NETWORK ID          NAME                DRIVER              SCOPE
8ea1387e2450        bridge              bridge              local
d88d9e592f04        host                host                local
462893ca237e        my-mongo-cluster    bridge              local
328759274a19        none                null                local
```

### *Setting up containers*

To start up the first container, mongo1, run the command:

```
$ docker run \
-p 30001:27017 \
--name mongo1 \
--net my-mongo-cluster \
mongo mongod --replSet my-mongo-set
```

The other 2 containers,

```
$ docker run \
-p 30002:27017 \
--name mongo2 \
--net my-mongo-cluster \
mongo mongod --replSet my-mongo-set

$ docker run \
-p 30003:27017 \
--name mongo3 \
--net my-mongo-cluster \
mongo mongod --replSet my-mongo-set
```

### Setting up replication

Connect to the mongo shell in any of the containers, ex, mongo1.
In the mongo shell, create the configuration first,

```
MongoDB shell version v4.0.10
> db = (new Mongo('localhost:27017')).getDB('mydb')
mydb
> config = {
  	"_id" : "my-mongo-set",
  	"members" : [
  		{
  			"_id" : 0,
  			"host" : "mongo1:27017"
  		},
  		{
  			"_id" : 1,
  			"host" : "mongo2:27017"
  		},
  		{
  			"_id" : 2,
  			"host" : "mongo3:27017"
  		}
  	]
  }
```

Then, start the replica set by the follow command:

```
> rs.initiate(config)
{ "ok" : 1 }
```

### *Running the Demo*

First, jump into the SMAMP directory and start the **Application Broker**,

```
$ ./mdp_broker
```

Then, start the **DB Broker**,

```
$ ./mdp_broker tcp://*:8888
```

Then, start the **mongodb_worker** and maintain the connection to the MongoDB database,

```
$ ./mongodb_worker
```

and start **mm_worker** (multiple mm_workers can be started here).

```
$ ./mm_worker
```

Now start the **mm_client** to test the SMAMP, 

```
$ ./mm_client
```

The requests are sent from the **mm_client** through the **Application Broker** to the
**mm_worker(s)**, the **mm_worker(s)** translate(s) the requests to MongoDB queries
and sent them to the **mongodb_worker** through the **DB Broker**.

The response path contains all the stops in the request path, but in the reverse order.

Finally, some successful message should appear in the console where the **mm_client**
is executed.

