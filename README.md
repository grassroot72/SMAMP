# Simple Microservice Architecture using Majordomo Protocol

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

### Majordomo Pattern - Service-Oriented Reliable Queuing

<img src="./Screenshot/Majordomo.jpg" width="480px">

Majordomo involves 3 parts, a client, a broker and a worker. It is a 
Service-Oriented Reliable Queuing protocol. It adds a "service name" to
requests that the client sends, and asks workers to register for specific
services. The broker, as its name implies, takes requests from clients and
forwards the requests to respective workers with specific "service names". 


<img src="./Screenshot/Titanic.jpg" width="480px">
