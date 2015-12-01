#include <bson.h>
#include <bcon.h>
#include <mongoc.h>
#include <stdio.h>
#include <assert.h>

int
main (int   argc,
      char *argv[])
{
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    bson_error_t error;
    bson_t *command;
    bson_t reply;
    char *str;

    mongoc_init ();

    client = mongoc_client_new ("mongodb://test:test@4localhost:27013/");
    assert(client);
    collection = mongoc_client_get_collection (client, "xzc_test", "item");

    command = BCON_NEW ("collStats", BCON_UTF8 ("item"));
    if (mongoc_collection_command_simple (collection, command, NULL, &reply, &error)) {
        str = bson_as_json (&reply, NULL);
        printf ("%s\n", str);
        bson_free (str);
    } else {
        fprintf (stderr, "Failed to run command: %s\n", error.message);
    }

    bson_destroy (command);
    bson_destroy (&reply);
    mongoc_collection_destroy (collection);
    mongoc_client_destroy (client);
    mongoc_cleanup ();

    return 0;
}
