/*
 * MM service - Majordomo Protocol client
 */

#include <bson/bson.h>
#include "mdp.h"

/*
 * Display the reply of zmsg_t type
 */
static void
s_reply_display(zmsg_t *reply)
{
  int sz, i;
  char *str;

  if (reply) {
    sz = zmsg_size(reply);

    for (i=0; i < sz; i++) {
      str = zmsg_popstr(reply);
      printf("%s\n", str);
      free(str);
    }
  }
}

/*
 * Send JSON based zmsg_t request to the mm_worker via Majordomo broker:
 * Internally, it converts the BSON objects to the JSON strings and
 * pass on to the Majordome broker
 */
static zmsg_t *
s_mm_send(mdp_client_t *session, char *operation, bson_t *query, bson_t *update)
{
  zmsg_t *request;
  zmsg_t *reply;
  char *qstr;
  char *ustr;

  request = zmsg_new();
  /* convert a BSON query oject to a JSON string */
  qstr = bson_as_canonical_extended_json(query, NULL);

  if (update) {
    /* convert the BSON query oject to the JSON string */
    ustr = bson_as_canonical_extended_json(update, NULL);
    zmsg_pushstr(request, ustr);       /* update string */
    free(ustr);
  }
  zmsg_pushstr(request, qstr);
  zmsg_pushstr(request, operation);    /* operation string */
  free(qstr);

  mdp_client_send(session, "MM", &request);    /* MM service */
  reply = mdp_client_recv(session, NULL, NULL);

  zmsg_destroy(&request);

  return reply;
}

/*
 * This client sends the simulated the Purchase Order (PO) operations to an
 * ERP Material Management (MM) processing service
 */
int main (int argc, char *argv[])
{
  int verbose;
  mdp_client_t *session;

  zmsg_t *reply;
  bson_t *query;
  bson_t *doc;
  bson_t *update;

  verbose = (argc > 1 && streq(argv[1], "-v"));
  session = mdp_client_new("tcp://localhost:5555", verbose);

  /* POST */
  /* POSave operation which triggers a CREATE in the mongodb worker */
  doc = bson_new();
  BSON_APPEND_UTF8(doc, "k_material", "cpu");
  reply = s_mm_send(session, "POSave", doc, NULL);
  bson_destroy(doc);
  s_reply_display(reply);
  zmsg_destroy(&reply);

  /* GET */
  /* POSelect operation which triggers a RETRIEVE in the mongodb worker */
  query = bson_new();
  BSON_APPEND_UTF8(query, "k_material", "cpu");
  reply = s_mm_send(session, "POSelect", query, NULL);
  bson_destroy(query);
  s_reply_display(reply);
  zmsg_destroy(&reply);

  /* PUT */
  /* POUpdate operation which triggers a UPDATE in the mongodb worker */
  query = bson_new();
  BSON_APPEND_UTF8(query, "k_material", "cpu");
  update = bson_new();
  BSON_APPEND_UTF8(update, "k_material", "memory");
  reply = s_mm_send(session, "POUpdate", query, update);
  bson_destroy(query);
  bson_destroy(update);
  s_reply_display(reply);
  zmsg_destroy(&reply);

  /* DELETE */
  /* PODelete operation which triggers a DELETE in the mongodb worker */
  query = bson_new();
  BSON_APPEND_UTF8(query, "k_material", "cpu");
  reply = s_mm_send(session, "PODelete", query, NULL);
  bson_destroy(query);
  s_reply_display(reply);
  zmsg_destroy(&reply);

  mdp_client_destroy(&session);

  return 0;
}
