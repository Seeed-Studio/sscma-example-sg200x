#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mosquitto.h>

void on_connect(struct mosquitto *mosq, void *obj, int reason_code) {
    printf("Connected to broker with code %d.\n", reason_code);
}

void on_publish(struct mosquitto *mosq, void *obj, int mid) {
    printf("Message %d published.\n", mid);
}

int main(int argc, char *argv[]) {
    struct mosquitto *mosq;
    int rc;

    mosquitto_lib_init();

    mosq = mosquitto_new("publisher-client", true, NULL);
    if (!mosq) {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_publish_callback_set(mosq, on_publish);

    rc = mosquitto_connect(mosq, "localhost", 1883, 60);
    if (rc) {
        fprintf(stderr, "Could not connect to broker. Error Code: %d\n", rc);
        return 1;
    }

    rc = mosquitto_loop_start(mosq);
    if (rc) {
        fprintf(stderr, "Could not start loop. Error Code: %d\n", rc);
        return 1;
    }

    rc = mosquitto_publish(mosq, NULL, "test/topic", strlen("Hello, World!"), "Hello, World!", 0, false);
    if (rc) {
        fprintf(stderr, "Could not publish message. Error Code: %d\n", rc);
        return 1;
    }

    // Give the client time to publish the message
    sleep(1);

    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}
