/*
 * Titanic client
 * Implements client side of http://rfc.zeromq.org/spec:9
 */

#include "mdp.h"

/*
 * Calls a TSP service
 * Returns response if successful (status code 200 OK), else NULL
 */
static zmsg_t *
s_service_call(mdp_client_t *session, char *service, zmsg_t **request_p)
{
  mdp_client_send(session, service, request_p);
  zmsg_t *reply = mdp_client_recv(session, NULL, NULL);

  if (reply) {
    zframe_t *status = zmsg_pop(reply);
    if (zframe_streq(status, "200")) {
      zframe_destroy(&status);
      return reply;
    }
    else if (zframe_streq(status, "400")) {
      printf("E: client fatal error, aborting\n");
      exit(EXIT_FAILURE);
    }
    else if (zframe_streq(status, "500")) {
      printf("E: server fatal error, aborting\n");
      exit(EXIT_FAILURE);
    }
  }
  else
    exit(EXIT_SUCCESS);    /* Interrupted or failed */

  zmsg_destroy(&reply);

  return NULL;             /* Didn't succeed; don't care why not */
}

/*
 * .split main task
 * The main task tests our service call by sending an echo request:
 */
int main(int argc, char *argv [])
{
  int verbose = (argc > 1 && streq(argv [1], "-v"));
  mdp_client_t *session = mdp_client_new("tcp://localhost:5555", verbose);

  /* 1. Send 'echo' request to Titanic */
  zmsg_t *request = zmsg_new();
  zmsg_pushstr(request, "shutdown");
  zmsg_pushstr(request, "echo");
  zmsg_t *reply = s_service_call(session, "titanic.request", &request);

  zframe_t *uuid = NULL;
  if (reply) {
    uuid = zmsg_pop(reply);
    zmsg_destroy(&reply);
    zframe_print(uuid, "I: request UUID ");
  }

  /* 2. Wait until we get a reply */
  while (!zctx_interrupted) {
    zclock_sleep(100);
    request = zmsg_new();
    zmsg_add(request, zframe_dup(uuid));
    zmsg_t *reply = s_service_call(session, "titanic.reply", &request);

    if (reply) {
      char *reply_string = zframe_strdup(zmsg_last(reply));
      printf("Reply: %s\n", reply_string);
      free(reply_string);
      zmsg_destroy(&reply);

      /* 3. Close request */
      request = zmsg_new();
      zmsg_add(request, zframe_dup(uuid));
      reply = s_service_call(session, "titanic.close", &request);
      zmsg_destroy(&reply);
      break;
    }
    else {
      printf("I: no reply yet, trying again...\n");
      zclock_sleep(5000);     /* Try again in 5 seconds */
    }
  }

  zframe_destroy(&uuid);
  mdp_client_destroy(&session);

  return 0;
}
