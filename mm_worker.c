/*
 * MM service - Majordomo Protocol worker
 */

#include <string.h>
#include <bson/bson.h>
#include "mdp.h"


struct _mm_engine_t {
  mdp_worker_t *to_client;    /* session which replies to mm_client */
  mdp_client_t *to_mongodb;   /* session which sends requests to mongodb_worker */
  char db[8];                 /* mongodb name */
};

typedef struct _mm_engine_t mm_engine_t;


static mm_engine_t *
s_mm_engine_new(int verbose)
{
  mm_engine_t *self;
  mdp_worker_t *to_client;
  mdp_client_t *to_mongodb;

  self = (mm_engine_t *)zmalloc(sizeof *self);
  to_client = mdp_worker_new("tcp://localhost:5555", "MM", verbose);
  to_mongodb = mdp_client_new("tcp://localhost:8888", verbose);

  self->to_client = to_client;
  self->to_mongodb = to_mongodb;
  /* should read from a cfg file, for convenience, I make it hardcoded */
  strcpy(self->db, "mydb");

  return self;
}

static void
s_mm_engine_destroy(mm_engine_t **self_p)
{
  assert(self_p);

  if (*self_p) {
    mm_engine_t *self = *self_p;
    mdp_worker_destroy(&self->to_client);
    mdp_client_destroy(&self->to_mongodb);
    free(self);
    *self_p = NULL;
  }
}

static zmsg_t *
s_mongodb_crud(mm_engine_t *self, char *collection, char *operation, bson_t *query, bson_t *update)
{
  zmsg_t *request;
  zmsg_t *reply;
  char *qstr;
  char *ustr;

  request = zmsg_new();
  qstr = bson_as_canonical_extended_json(query, NULL);

  if (update) {
    ustr = bson_as_canonical_extended_json(update, NULL);
    zmsg_pushstr(request, ustr);       /* update string */
    free(ustr);
  }
  zmsg_pushstr(request, qstr);         /* query string */
  zmsg_pushstr(request, operation);    /* operation string */
  zmsg_pushstr(request, collection);   /* collection string */
  zmsg_pushstr(request, self->db);     /* db string */
  free(qstr);

  mdp_client_send(self->to_mongodb, "MongoDB", &request);
  reply = mdp_client_recv(self->to_mongodb, NULL, NULL);

  zmsg_destroy(&request);

  return reply;
}

static void
s_mm_handle_request(mm_engine_t *self, zmsg_t *request, zframe_t *reply_to)
{
  zmsg_t *report;
  char *operation;

  bson_error_t error;
  zmsg_t *reply;
  bson_t *query;
  bson_t *doc;
  bson_t *update;

  report = zmsg_new();
  operation = zmsg_popstr(request);

  /*
   * Here the data to be processed should be added, and the processing logic
   * and algorithm should be presented, but for this demo. I only use the
   * data from the client directly to perform mongodb CRUD
   */
  if (strcmp(operation, "POSave") == 0) {
    char *po_doc;
    char *replystr;

    po_doc = zmsg_popstr(request);
    /* convert the JSON string to the BSON object */
    doc = bson_new_from_json((const uint8_t *)po_doc, -1, &error);
    free(po_doc);

    /* reply from the mongodb worker */
    reply = s_mongodb_crud(self, "Coll_PO", "CREATE", doc, NULL);
    bson_destroy(doc);

    replystr = zmsg_popstr(reply);
    if (strcmp(replystr, "200") == 0) {
      zmsg_pushstr(report, "One document created.");
    }
    else {
      zmsg_pushstr(report, "creating document failed.");
    }
    free(replystr);

    zmsg_destroy(&reply);
  }

  if (strcmp(operation, "POSelect") == 0) {
    int sz, i;
    char *po_query;
    char *str;

    po_query = zmsg_popstr(request);
    /* convert the JSON string to the BSON query object */
    query = bson_new_from_json((const uint8_t *)po_query, -1, &error);
    free(po_query);

    /* reply from the mongodb worker */
    reply = NULL;
    reply = s_mongodb_crud(self, "Coll_PO", "RETRIEVE", query, NULL);
    bson_destroy(query);

    if (reply) {
      sz = zmsg_size(reply);

      for (i=0; i < sz; i++) {
        str = zmsg_popstr(reply);
        zmsg_pushstr(report, str);
        free(str);
      }
    }
    else {
      zmsg_pushstr(report, "Nothing selected");
    }

    zmsg_destroy(&reply);
  }

  if (strcmp(operation, "POUpdate") == 0) {
    char *po_query;
    char *po_update;
    char *replystr;

    po_query = zmsg_popstr(request);
    po_update = zmsg_popstr(request);
    /* convert the JSON string to the BSON query object */
    query = bson_new_from_json((const uint8_t *)po_query, -1, &error);
    free(po_query);
    /* convert the JSON string to the BSON update object */
    update = bson_new_from_json((const uint8_t *)po_update, -1, &error);
    free(po_update);

    /* reply from the mongodb worker */
    reply = s_mongodb_crud(self, "Coll_PO", "UPDATE", query, update);
    bson_destroy(query);
    bson_destroy(update);

    replystr = zmsg_popstr(reply);
    if (strcmp(replystr, "200") == 0) {
      zmsg_pushstr(report, "One document updated.");
    }
    else {
      zmsg_pushstr(report, "Updating document failed.");
    }
    free(replystr);

    zmsg_destroy(&reply);
  }

  /*
   * In a real ERP system, the delete operation should not be committed
   * directly, it sets a 'deletion mark' field for the respective entry.
   * Here I just want to show the DB deletion which completes the CRUD
   */
  if (strcmp(operation, "PODelete") == 0) {
    char *po_query;
    char *replystr;

    po_query = zmsg_popstr(request);
    /* convert the JSON string to the BSON query object */
    query = bson_new_from_json((const uint8_t *)po_query, -1, &error);
    free(po_query);

    /* reply from the mongodb worker */
    reply = s_mongodb_crud(self, "Coll_PO", "DELETE", query, NULL);
    bson_destroy(query);

    replystr = zmsg_popstr(reply);
    if (strcmp(replystr, "200") == 0) {
      zmsg_pushstr(report, "One document deleted.");
    }
    else {
      zmsg_pushstr(report, "Deleting document failed.");
    }
    free(replystr);

    zmsg_destroy(&reply);
  }

  /* reply to the client - mm reply message (mongodb reply) */
  mdp_worker_send(self->to_client, &report, reply_to);
  zframe_destroy(&reply_to);

  /* clean up */
  free(operation);
  zmsg_destroy(&request);
  zmsg_destroy(&report);
}

/*
 * This worker simulates the the Purchase Order (PO) processing engine in an
 * ERP Material Management (MM) service and return the results back to the client
 */
int main(int argc, char *argv[])
{
  int verbose;
  mm_engine_t *engine;

  verbose = (argc > 1 && streq(argv[1], "-v"));
  engine = s_mm_engine_new(verbose);

  while (true) {
    zframe_t *reply_to;
    zmsg_t *request = mdp_worker_recv(engine->to_client, &reply_to);
    if (request == NULL) {
      break;              /* Worker was interrupted */
    }

    s_mm_handle_request(engine, request, reply_to);
  }

  s_mm_engine_destroy(&engine);

  return 0;
}
