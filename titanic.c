/*
 * Titanic service (similar to Majordomo worker)
 * Implements server side of http://rfc.zeromq.org/spec:9
 */


#include "mdp.h"
#include "zfile.h"
#include <uuid/uuid.h>


#define TITANIC_DIR ".titanic"

/*
 * Return a new UUID as a printable character string
 * Caller must free returned string when finished with it
 */
static char *
s_generate_uuid(void)
{
  char hex_char[] = "0123456789ABCDEF";
  char *uuidstr = zmalloc(sizeof(uuid_t)*2 + 1);
  uuid_t uuid;
  uuid_generate(uuid);
  int byte_nbr;
  for (byte_nbr = 0; byte_nbr < sizeof(uuid_t); byte_nbr++) {
    uuidstr[byte_nbr*2 + 0] = hex_char[uuid[byte_nbr] >> 4];
    uuidstr[byte_nbr*2 + 1] = hex_char[uuid[byte_nbr] & 15];
  }
  return uuidstr;
}

/*
 * Returns freshly allocated request filename for given UUID
 */
static char *
s_request_filename (char *uuid) {
  char *filename = malloc(256);
  snprintf(filename, 256, TITANIC_DIR "/%s.req", uuid);
  return filename;
}

/*
 * Returns freshly allocated reply filename for given UUID
 */
static char *
s_reply_filename(char *uuid) {
  char *filename = malloc(256);
  snprintf(filename, 256, TITANIC_DIR "/%s.rep", uuid);
  return filename;
}

/*
 * .split Titanic request service
 * The {{titanic.request}} task waits for requests to this service. It writes
 * each request to disk and returns a UUID to the client. The client picks
 * up the reply asynchronously using the {{titanic.reply}} service:
 */
static void
titanic_request(zsock_t *pipe, void *args)
{
  mdp_worker_t *worker =
    mdp_worker_new("tcp://localhost:5555", "titanic.request", 0);

  zmsg_t *reply = NULL;
  zframe_t *reply_to;
  zsock_signal(pipe, 0);

  while (true) {
    zmsg_t *request = mdp_worker_recv(worker, &reply_to);
    if (!request) {
      break;      /* Interrupted, exit */
    }

    zmsg_t *req = zmsg_dup(request);
    char *service = zmsg_popstr(req);
    if (strcmp(service, "shutdown") == 0) {
      free(service);
      zmsg_destroy(&req);
      zmsg_destroy(&request);
      break;
    }

    /* Ensure message directory exists */
    zfile_mkdir(TITANIC_DIR);

    /* Generate UUID and save message to disk */
    char *uuid = s_generate_uuid();
    char *filename = s_request_filename(uuid);
    FILE *file = fopen(filename, "w");
    assert(file);
    zmsg_save(request, file);
    fclose(file);

    /* Send UUID through to message queue */
    reply = zmsg_new();
    zmsg_pushstr(reply, uuid);
    zmsg_send(&reply, pipe);

    /* Now send UUID back to client */
    reply = zmsg_new();
    zmsg_pushstr(reply, uuid);
    zmsg_pushstr(reply, "200");

    mdp_worker_send(worker, &reply, reply_to);

    zmsg_destroy(&request);
    free(uuid);
    free(filename);
  }

  zframe_destroy(&reply_to);
  mdp_worker_destroy(&worker);
}

/*
 * .split Titanic reply service
 * The {{titanic.reply}} task checks if there's a reply for the specified
 * request (by UUID), and returns a 200 (OK), 300 (Pending), or 400
 * (Unknown) accordingly:
 */
static void
titanic_reply(zsock_t *pipe, void *args)
{
  mdp_worker_t *worker =
    mdp_worker_new("tcp://localhost:5555", "titanic.reply", 0);

  zframe_t *reply_to;
  zmsg_t *reply = NULL;
  zsock_signal(pipe, 0);

  while (true) {
    zmsg_t *request = mdp_worker_recv(worker, &reply_to);
    if (!request) {
      break;      /* Interrupted, exit */
    }

    zmsg_t *req = zmsg_dup(request);
    char *service = zmsg_popstr(req);
    if (strcmp(service, "shutdown") == 0) {
      free(service);
      zmsg_destroy(&req);
      zmsg_destroy(&request);
      break;
    }

    char *uuid = zmsg_popstr(request);
    char *req_filename = s_request_filename(uuid);
    char *rep_filename = s_reply_filename(uuid);
    if (zfile_exists(rep_filename)) {
      FILE *file = fopen(rep_filename, "r");
      assert(file);
      reply = zmsg_load(file);
      zmsg_pushstr(reply, "200");
      fclose(file);
    }
    else {
      reply = zmsg_new();
      if (zfile_exists(req_filename))
        zmsg_pushstr(reply, "300"); /* Pending */
      else
        zmsg_pushstr(reply, "400"); /* Unknown */
    }

    mdp_worker_send(worker, &reply, reply_to);

    zmsg_destroy(&request);
    free(uuid);
    free(req_filename);
    free(rep_filename);
  }

  zframe_destroy(&reply_to);
  mdp_worker_destroy(&worker);
}

/*
 * .split Titanic close task
 * The {{titanic.close}} task removes any waiting replies for the request
 * (specified by UUID). It's idempotent, so it is safe to call more than
 * once in a row:
 */
static void
titanic_close(zsock_t *pipe, void *args)
{
  mdp_worker_t *worker =
    mdp_worker_new("tcp://localhost:5555", "titanic.close", 0);

  zframe_t *reply_to;
  zmsg_t *reply = NULL;
  zsock_signal(pipe, 0);

  while (true) {
    zmsg_t *request = mdp_worker_recv(worker, &reply_to);
    if (!request) {
      break;      /* Interrupted, exit */
    }

    zmsg_t *req = zmsg_dup(request);
    char *service = zmsg_popstr(req);
    if (strcmp(service, "shutdown") == 0) {
      free(service);
      zmsg_destroy(&req);
      zmsg_destroy(&request);
      break;
    }

    char *uuid = zmsg_popstr(request);
    char *req_filename = s_request_filename(uuid);
    char *rep_filename = s_reply_filename(uuid);
    zfile_delete(req_filename);
    zfile_delete(rep_filename);

    reply = zmsg_new();
    zmsg_pushstr(reply, "200");

    mdp_worker_send(worker, &reply, reply_to);

    zmsg_destroy(&request);
    free(uuid);
    free(req_filename);
    free(rep_filename);
  }

  zframe_destroy(&reply_to);
  mdp_worker_destroy(&worker);
}

/*
 * .split try to call a service
 * Here, we first check if the requested MDP service is defined or not,
 * using a MMI lookup to the Majordomo broker. If the service exists,
 * we send a request and wait for a reply using the conventional MDP
 * client API. This is not meant to be fast, just very simple:
 */
static int
s_service_success(char *uuid)
{
  /* Load request message, service will be first frame */
  char *filename = s_request_filename(uuid);
  FILE *file = fopen(filename, "r");
  free(filename);

  /* If the client already closed request, treat as successful */
  if (!file) {
    return 1;
  }

  zmsg_t *request = zmsg_load(file);
  fclose(file);

  zframe_t *service = zmsg_pop(request);
  char *service_name = zframe_strdup(service);

  /* Create MDP client session with short timeout */
  mdp_client_t *client = mdp_client_new("tcp://localhost:5555", false);
  mdp_client_set_timeout(client, 1000);  /* 1 sec */

  /* Use MMI protocol to check if service is available */
  zmsg_t *mmi_request = zmsg_new();
  zmsg_add(mmi_request, service);
  mdp_client_send(client, "mmi.service", &mmi_request);

  zmsg_t *mmi_reply = mdp_client_recv(client, NULL, NULL);
  int service_ok = (mmi_reply && zframe_streq(zmsg_first(mmi_reply), "200"));
  zmsg_destroy(&mmi_reply);

  int result = 0;
  if(service_ok) {
    mdp_client_send(client, service_name, &request);
    zmsg_t *reply = mdp_client_recv(client, NULL, NULL);

    if (reply) {
      filename = s_reply_filename(uuid);
      FILE *file = fopen(filename, "w");
      assert(file);
      zmsg_save(reply, file);
      fclose(file);
      free(filename);
      result = 1;
    }
    zmsg_destroy(&reply);
  }
  else
    zmsg_destroy(&request);

  mdp_client_destroy(&client);
  free(service_name);

  return result;
}

static void
s_shutdown_call(mdp_client_t *session, char *service, zmsg_t **request_p)
{
  mdp_client_send(session, service, request_p);
  zmsg_t *reply = mdp_client_recv(session, NULL, NULL);
  if (reply) {
    zmsg_destroy(&reply);
  }
}

/*
 * .split worker task
 * This is the main thread for the Titanic worker. It starts three child
 * threads; for the request, reply, and close services. It then dispatches
 * requests to workers using a simple brute force disk queue. It receives
 * request UUIDs from the {{titanic.request}} service, saves these to a disk
 * file, and then throws each request at MDP workers until it gets a
 * response.
 */
int main(int argc, char *argv [])
{
  int verbose = (argc > 1 && streq(argv [1], "-v"));

  /* use zactor to handle multithreads */
  zactor_t *request_actor = zactor_new(titanic_request, NULL);
  zactor_t *reply_actor = zactor_new(titanic_reply, NULL);
  zactor_t *close_actor = zactor_new(titanic_close, NULL);

  /* Main dispatcher loop */
  while (true) {
    /* We'll dispatch once per second, if there's no activity */
    zmq_pollitem_t items[] = { { zsock_resolve(request_actor), 0, ZMQ_POLLIN, 0 } };
    int rc = zmq_poll(items, 1, 1000*ZMQ_POLL_MSEC);
    if (rc == -1) {
      break;      /* Interrupted */
    }

    if (items [0].revents & ZMQ_POLLIN) {
      /* Append UUID to queue, prefixed with '-' for pending */
      zmsg_t *msg = zmsg_recv(request_actor);

      if (!msg) {
        break;    /* Interrupted */
      }

      /* Ensure message directory exists */
      zfile_mkdir(TITANIC_DIR);

      FILE *q_file = fopen(TITANIC_DIR "/queue", "a");
      char *uuid = zmsg_popstr(msg);
      fprintf(q_file, "-%s\n", uuid);
      fclose(q_file);
      free(uuid);
      zmsg_destroy(&msg);
    }

    /* Brute force dispatcher */
    char entry[] = "?.......:.......:.......:.......:";
    FILE *file = fopen(TITANIC_DIR "/queue", "r+");

    while (file && fread(entry, 33, 1, file) == 1) {
      /* UUID is prefixed with '-' if still waiting */
      if (entry[0] == '-') {
        if (verbose) {
          printf ("I: processing request %s\n", entry + 1);
        }

        if (s_service_success(entry + 1)) {
          /* Mark queue entry as processed */
          fseek(file, -33, SEEK_CUR);
          fwrite("+", 1, 1, file);
          fseek(file, 32, SEEK_CUR);
        }
      }

      /* Skip end of line, LF or CRLF */
      if (fgetc(file) == '\r') {
        fgetc(file);
      }

      if (zctx_interrupted) {
        break;
      }
    }

    if (file) {
      fclose (file);
    }
  }

  /* shutdown the titanic services gracefully */
  mdp_client_t *session = mdp_client_new("tcp://localhost:5555", 0);

  printf("shutting down titanic.request service ...\n");
  zmsg_t *request = zmsg_new();
  zmsg_pushstr(request, "shutdown");
  s_shutdown_call(session, "titanic.request", &request);

  printf("shutting down titanic.reply service ...\n");
  zmsg_t *reply = zmsg_new();
  zmsg_pushstr(reply, "shutdown");
  s_shutdown_call(session, "titanic.reply", &reply);

  printf("shutting down titanic.close service ...\n");
  zmsg_t *close = zmsg_new();
  zmsg_pushstr(close, "shutdown");
  s_shutdown_call(session, "titanic.close", &close);

  mdp_client_destroy(&session);

  zactor_destroy(&request_actor);
  zactor_destroy(&reply_actor);
  zactor_destroy(&close_actor);

  printf("shutdown completed!\n");

  return 0;
}
