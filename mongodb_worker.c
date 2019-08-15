/*
 * MongoDB service - Majordomo Protocol client
 */

#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include "mdp.h"


struct _mongodb_engine_t {
  mdp_worker_t *session;
  mongoc_client_t *mongo_client;
};

typedef struct _mongodb_engine_t mongodb_engine_t;


static mongodb_engine_t *
s_mongodb_engine_new(int verbose)
{
  mongodb_engine_t *self;
  mdp_worker_t *session;
  mongoc_client_t *client;

  self = (mongodb_engine_t *)zmalloc(sizeof *self);
  session = mdp_worker_new("tcp://localhost:8888", "MongoDB", verbose);

  mongoc_init();
  /* Connects to a mongodb database or a mongodb replica set's PRIMARY node */
  client = mongoc_client_new("mongodb://localhost:30001/?appname=mongodb_engine");

  self->session = session;
  self->mongo_client = client;

  return self;
}

static void
s_mongodb_engine_destroy(mongodb_engine_t **self_p)
{
  assert(self_p);

  if (*self_p) {
    mongodb_engine_t *self = *self_p;
    mdp_worker_destroy(&self->session);
    mongoc_client_destroy(self->mongo_client);
    mongoc_cleanup();
    free(self);
    *self_p = NULL;
  }
}

static void
s_mongodb_handle_create(zmsg_t *report, zmsg_t *request, mongoc_collection_t *collection)
{
  bson_error_t error;
  bson_oid_t oid;
  bson_t *full_doc;
  bson_t *sub_doc;
  char *jdoc;

  full_doc = bson_new();
  bson_oid_init(&oid, NULL);
  BSON_APPEND_OID(full_doc, "_id", &oid);

  /* Get the JSON string */
  jdoc = zmsg_popstr(request);
  /* convert the JSON string to the BSON object */
  sub_doc = bson_new_from_json((const uint8_t *)jdoc, -1, &error);
  free(jdoc);

  bson_iter_t iter;
  const char *key;
  char *value;
  const bson_value_t *bvalue;

  if (bson_iter_init(&iter, sub_doc)) {
    while (bson_iter_next(&iter)) {
      key = bson_iter_key(&iter);
      bvalue = bson_iter_value(&iter);
      /* for this demo, let's assume the value is of type utf8 */
      value = bvalue->value.v_utf8.str;
      BSON_APPEND_UTF8(full_doc, key, value);
    }
  }

  /* create the document */
  if (!mongoc_collection_insert_one(collection, full_doc, NULL, NULL, &error)) {
    zmsg_pushstr(report, error.message);
  }
  else {
    zmsg_pushstr(report, "200");   /* 200 - status: successful */
  }

  bson_destroy(sub_doc);
  bson_destroy(full_doc);
}

static void
s_mongodb_handle_retrieve(zmsg_t *report, zmsg_t *request, mongoc_collection_t *collection)
{
  bson_error_t error;
  mongoc_cursor_t *cursor;
  const bson_t *doc;
  bson_t *query;
  char *str;

  /* Get the JSON string */
  char *jdoc = zmsg_popstr(request);
  /* convert the JSON string to the BSON query object */
  query = bson_new_from_json((const uint8_t *)jdoc, -1, &error);
  free(jdoc);

  cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

  /* Put all the found entries in the report JSON string */
  while (mongoc_cursor_next(cursor, &doc)) {
    str = bson_as_canonical_extended_json(doc, NULL);
    zmsg_pushstr(report, str);
    free(str);
  }

  bson_destroy(query);
  mongoc_cursor_destroy(cursor);
}

static void
s_mongodb_handle_update(zmsg_t *report, zmsg_t *request, mongoc_collection_t *collection)
{
  bson_error_t error;
  bson_t *query;
  bson_t *doc;
  bson_t *update = NULL;
  char *jdoc;

  /* Get the JSON string */
  jdoc = zmsg_popstr(request);
  /* convert the JSON string to the BSON query object */
  query = bson_new_from_json((const uint8_t *)jdoc, -1, &error);
  free(jdoc);

  jdoc = zmsg_popstr(request);
  /* convert the JSON string to the BSON update object */
  doc = bson_new_from_json((const uint8_t *)jdoc, -1, &error);
  free(jdoc);

  bson_iter_t iter;
  const char *key;
  char *value;
  const bson_value_t *bvalue;

  if (bson_iter_init(&iter, doc)) {
    while (bson_iter_next(&iter)) {
      key = bson_iter_key(&iter);
      bvalue = bson_iter_value(&iter);
      /* let's assume the value is of type utf8 */
      value = bvalue->value.v_utf8.str;
      update = BCON_NEW("$set", "{", key, BCON_UTF8(value), "}");
      /* only update the first found entry */
    }
  }

  if (!mongoc_collection_update_one(collection, query, update, NULL, NULL, &error)) {
    zmsg_pushstr(report, error.message);
  }
  else {
    zmsg_pushstr(report, "200");   /* 200 - status: successful */
  }

  bson_destroy(query);
  bson_destroy(doc);
  bson_destroy(update);
}

static void
s_mongodb_handle_delete(zmsg_t *report, zmsg_t *request, mongoc_collection_t *collection)
{
  bson_error_t error;
  bson_t *query;

  /* Get the JSON string */
  char *jdoc = zmsg_popstr(request);
  /* convert the JSON string to the BSON query object */
  query = bson_new_from_json((const uint8_t *)jdoc, -1, &error);
  free(jdoc);

  if (!mongoc_collection_delete_one(collection, query, NULL, NULL, &error)) {
    zmsg_pushstr(report, error.message);
  }
  else {
    zmsg_pushstr(report, "200");   /* 200 - status: successful */
  }

  bson_destroy(query);
}


static void
s_mongodb_handle_request(mongodb_engine_t *self, zmsg_t *request, zframe_t *reply_to)
{
  char *db;
  char *collection;
  char *operation;
  zmsg_t *report;
  mongoc_collection_t *coll;

  db = zmsg_popstr(request);
  collection = zmsg_popstr(request);
  operation = zmsg_popstr(request);

  report = zmsg_new();

  /* get the collection from the db */
  coll = mongoc_client_get_collection(self->mongo_client, db, collection);

  /* CRUD operations */
  if (strcmp(operation, "CREATE") == 0) {
    s_mongodb_handle_create(report, request, coll);
  }

  if (strcmp(operation, "RETRIEVE") == 0) {
    s_mongodb_handle_retrieve(report, request, coll);
  }

  if (strcmp(operation, "UPDATE") == 0) {
    s_mongodb_handle_update(report, request, coll);
  }

  if (strcmp(operation, "DELETE") == 0) {
    s_mongodb_handle_delete(report, request, coll);
  }

  /* Send the report back */
  mdp_worker_send(self->session, &report, reply_to);
  zframe_destroy(&reply_to);

  /* clean up */
  free(operation);
  free(collection);
  free(db);
  mongoc_collection_destroy(coll);
  zmsg_destroy(&request);
  zmsg_destroy(&report);
}

/*
 * This worker provides the simple CRUD services of Mongodb and sends results back
 * to the respective clients
 */
int main(int argc, char *argv[])
{
  int verbose;
  mongodb_engine_t *mdb_engine;

  verbose = (argc > 1 && streq(argv[1], "-v"));
  mdb_engine = s_mongodb_engine_new(verbose);

  while (true) {
    zframe_t *reply_to;
    zmsg_t *request = mdp_worker_recv(mdb_engine->session, &reply_to);
    if (request == NULL) {
      break;              /* Worker was interrupted */
    }

    s_mongodb_handle_request(mdb_engine, request, reply_to);
  }

  s_mongodb_engine_destroy(&mdb_engine);

  return 0;
}
