# Simple Microservice Architecture using Majordomo Protocol (SMAMP)

This is a PoC(Prove of Concept) project to show that microservices can be 
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
forwards the requests to the respective workers with specific "service name". 

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
In SMAMP, the application broker acts as a application layer which dispatches the
specific request to the respective workers which provide specific microservices.
The DB broker acts as a infrastructure layer which replays the requests from
the microservices to SQL or NoSQL workers.

SMAMP aims to align with the Domain-Driven Design principle. In this PoC project,
I use a "MM service" to simulate some functions in the Material Management Module
of the ERP software. I use MongoDB as the database server which is connected to
the "NoSQL service" worker. Inside the "MM service", the CRUD operations of
MongoDB are implemented.

### *Performance Turning*

 
