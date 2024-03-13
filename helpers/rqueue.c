/*
 *
 * This file - example about queue with Redis LIST data structure
 *
 */

#include <hiredis.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define NEED_FULL_LOG 1
#define THIS_FILE "rqueue "

void print_time() {
    struct timeval tv;
    struct tm* ptm;
    char time_string[40];
    long milliseconds;

    /* Obtain the time of day, and convert it to a tm struct. */
    gettimeofday (&tv, NULL);
    ptm = localtime (&tv.tv_sec);
    /* Format the date and time, down to a single second. */
    strftime (time_string, sizeof (time_string), "%Y-%m-%d %H:%M:%S", ptm);
    /* Compute milliseconds from microseconds. */
    milliseconds = tv.tv_usec / 1000;
    /* Print the formatted time, in seconds, followed by a decimal point
    and the milliseconds. */
    fprintf (stdout, "[%s.%03ld] ", time_string, milliseconds);

}

int main() {

    redisContext *c;
    redisReply * reply;

    const char *hostname = "127.0.0.1";
    int port = 6379;

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }

    char *cc_token, *cc_rest;

    do {
        //work
        reply = redisCommand(c,"RPOP once_exec_connchannels");

        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sRPOP once_exec_connchannels is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            strcpy(cc_rest, reply->str);

            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "make job...\n");

            cc_token =strtok_r(cc_rest, ":", &cc_rest);
            printf("cc_id = %ld\n", atol(cc_token));

            cc_token =strtok_r(cc_rest, ":", &cc_rest);
            printf("cc_val = %ld\n", atol(cc_token));
        }
    } while (reply->type != REDIS_REPLY_NIL);

}
