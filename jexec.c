//#include <sqlite3.h>
#include "sqlite3.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h> /* for getpid(), usleep() and others */
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
//#include <hiredis.h>
#include "hiredis/hiredis.h"
// debian variant
//#include <hiredis/hiredis.h>
#include <fcntl.h>

//TODO  move interpeter code to interpreter.c
//#include "interpreter.h"

// information from this sites: https://www.linuxjournal.com/content/accessing-sqlite-c - here update example!!!
// https://www.tutorialspoint.com/sqlite/sqlite_c_cpp.htm
// http://zetcode.com/db/sqlitec/

#define VER "#v0.05# "
#define THIS_FILE "jexe "
//TODO make define handler in NEED_FULL_LOG
#define NEED_FULL_LOG 1
#define MAX_DEL_RECORDS  100
#define MAX_SCENARIO_ACTIONS 100
#define MAX_UICONTROLS_IN_USERPAGE 2048 
#define MAX_COMMAND_STACK_SIZE 100
#define MAX_CONN_CHANNELS 2048
#define TEN_SECONDS  10
#define FIVE_SECONDS  5
#define MINUTE  60
#define HOUR  3600
#define DAY  86400
#define WEEK 604800
#define MONTH 86400*30
#define MAIN_LOOP_DELAY 1000000
#define DEFAULT_IP_ADDR "192.168.0.71\n"
#define DEFAULT_IP_MASK "255.255.255.0\n"
#define JEXEC_DB_FILE   "test.db"
#define WEBAP_DB_FILE   "/home/si/work/leo_express_app/db.sqlite"
#define MB_RTU_EXE      "/home/si/work/mbrtuexe/rdmbrtuexe"

// GLOBAL
// database context
// for scenarios
int rc;
// for web
//int rc_web;
// REDIS
redisContext *c;
// if database pointer is NULL - sqlite3 generate "out of memory" error
// dbase of scenarois
sqlite3 *db;
// dbase of configuration
//sqlite3 *db_web;

// GLOBAL SCENARIOS VARIABLES
// program counter
int scen_pc;
// scenario end flag
int scen_end;
// the last command of the main program (when exiting the subroutine)
//int scen_stack;
struct stack
{
    int stk[MAX_COMMAND_STACK_SIZE];
    int top;
};
// last command now in LIFO stack
typedef struct stack STACK;
STACK ps;
// array of delete records
int del_scen_rec[MAX_DEL_RECORDS];
int del_array_no;
// time of main loop of rdexe
long main_cycle_time, start_main_cycle_time;
// sensors states need check array
int sensor_state_is_need_to_check[MAX_CONN_CHANNELS];

int actions_handler(int);

//          fill the array of sensor states need change by 1
//          this check is for time in main loop
//
void fill_sensor_state_changes_ones() {
    int j;
    for (j = 0; j < MAX_CONN_CHANNELS; j++) sensor_state_is_need_to_check[j] = 1;
}

//          fill the array of ccprevstate redis keys by "unknown"
//
void reset_prev_uicontrol_states() {

    redisReply *reply;
    reply = redisCommand(c, "SMEMBERS user_page_controls_set");

    if ( reply->type == REDIS_REPLY_ERROR ) {
        printf( "Error: %s\n", reply->str );
    } else if ( reply->type != REDIS_REPLY_ARRAY ) {
        printf( "Unexpected type: %d\n", reply->type );
    } else {
        int i;
        // in redis SET storied all controls in user WEB-page who wait UI change event
        // loop all of it
        for (i = 0; i < reply->elements; i++ ) {
            redisCommand(c,"SET ccprevstate_%s %s", reply->element[i]->str, "unknown");
        }
    }

    freeReplyObject(reply);

}

//          function update all keys ccstate_N with 'inactive'
//
void set_all_ccstates_inactive(void) {

    redisReply *reply_all_ccstates;

    reply_all_ccstates = redisCommand(c,"KEYS ccstate_*");
    if ( reply_all_ccstates->type == REDIS_REPLY_ERROR ) {
        if (NEED_FULL_LOG) fprintf(stdout, "%sError: %s\n", THIS_FILE, reply_all_ccstates->str );

    } else if ( reply_all_ccstates->type != REDIS_REPLY_ARRAY ) {
        if (NEED_FULL_LOG) fprintf(stdout, "%sUnexpected type: %d\n", THIS_FILE, reply_all_ccstates->type );
    } else {
        int i = 0;

        for (i = 0; i < reply_all_ccstates->elements; i++ ) {
            if (NEED_FULL_LOG) fprintf(stdout, "%sstate key: %s\n", THIS_FILE, reply_all_ccstates->element[i]->str );

            redisCommand(c, "SET %s %s", reply_all_ccstates->element[i]->str, "inactive");

        }
    }

    freeReplyObject(reply_all_ccstates);

}

//           getting current time and return long value
//
long ltime() {
    long t;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t = tv.tv_sec + ((long)tv.tv_usec)/1e6;
    return t;
}

//          prepare current date and time for logging
//          idea from http://www.informit.com/articles/article.aspx?p=23618&seqNum=8
//
//
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

//          write time with %H:%M:%S format to pointer,
//          must malloc and free in function, where we call
//          get_time()
//
void get_time(char *ptr) {
    struct timeval tv;
    struct tm* ptm;
    char time_string[30];
    long milliseconds;

    /* Obtain the time of day, and convert it to a tm struct. */
    gettimeofday (&tv, NULL);
    ptm = localtime (&tv.tv_sec);
    /* Format the date and time, down to a single second. */
    strftime (time_string, sizeof (time_string), "%H:%M:%S", ptm);
    /* Compute milliseconds from microseconds. */
    milliseconds = tv.tv_usec / 1000;
    /* Print the formatted time, in seconds, followed by a decimal point
    and the milliseconds. */
    sprintf (ptr, "[%s.%03ld] ", time_string, milliseconds);
}

//          log message to syslog
//          see /etc/syslog.conf for local1 item
void logger (char *msg) {
    char *log_time;
    log_time  = malloc(sizeof(char)*30);
    get_time(log_time);
    openlog(THIS_FILE, LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_INFO, "%s%s", log_time, msg);
    closelog();
    free(log_time);
}

//         function to add an element to the stack
    void command_stack_push (int element) {
        int num;
        if (ps.top == (MAX_COMMAND_STACK_SIZE - 1))
        {
            printf ("MAX_COMMAND_STACK_SIZE is reached!\n");
            return;
        }
        else
        {
            ps.top = ps.top + 1;
            ps.stk[ps.top] = element;
        }
        return;

    }

//          function to pop an element from the stack
int command_stack_pop() {
    int num;
    if (ps.top == - 1) {
        printf ("Command stack is empty\n");
        return (ps.top);
    }
    else {
        num = ps.stk[ps.top];
        printf ("Poped element of program stack = %d\n", ps.stk[ps.top]);
        ps.top = ps.top - 1;
    }
    return(num);
}

//          prepare statement PRAGMA journal_mode=WAL;
//          This option is persistent (not need using twice)
//          and allow two processes write and read data from 
//          database without any work from programmer side
//          returned value -1  - error
//
int sqlite3_journal_mode_wal_on(void) { 
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "PRAGMA journal_mode=WAL;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {

        int step = sqlite3_step(res);
        if (step == SQLITE_DONE) {
            ret_val = 1;
            if (NEED_FULL_LOG) fprintf(stdout, "%sDB PRAGMA journal_mode=WAL; is OK!\n", THIS_FILE);
        } else {
            ret_val = -1;
            fprintf(stdout, "%sDB PRAGMA journal_mode=WAL; is NOT OK! May be it is ON earlier!\n", THIS_FILE);
        }

    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }


    sqlite3_finalize(res);

    return ret_val;
}

//          prepare statement PRAGMA cache_size= ...;
//          returned value -1  - error
//
int sqlite3_set_cache_size (void) { 
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "PRAGMA cache_size=-4000;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {

        int step = sqlite3_step(res);
        if (step == SQLITE_DONE) {
            ret_val = 1;
            if (NEED_FULL_LOG) fprintf(stdout, "%sDB PRAGMA cache_size=-4000; is OK!\n", THIS_FILE);
        } else {
            ret_val = -1;
            fprintf(stdout, "%sDB PRAGMA cache_size=-4000; is NOT OK!\n", THIS_FILE);
        }

    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }


    sqlite3_finalize(res);

    return ret_val;
}

//          prepare statement PRAGMA foreign_keys = ON;
//          returned value -1  - error
//
int sqlite3_foreign_keys_on(void) { 
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "PRAGMA foreign_keys = ON;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {

        int step = sqlite3_step(res);
        if (step == SQLITE_DONE) {
            ret_val = 1;
            if (NEED_FULL_LOG) fprintf(stdout, "%sDB PRAGMA foreign_keys = ON; is OK!\n", THIS_FILE);
        } else {
            ret_val = -1;
            fprintf(stdout, "%sDB PRAGMA foreign_keys = ON; is NOT OK!\n", THIS_FILE);
        }

    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }


    sqlite3_finalize(res);

    return ret_val;
}

//          Duplicate Parameters table data - see gooogle query
//          How to copy data from one database to another
//
int duplicate_parameters(long scen_id) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "INSERT INTO Parameters SELECT * FROM db_web.Parameters WHERE scenarioId=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        //  it's parameterized query
        sqlite3_bind_int(res, 1, scen_id);

    } else {
        //TODO error log
        fprintf(stderr, "Failed to execute statement INSERT INTO Parameters : %s\n", sqlite3_errmsg(db));
        return -1;
    }

        int step = sqlite3_step(res);
        if (step == SQLITE_DONE) {
            ret_val = 1;
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sDUPLICATE Parameters table data; is OK!\n", THIS_FILE);
        } else {
            ret_val = -1;
            fprintf(stdout, "%s!!!DUPLICATE Parameters table data; is NOT OK!!\n", THIS_FILE);
        }

    sqlite3_finalize(res);

    return ret_val;
}

//          Duplicate Actions table data - see web query
//          How to copy data from one database to another
//
int duplicate_actions(long scen_id) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "INSERT INTO Actions SELECT * FROM db_web.Actions WHERE scenarioId=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        //  it's parameter
        sqlite3_bind_int(res, 1, scen_id);

    } else {
        //TODO error log
        fprintf(stderr, "Failed to execute statement INSERT INTO Actions : %s\n", sqlite3_errmsg(db));
        return -1;
    }

        int step = sqlite3_step(res);
        if (step == SQLITE_DONE) {
            ret_val = 1;
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sDUPLICATE Actions table data; is OK!\n", THIS_FILE);
        } else {
            ret_val = -1;
            fprintf(stdout, "%s!!!DUPLICATE Actions table data; is NOT OK!!\n", THIS_FILE);
        }

    sqlite3_finalize(res);

    return ret_val;
}

//          duplicating scenario record from web base
//
int duplicate_scenario(long scen_id) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "INSERT INTO Scenarios SELECT * FROM db_web.Scenarios WHERE id=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        //  it's parameter
        sqlite3_bind_int(res, 1, scen_id);

    } else {
        //TODO error log
        fprintf(stderr, "Failed to execute statement INSERT INTO Scenarios : %s\n", sqlite3_errmsg(db));
        return -1;
    }

        int step = sqlite3_step(res);
        if (step == SQLITE_DONE) {
            ret_val = 1;
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sDUPLICATE Scenarios table data; is OK!\n", THIS_FILE);
        } else {
            ret_val = -1;
            fprintf(stdout, "%s!!!DUPLICATE Scenarios table data; is NOT OK!!\n", THIS_FILE);
        }

    sqlite3_finalize(res);

    return ret_val;
}

//          activate scenario function
//
int activate_scenario(long scen_id, int value) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "UPDATE Scenarios SET active=? WHERE id=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        //  it's parameter
        sqlite3_bind_int(res, 1, value);
        sqlite3_bind_int(res, 2, scen_id);

    } else {
        //TODO error log
        fprintf(stderr, "Failed to execute statement UPDATE Scenarios : %s\n", sqlite3_errmsg(db));
        return -1;
    }

        int step = sqlite3_step(res);
        if (step == SQLITE_DONE) {
            ret_val = 1;
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sUPDATE Scenarios table active=%d; is OK!\n", THIS_FILE, value);
        } else {
            ret_val = -1;
            fprintf(stdout, "%s!!!UPDATE Scenarios table data; is NOT OK!!\n", THIS_FILE);
        }

    sqlite3_finalize(res);

    return ret_val;
}

//        finding sensor id by actuator id from Ccassociations table
//
long get_cc_sens_id(long ccid) {

    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "SELECT ccidSens FROM db_web.Ccassociations WHERE ccidAct=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        //  it's parameter
        sqlite3_bind_int(res, 1, ccid);

    } else {
        //TODO error log
        fprintf(stderr, "Failed to execute statement SELECT * FROM Ccassociations: %s\n", sqlite3_errmsg(db));
        return -1;
    }

        int step = sqlite3_step(res);
        if (step == SQLITE_ROW) {
            ret_val = sqlite3_column_int(res, 0);
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sAssociation for actuator id=%ld exist in table!\n", THIS_FILE, ccid);
        } else {
            ret_val = 0;
            if (NEED_FULL_LOG) print_time();
            fprintf(stdout, "%sNo associations with actuator id=%ld in table!\n", THIS_FILE, ccid);
        }

    sqlite3_finalize(res);

    return ret_val;

}


//         check if scenario id exist function
//         ret 0 - if not id in table
//         1 - if this id presence
//
int scenario_exist_check(long scen_id) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "SELECT * FROM  Scenarios WHERE id=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        //  it's parameter
        sqlite3_bind_int(res, 1, scen_id);

    } else {
        //TODO error log
        fprintf(stderr, "Failed to execute statement SELECT * FROM Scenarios: %s\n", sqlite3_errmsg(db));
        return -1;
    }

        int step = sqlite3_step(res);
        if (step == SQLITE_ROW) {
            ret_val = 1;
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sScenario id %ld exist in table!\n", THIS_FILE, scen_id);
        } else {
            ret_val = 0;
            if (NEED_FULL_LOG) print_time();
            fprintf(stdout, "%sScenario id not in table!\n", THIS_FILE);
        }

    sqlite3_finalize(res);

    return ret_val;
}

//          writing time of last executed operation to Parameters table
//          returned value -1  - error
//
int write_last_exec_time (int pid) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    int ret_val;

    //      prepare query for write
    //
    sql = "UPDATE Parameters SET lastExecTime=? WHERE id=?";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    //      it's parameterized query
        sqlite3_bind_int(res, 1, ltime());
        sqlite3_bind_int(res, 2, pid);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        ret_val = 1;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sUPDATE Parameters with last exec time is OK!\n", THIS_FILE);
    } else {
        ret_val = -1;
        fprintf(stdout, "%sUPDATE Parameters with last exec time is NOT OK!\n", THIS_FILE);
    }

    sqlite3_finalize(res);

    return ret_val;
}

//          writing time of last executed operation to Parameters table
//          with correction - to time, which is saved, when create record
//          returned value -1  - error
//
int write_last_corr_time (int pid, long rtc_param_0, long rtc_intrval) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    int ret_val;
    // corrected time
    long corr_time = rtc_param_0 + (long)((ltime() - rtc_param_0) / rtc_intrval ) * rtc_intrval;

    //      prepare query for write
    //
    sql = "UPDATE Parameters SET lastExecTime=? WHERE id=?";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    //      it's parameterized query
        //sqlite3_bind_int(res, 1, ltime());
        sqlite3_bind_int(res, 1, corr_time);
        sqlite3_bind_int(res, 2, pid);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        ret_val = 1;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sUPDATE Parameters with last corrected time is OK!\n", THIS_FILE);
    } else {
        ret_val = -1;
        fprintf(stdout, "%sUPDATE Parameters with last corrected time is NOT OK!\n", THIS_FILE);
    }

    sqlite3_finalize(res);

    return ret_val;
}

//          writing default address to db_web.Ipsettings table
//          returned value 0 - error, 1 - success
//
int set_default_ip_addr() {
    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    int ret_val;

    //      prepare query for write
    //
    sql = "UPDATE db_web.Ipsettings SET ipaddr=?, ipmask=? WHERE id=1;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    //      it's parameterized query
        //sqlite3_bind_int(res, 1, ltime());
        sqlite3_bind_text(res, 1, DEFAULT_IP_ADDR,-1,NULL);
        sqlite3_bind_text(res, 2, DEFAULT_IP_MASK,-1,NULL);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        ret_val = 1;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sUPDATE Ipsettings with default ip-address is OK!\n", THIS_FILE);
    } else {
        ret_val = 0;
        fprintf(stdout, "%sUPDATE Ipsettings with default ip-address is NOT OK!\n", THIS_FILE);
    }

    sqlite3_finalize(res);

    return ret_val;
}

//          writing default admin password to Users table
//          returned value 0 - error, 1 - success
//
int set_default_admin_passwd() {
    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    int ret_val;

    //      prepare query for write
    //
    sql = "UPDATE db_web.Users SET passwd='admin' WHERE id=1;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    //      it's parameterized query
        //sqlite3_bind_int(res, 1, ltime());
        //sqlite3_bind_text(res, 1, DEFAULT_PASSWD,-1,NULL);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        ret_val = 1;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sUPDATE Users with default pass is OK!\n", THIS_FILE);
    } else {
        ret_val = 0;
        fprintf(stdout, "%sUPDATE Users with default pass is NOT OK!\n", THIS_FILE);
    }

    sqlite3_finalize(res);

    return ret_val;
}

//          writing to Scenarios table - all scenarios is not active
//          returned value 0 - error, 1 - success
//
int set_all_scenarios_not_active() {
    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    int ret_val;

    //      prepare query for write
    //
    sql = "UPDATE db_web.Scenarios SET active=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    //      if it's parameterized query
        sqlite3_bind_int(res, 1, 0);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        ret_val = 1;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sUPDATE Scenarios with all not active is OK!\n", THIS_FILE);
    } else {
        ret_val = 0;
        fprintf(stdout, "%sUPDATE Scenarios with all is not active is NOT OK!\n", THIS_FILE);
    }

    sqlite3_finalize(res);

    return ret_val;
}

//          deleting scenario function
//          deleting scenario function
//          returned value -1  - error
//
int delete_scenario(int sid) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    int ret_val;

    //      prepare query for delete scenario
    //
    sql = "DELETE FROM Scenarios WHERE id = ?";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {

    //      it's parameterized query
    //
        sqlite3_bind_int(res, 1, sid);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        ret_val = 1;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sDELETE scenario with id %d is OK!\n", THIS_FILE, sid);
    } else {
        ret_val = -1;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sDELETE scenario is NOT OK!\n", THIS_FILE);
    }

    sqlite3_finalize(res);

    return ret_val;
}

//          Add once executed operation to Scenarios, Actions, Parameters tables
//          no returned value (void)
//
void db_act_once_add () {
    char *err_msg = 0;
    char *err;
    sqlite3_stmt *res;
    char *sql;
    long connchannel_id;
    int val;
    long scenario_id, action_id;
    //int ret_val;
    int step;
    redisReply *reply;

    //      get once_exec_connchannel_id
    //
    reply = redisCommand(c,"GET once_exec_connchannel_id");
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sGET once_exec_connchannel_id %s\n", THIS_FILE, reply->str);

    if ( reply->type != REDIS_REPLY_NIL ) {
        connchannel_id = (long)strtod(reply->str, &err);

    } else {
        freeReplyObject(reply);
        if (NEED_FULL_LOG) fprintf(stdout, "break once_exec_connchannel_id...");
        goto ActOnceAddDone;
    }

    freeReplyObject(reply);

    //      get once_exec_value
    //
    reply = redisCommand(c,"GET once_exec_value");
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sGET once_exec_value %s\n", THIS_FILE, reply->str);

    if ( reply->type != REDIS_REPLY_NIL ) {
        val = (int)strtod(reply->str, &err);

    } else {
        freeReplyObject(reply);
        if (NEED_FULL_LOG) fprintf(stdout, "break GET once_exec_value...");
        goto ActOnceAddDone;
    }

    freeReplyObject(reply);

    //      prepare query for write to Scenarios table
    //
    sql = "INSERT INTO Scenarios VALUES(NULL, 'Action once', 0, 1, 1, '0-1-2-3-4-5-6', 0);";
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        goto ActOnceAddDone;
    }

    scenario_id = (long)sqlite3_last_insert_rowid(db);

    //      prepare query for write to Actions table
    //
    sql = "INSERT INTO Actions VALUES(NULL, ?, '', 1, 1, 12);";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    //      it's parameterized query
        sqlite3_bind_int(res, 1, scenario_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        delete_scenario(scenario_id);
        goto ActOnceAddDone;
    }

    step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sInsert into Actions table - OK!\n", THIS_FILE);

        action_id = (long)sqlite3_last_insert_rowid(db);

    } else {
        fprintf(stdout, "%sInsert into Actions table is NOT OK!\n", THIS_FILE);
        goto ActOnceAddDone;
    }

    sqlite3_finalize(res);

    //      prepare query for write to Parameters table
    //      TODO add field names for more understanding 
    //      for parameter 1
    sql = "INSERT INTO Parameters VALUES(NULL, ?, 1, '', 'Actuator', NULL, ?, '', '', 0, ?);";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    //      it's parameterized query
        sqlite3_bind_int(res, 1, action_id);
        sqlite3_bind_int(res, 2, connchannel_id);
        sqlite3_bind_int(res, 3, scenario_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        delete_scenario(scenario_id);
        goto ActOnceAddDone;
    }

    step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sInsert P1 into Parameters table - OK!\n", THIS_FILE);
    } else {
        fprintf(stdout, "%sInsert P1 into Parameters table is NOT OK!\n", THIS_FILE);
        goto ActOnceAddDone;
    }

    sqlite3_finalize(res);
 
    //      prepare query for write to Parameters table
    //      for parameter 2
    sql = "INSERT INTO Parameters VALUES(NULL, ?, 2, '', 'Constant', ?, NULL, '', '', 0, ?);";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
    //      it's parameterized query
        sqlite3_bind_int(res, 1, action_id);
        sqlite3_bind_int(res, 2, val);
        sqlite3_bind_int(res, 3, scenario_id);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        delete_scenario(scenario_id);
        goto ActOnceAddDone;
    }

    step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sInsert P2 into Parameters table - OK!\n", THIS_FILE);
    } else {
        fprintf(stdout, "%sInsert P2 into Parameters table is NOT OK!\n", THIS_FILE);
        goto ActOnceAddDone;
    }

    sqlite3_finalize(res);

    //      set a REDIS key - add complete
    reply = redisCommand(c,"SET once_exec_action_needed 0");
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sSET once_exec_action_needed 0 - %s!\n", THIS_FILE, reply->str);
    freeReplyObject(reply);

    //      05.02.2020 now make action yet
    actions_handler(scenario_id);

ActOnceAddDone:
   return; 
}

//          function returned value from sensor
//          returned value -1  - error
//          now, value of sensor storied in redis key "ccstate_N"
//          where N - ccId (connchannelId)
//
int get_sensor_data (int conn_chann_id) {

    unsigned int j;
    redisReply *reply, *reply1;
    char *err;
    sqlite3_stmt *res;
    char *sql;
    int ret_val;
    char *log_msg, log_addr[100];

    //          now we check state of sensor in redis keys
    //          ccstate_N - is state of sensor with connchannelId=N
    //          cctimestamp_N - is time of last sensor poll with connchannelId=N
    // 
    long cctimestamp;
    reply = redisCommand(c,"GET cctimestamp_%d", conn_chann_id);
    if ( reply->type != REDIS_REPLY_NIL ) {
        cctimestamp = (long)strtod(reply->str, &err);

    } else {
        cctimestamp = 0;
    }
    freeReplyObject(reply);

    //          if (time_now - time_of_last_sensor_poll) < main_cycle_time
    //          data is fresh! Get it from redis key!
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%s cctimestamp is %ld, ltime() is %ld, need_to_check[%d] is %d, main_cycle_time is %ld\n", \
                    THIS_FILE, cctimestamp, ltime(), conn_chann_id, sensor_state_is_need_to_check[conn_chann_id], main_cycle_time ); 
    if ( ltime() - cctimestamp <= 2 * main_cycle_time && sensor_state_is_need_to_check[conn_chann_id] == 0 ) {
        // get value from redis key
        reply = redisCommand(c,"GET ccstate_%d", conn_chann_id);
        if ( reply->type != REDIS_REPLY_NIL ) {
            ret_val = (int)strtod(reply->str, &err);
        } else ret_val = -1;
        freeReplyObject(reply);

        // this staff is moved to main_loop when usleep()
        //sensor_state_is_need_to_check[conn_chan_id] == 1;

        //
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sget_sensor_data() returned %d from redis key\n", THIS_FILE, ret_val); 

        if ( ret_val != -1) goto EndOfGetSensor;
        // else we must get sensor state from hardware

    }

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%slong time query started\n", THIS_FILE); 

    //          prepare query for get Sensor information
    //
    sql = "SELECT db_web.Cdprotos.name AS proto, db_web.Connchannels.funct AS function, db_web.Connchannels.channelAddr AS channel_addr," \
        " db_web.Connchannels.io AS io, db_web.Cdprotos.portSpeed AS port_speed, db_web.Cdprotos.portParams AS port_params," \
        " db_web.Cdprotos.paddress AS device_addr," \
        " db_web.Sysports.osName AS port_os_name, db_web.Connchannels.alias AS actuator_alias, db_web.Conndevices.alias AS device_alias," \
        " db_web.Cdprotos.login AS channel_login, db_web.Cdprotos.passwd AS channel_passwd " \
        " FROM db_web.Connchannels INNER JOIN db_web.Conndevices ON db_web.Conndevices.id = db_web.Connchannels.conndeviceId" \
        " INNER JOIN db_web.Cdprotos ON db_web.Cdprotos.id = db_web.Connchannels.cdprotoId" \
        " INNER JOIN db_web.Sysports ON db_web.Sysports.Id = db_web.Cdprotos.portId WHERE db_web.Connchannels.Id = ?";

    //rc_web = sqlite3_prepare_v2(db_web, sql, -1, &res, 0);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    //if (rc_web == SQLITE_OK) {
    if (rc == SQLITE_OK) {
        //          it's parameterized query
        sqlite3_bind_int(res, 1, conn_chann_id);
    } else {
        //fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db_web));
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        // error handler
        log_msg = malloc(60*sizeof(char));
        sprintf(log_msg, "!!! Failed to execute SQL statement in get_sensor_data() !!!");
        // TODO logger_er()
        logger(log_msg);
        free(log_msg);
        return -1;
    }

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sUsing web conf database for sensor\n", THIS_FILE);

    //          take first record only !!!
    int step = sqlite3_step(res);
    //printf("step = %d\n", step);
    if (step == SQLITE_ROW) {
        if (NEED_FULL_LOG) print_time();
        fprintf(stdout, "SENSOR information: ");
        for (j = 0; j < sqlite3_column_count(res); j++) {
            fprintf(stdout, "%s: %s|", sqlite3_column_name(res,j), sqlite3_column_text(res, j)); 
            //  Set a REDIS keys 
            if ( sqlite3_column_text(res, j) )
                reply = redisCommand(c,"SET %s %s", sqlite3_column_name(res,j), sqlite3_column_text(res, j));
            else 
                reply = redisCommand(c,"SET %s %s", sqlite3_column_name(res,j), "");

            //printf("SET: %s\n", reply->str);
            freeReplyObject(reply);
        }
        //      for sensor, io = 0 
        reply = redisCommand(c,"SET io 0");
        freeReplyObject(reply);
        //      Getting information from the actuator is also important, 
        //      because GUI need to understand what state it is in
        if (NEED_FULL_LOG) fprintf(stdout, "<---- %sSENSOR: io is set to 0 (even for output)", THIS_FILE);
        //printf ("<=======  The SENSOR information\n");
    }
    else { 
        //printf("step = %d", step);
        if (NEED_FULL_LOG) fprintf(stderr, "%s!!!Failed to find sensor ccid=%d information in dbase!!!\n", THIS_FILE, conn_chann_id);
        // error handler
        log_msg = malloc(60*sizeof(char));
        sprintf(log_msg, "!!! Failed to find info in get_sensor_data() ccid=%d\n!!!", conn_chann_id);
        // TODO logger_er()
        logger(log_msg);
        free(log_msg);

        return -1;
    }

    //          finalize() must be here
    //sqlite3_finalize(res);

    fprintf(stdout, "\n");
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%slong time query finished\n", THIS_FILE); 


    //          check what protocol is used
    //          now modbus
    if ( strcmp( sqlite3_column_text(res,0), "MODBUS_RTU" ) == 0 ) {
        char buf1[100];
        //strcpy(buf1, "/home/root/leo/modbusrtu/rdmbrtuexe");
        strcpy(buf1, MB_RTU_EXE);
        //strcat(buf1, "");

        int status = system(buf1);
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sModbus RTU executor exit code %d\n", THIS_FILE, status / 256);

        // if error occured
        if(status != 0) return -1;

        //            waiting modbus command execution
        //usleep(10);

        //            get value from redis storage
        //            local_exec_ret_data is the key for 
        //            transferring sensor state from modbus
        reply = redisCommand(c,"GET local_exec_ret_data");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET local_exec_ret_data is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            ret_val = (int)strtod(reply->str, &err);

        } else {
            ret_val = -1;
        }
        freeReplyObject(reply);

        if ( ret_val != -1 ) {
            //          refresh value
            reply = redisCommand(c, "SET ccstate_%d %d", conn_chann_id, ret_val);
            if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
                // TODO error log
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET ccstate_%d!\n", THIS_FILE, conn_chann_id);
                // next time - get val from hardware
                // need to poll this sensor
                sensor_state_is_need_to_check[conn_chann_id] == 1;
                goto EndOfGetSensor;

            } else {
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sSET ccstate_%d redis key to %d - %s!\n", THIS_FILE, conn_chann_id, ret_val, reply->str);
            }
            freeReplyObject(reply);
        }
    } else if ( strcmp( sqlite3_column_text(res,0), "MODBUS_TCP" ) == 0 ) {
        char buf1[100];
        strcpy(buf1, "/home/root/leo/modbustcp/rdmbtcpexe");
        //strcat(buf1, "");

        int status = system(buf1);
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sModbus TCP executor exit code %d\n", THIS_FILE, status / 256);

        // if error occured
        if(status != 0) return -1;

        //            waiting modbus command execution
        //usleep(10);

        //            get value from redis storage
        //            local_exec_ret_data is the key for 
        //            transferring sensor state from modbus
        reply = redisCommand(c,"GET local_exec_ret_data");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET local_exec_ret_data is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            ret_val = (int)strtod(reply->str, &err);

        } else {
            ret_val = -1;
        }
        freeReplyObject(reply);

        if ( ret_val != -1 ) {
            //          refresh value
            reply = redisCommand(c, "SET ccstate_%d %d", conn_chann_id, ret_val);
            if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
                // TODO error log
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET ccstate_%d!\n", THIS_FILE, conn_chann_id);
                // next time - get val from hardware
                // need to poll this sensor
                sensor_state_is_need_to_check[conn_chann_id] == 1;
                goto EndOfGetSensor;

            } else {
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sSET ccstate_%d redis key to %d - %s!\n", THIS_FILE, conn_chann_id, ret_val, reply->str);
            }
            freeReplyObject(reply);
        }
    }
    else if ( strcmp( sqlite3_column_text(res,0), "MQTT" ) == 0 ) {
        //          check if it is the input
        if ( strcmp( sqlite3_column_text(res,3), "0" ) == 0 ) {
            //      then in ccstate_N(ccid) value of sensor is already
            //      if process is running, of course

            // now we get PID of proc which is sucbscribed to topic of ccid
            reply = redisCommand(c,"GET ccpid_%d", conn_chann_id);
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccpid_%d: %s!\n", THIS_FILE, conn_chann_id, reply->str);
            if ( reply->type != REDIS_REPLY_NIL ) {
                //   check read_mqtt_ccid_topic process
                char proc_path[100];
                sprintf(proc_path, "/proc/%s", reply->str);
                int pid_val = (int)strtod(reply->str, &err);

                // see Kerrick page 432

                if ( kill(pid_val, 0)!=0 ) {
                    usleep(500000);
                    // see Kerrick page 259
                    int fd = open(proc_path, O_RDONLY);
                    if (fd != -1) {
                        if (NEED_FULL_LOG) print_time();
                        if (NEED_FULL_LOG) fprintf(stdout, "%sprocess read_mqtt_ccid_topic %d: is running!\n", THIS_FILE, conn_chann_id);
                        ret_val = 1;
                    }
                    else {
                        if (NEED_FULL_LOG) print_time();
                        if (NEED_FULL_LOG) fprintf(stdout, "%sprocess read_mqtt_ccid_topic %d: is NOT running (but key in redis is present)!\n", THIS_FILE, conn_chann_id);
                        //exit(1);
                        if (NEED_FULL_LOG) fprintf(stdout, "%sstart new process read_mqtt_ccid_topic_%d!\n", THIS_FILE, conn_chann_id);
                        //   for start new process form command string
                        char buf1[100];

                        char *err;
                        char *token_addr;
                        char *rest;
                        //   address and port in one field... split this
                        rest = malloc(sizeof(char)*100);   
                        strcpy(rest, sqlite3_column_text(res,4));
                        token_addr = strtok_r(rest, ":", &rest);

                        int port = strtod(rest, &err);
                        // TODO error handler!!!

                        // parameters: ccid, topic (channel_addr field), broker_addr, port, login, passwd
                        sprintf (buf1, "/home/root/leo/mosquitto/rdmqtt_ccid_topic_read %d %s %s %d %s %s", conn_chann_id, sqlite3_column_text(res,2), token_addr, port, sqlite3_column_text(res,10), sqlite3_column_text(res,11));
                        if (NEED_FULL_LOG) fprintf(stdout, "%scommand is: %s\n", THIS_FILE, buf1);
                        //   see Kerrisk page 535
                        pid_t moschild;

                        switch (moschild = fork()) {
                        case -1:
                            // error
                            ret_val = -1;
                            break;
                        case 0:
                            system(buf1);
                            usleep(500000);
                            ret_val = 1;
                            break;
                        default:
                            ;
                        }
                    }

                }
                else {
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sprocess read_mqtt_ccid_topic %d: is running!\n", THIS_FILE, conn_chann_id);
                    ret_val = 1;
                }
            }
            else {
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sprocess read_mqtt_ccid_topic %d: is NOT running (key in redis not present)!\n", THIS_FILE, conn_chann_id);
                if (NEED_FULL_LOG) fprintf(stdout, "%sstart new process read_mqtt_ccid_topic_%d!\n", THIS_FILE, conn_chann_id);
                //   for start new process form command string
                char buf1[100];

                char *err;
                char *token_addr;
                char *rest;
                //   address and port in one field... split this
                rest = malloc(sizeof(char)*100);   
                strcpy(rest, sqlite3_column_text(res,4));
                token_addr = strtok_r(rest, ":", &rest);

                int port = strtod(rest, &err);
                // TODO error handler!!!

                // parameters: ccid, topic (channel_addr field), broker_addr, port, login, passwd
                sprintf (buf1, "/home/root/leo/mosquitto/rdmqtt_ccid_topic_read %d %s %s %d %s %s", conn_chann_id, sqlite3_column_text(res,2), token_addr, port, sqlite3_column_text(res,10), sqlite3_column_text(res,11));
                if (NEED_FULL_LOG) fprintf(stdout, "%scommand is: %s\n", THIS_FILE, buf1);
                //   see Kerrisk page 535
                pid_t moschild;

                switch (moschild = fork()) {
                case -1:
                    // error
                    ret_val = -1;
                    break;
                case 0:
                    system(buf1);
                    ret_val = 1;
                    usleep(500000);
                    break;
                default:
                    ;
                }

            }
            freeReplyObject(reply);

        }
        //          output handler
        else {
            long sensor_ccid;
            sensor_ccid = get_cc_sens_id(conn_chann_id);
            if ( sensor_ccid ) {
                get_sensor_data ( sensor_ccid );
                //      copy key value
                //      now we looking for ccstate_N keys
                //
                reply1 = redisCommand(c, "GET ccstate_%d", sensor_ccid);
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld for copying sensor value to actuator (for GUI needs) is %s\n", THIS_FILE, sensor_ccid, reply1->str);
                if ( reply1->type != REDIS_REPLY_NIL ) {
                    redisCommand(c, "SET ccstate_%d %s", conn_chann_id, reply1->str);
                    ret_val = 1;
                }
                else {
                    redisCommand(c, "SET ccstate_%d %s", conn_chann_id, "unknown");
                    ret_val = -1;
                }
                freeReplyObject(reply1);
            } else {
                redisCommand(c, "SET ccstate_%d %s", conn_chann_id, "unknown");
                ret_val = -1;
            }
        }
    }
    sqlite3_finalize(res);

    if ( ret_val != -1 ) {
        //          redis - refresh timestamp
        reply = redisCommand(c,"SET cctimestamp_%d %d", conn_chann_id, ltime());
        freeReplyObject(reply);

        //        next time not need to poll this sensor
        sensor_state_is_need_to_check[conn_chann_id] = 0;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%ssensor_state_is_need_to_check[%d] is set to %d!\n", THIS_FILE, conn_chann_id, sensor_state_is_need_to_check[conn_chann_id]);
    }


    //            TODO may be exec_returned_value must reset to 0??

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%slocal executor work finished\n", THIS_FILE); 

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sget_sensor_data() returned %d (-1 is bad)!\n", THIS_FILE, ret_val); 

EndOfGetSensor:
    return ret_val;

}

//          function set value to actuator
//          returned value -1  - error
//
int set_actuator (int conn_chann_id, int value) {

    unsigned int j;
    redisReply *reply;
    char *err;
    // TODO err_msg
    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    int ret_val;
    char *log_msg, log_addr[100];

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%slong time query started\n", THIS_FILE); 

    //         prepare query for get Actuator information
    //
    sql = "SELECT db_web.Cdprotos.name AS proto, db_web.Connchannels.funct AS function, db_web.Connchannels.channelAddr AS channel_addr," \
        " db_web.Connchannels.io AS io, db_web.Cdprotos.portSpeed AS port_speed, db_web.Cdprotos.portParams AS port_params," \
        " db_web.Cdprotos.paddress AS device_addr," \
        " db_web.Sysports.osName AS port_os_name, db_web.Connchannels.alias AS actuator_alias, db_web.Conndevices.alias AS device_alias," \
        " db_web.Cdprotos.login AS channel_login, db_web.Cdprotos.passwd AS channel_passwd " \
        " FROM db_web.Connchannels INNER JOIN db_web.Conndevices ON db_web.Conndevices.id = db_web.Connchannels.conndeviceId" \
        " INNER JOIN db_web.Cdprotos ON db_web.Cdprotos.id = db_web.Connchannels.cdprotoId" \
        " INNER JOIN db_web.Sysports ON db_web.Sysports.Id = db_web.Cdprotos.portId WHERE db_web.Connchannels.Id = ?";

    //rc_web = sqlite3_prepare_v2(db_web, sql, -1, &res, 0);
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    //if (rc_web == SQLITE_OK) {
    if (rc == SQLITE_OK) {
        //     it's parameterized query
        sqlite3_bind_int(res, 1, conn_chann_id);
    } else {
        //if (NEED_FULL_LOG) fprintf(stderr, "%sFailed to execute statement: %s\n", THIS_FILE, sqlite3_errmsg(db_web));
        if (NEED_FULL_LOG) fprintf(stderr, "%sFailed to execute statement: %s\n", THIS_FILE, sqlite3_errmsg(db));
        // error handler
        log_msg = malloc(60*sizeof(char));
        sprintf(log_msg, "!!! Failed to execute SQL statement in set_actuator() !!!");
        // TODO logger_er()
        logger(log_msg);
        free(log_msg);

        return -1;
    }

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sUsing web conf database for actuator, ccid=%d\n", THIS_FILE, conn_chann_id);

    //         take first record only !!!
    int step = sqlite3_step(res);
    if (step == SQLITE_ROW) {
        if (NEED_FULL_LOG) print_time();
        fprintf(stdout, "ACTUATOR information: ");
        for (j = 0; j < sqlite3_column_count(res); j++) {
            printf("%s: %s|", sqlite3_column_name(res,j), sqlite3_column_text(res, j)); 
            // Set a REDIS keys 
            reply = redisCommand(c,"SET %s %s", sqlite3_column_name(res,j), sqlite3_column_text(res, j));
            //printf("SET: %s\n", reply->str);
            freeReplyObject(reply);
        }
    }
    else { 
        //printf("step = %d", step);
        if (NEED_FULL_LOG) fprintf(stderr, "%s!!!Failed to find actuator information ccid=%d in dbase!!!\n", THIS_FILE, conn_chann_id);
        // error handler
        log_msg = malloc(60*sizeof(char));
        sprintf(log_msg, "!!! Failed to find info in set_actuator() ccid=%d !!!\n", conn_chann_id);
        // TODO logger_er()
        logger(log_msg);
        free(log_msg);

        return -1;
    }

    fprintf (stdout, "\n");
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf (stdout, "%sActuator is ready to set VALUE: %d\n", THIS_FILE, value);

    //          set value in redis storage
    reply = redisCommand(c, "SET modbus_data %d", value);
    freeReplyObject(reply);

    //          story log_addr
    //
    sprintf(log_addr, "protocol %s addr %s, channel addr %s, %s", sqlite3_column_text (res, 0), sqlite3_column_text(res, 5), sqlite3_column_text(res, 2), sqlite3_column_text(res, 1));

    //sqlite3_finalize(res);


    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%slong time query finished\n", THIS_FILE); 

    //          check what protocol is used
    //          and do work !!!
    if ( strcmp( sqlite3_column_text(res,0), "MODBUS_RTU" ) == 0 ) {
        char buf1[100];
        //strcpy(buf1, "/home/root/leo/modbusrtu/rdmbrtuexe");
        strcpy(buf1, MB_RTU_EXE);
        //strcat(buf1, "");

        int status = system(buf1);
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sModbus RTU executor exit code %d\n", THIS_FILE, status / 256);

        // if error occured
        if(status != 0) return -1; 

        //      waiting command execution
        //usleep(50);
        //      executor returned data handler
        //      get value from redis storage
        //      local_exec_ret_data is the key for 
        //      transferring register state from modbus
        reply = redisCommand(c,"GET local_exec_ret_data");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET local_exec_ret_data is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            // actuator not only set ON or OFF - in redis key local_exec_ret_data may be value of HR
            ret_val = (int)strtod(reply->str, &err);

        } else {
            ret_val = -1;
        }

        freeReplyObject(reply);

    } else if ( strcmp( sqlite3_column_text(res,0), "MODBUS_TCP" ) == 0 ) {
        // TODO think about aggregate RTU and TCP
        char buf1[100];
        strcpy(buf1, "/home/root/leo/modbustcp/rdmbtcpexe");
        //strcat(buf1, "");

        int status = system(buf1);
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sModbus TCP executor exit code %d\n", THIS_FILE, status / 256);

        // if error occured
        if(status != 0) return -1; 

        //      waiting command execution
        //usleep(50);
        //      executor returned data handler
        //      get value from redis storage
        //      local_exec_ret_data is the key for 
        //      transferring register state from modbus
        reply = redisCommand(c,"GET local_exec_ret_data");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET local_exec_ret_data is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            // actuator not only set ON or OFF - in redis key local_exec_ret_data may be value of HR
            ret_val = (int)strtod(reply->str, &err);

        } else {
            ret_val = -1;
        }

        freeReplyObject(reply);
    }
    else if ( strcmp( sqlite3_column_text(res,0), "MQTT" ) == 0 ) {
        //   for start new process form command string
        char buf1[100];

        char *err;
        char *token_addr;
        char *rest;
        //   address and port in one field... split this
        rest = malloc(sizeof(char)*100);   
        strcpy(rest, sqlite3_column_text(res,4));
        token_addr = strtok_r(rest, ":", &rest);

        int port = strtod(rest, &err);
        // TODO error handler!!!

        // paddress (device_addr) field (addr+port), topic (channel_addr) field and data (value variable) + login/passwd
        sprintf (buf1, "mosquitto_pub -h %s -p %d -t %s -m %d -u %s -P %s", token_addr, port, sqlite3_column_text(res,2), value, sqlite3_column_text(res,10), sqlite3_column_text(res,11));

        int status = system(buf1);
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%s%s executor exit code %d\n", THIS_FILE, buf1, status / 256);

        // if error occured
        if(status != 0) return -1;
        else ret_val = 1;
    }
    sqlite3_finalize(res);

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%slocal executor work finished\n", THIS_FILE); 

    //          local_exec_ret_data value is returned value

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sset_actuator() returned %d\n", THIS_FILE, ret_val); 

    if ( ret_val >= 0 ) {
        log_msg = malloc(160*sizeof(char));
        sprintf(log_msg, "SET ACTUATOR %s to %d", log_addr, ret_val);
        // TODO logger_er()
        logger(log_msg);
        free(log_msg);

    }
    //          if state any actuator is changed - refresh sensors needed
    //          TODO think about this - how to glue sensor state and actuator
    //          they have different connchannelId
    //if ( ret_val != -1) fill_sensor_state_changes_ones();

    return ret_val;

}

//          store Timer name and value in redis key
//
int set_timer_in_redis(char * name, long value) {
    redisReply *reply;
    int ret_val = 1;

    if (NEED_FULL_LOG) fprintf(stdout, "%sstart set_timer_in_redis()\n", THIS_FILE);

    //char * name_ptr;
    //name_ptr = malloc(sizeof(char)*30);
    long ltimeplusvalue = ltime() + value;
    reply = redisCommand(c, "SET tmr_%s %d#%ld", name, value, ltimeplusvalue);
    if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
        // TODO error log
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET tmr_%s!\n", THIS_FILE, name);
        ret_val = -1;

    } else {
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sSET tmr_%s to %ld - %s!\n", THIS_FILE, name, value, reply->str);
    }

    freeReplyObject(reply);

    return ret_val;

}

//          get Timer from redis key by name
//
int get_timer_from_redis(char * name) {
    char *err;
    redisReply *reply;
    int ret_val;

    if (NEED_FULL_LOG) fprintf(stdout, "%sstart get_timer_from_redis()\n", THIS_FILE);

    reply = redisCommand(c, "GET tmr_%s", name);
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sGET tmr_%s %s\n", THIS_FILE, name, reply->str);

    if ( reply->type != REDIS_REPLY_NIL ) {
        ret_val = (int)strtod(reply->str, &err);

    } else {
        // from -32768 to 32767 is right 
        ret_val = -32769;
    }
    freeReplyObject(reply);

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%s get_timer_from_redis() return %d\n", THIS_FILE, ret_val);

    return ret_val;
}

//          store variable name and value in redis key
//
int set_variable_in_redis(char * name, long value) {
    redisReply *reply;
    int ret_val = 1;

    if (NEED_FULL_LOG) fprintf(stdout, "%sstart set_variable_in_redis()\n", THIS_FILE);

    //char * name_ptr;
    //name_ptr = malloc(sizeof(char)*30);
    reply = redisCommand(c, "SET var_%s %d", name, value);
    if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
        // TODO error log
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET var_%s!\n", THIS_FILE, name);
        ret_val = -1;

    } else {
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sSET var_%s to %ld - %s!\n", THIS_FILE, name, value, reply->str);
    }

    //free (name_ptr);
    freeReplyObject(reply);

    return ret_val;

}

//          get variable from redis key by name
//
int get_variable_from_redis(char * name) {
    char *err;
    redisReply *reply;
    int ret_val;

    if (NEED_FULL_LOG) fprintf(stdout, "%sstart get_variable_from_redis()\n", THIS_FILE);

    reply = redisCommand(c, "GET var_%s", name);
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sGET var_%s is %s\n", THIS_FILE, name, reply->str);

    if ( reply->type != REDIS_REPLY_NIL ) {
        ret_val = (int)strtod(reply->str, &err);

    } else {
        // from -32768 to 32767 is right 
        ret_val = -32769;
    }
    freeReplyObject(reply);

    return ret_val;
}

//          set statistic for ccid (any sensor)
//
int set_statistic(long ccid, int val) {
    sqlite3_stmt *res;
    int ret_val;
    char *sql = "INSERT INTO db_web.Statistics VALUES(NULL, ?, ?, ?);";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        //  it's parameterized query
        sqlite3_bind_int(res, 1, ccid);
        sqlite3_bind_int(res, 2, ltime());
        sqlite3_bind_int(res, 3, val);

    } else {
        //TODO error log
        fprintf(stderr, "Failed to execute statement INSERT INTO Statistics : %s\n", sqlite3_errmsg(db));
        return -1;
    }

    int step = sqlite3_step(res);
    if (step == SQLITE_DONE) {
        ret_val = 1;
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sINSERT INTO Statistics table is OK!\n", THIS_FILE);
    } else {
        ret_val = -1;
        fprintf(stdout, "%s!!!INSERT INTO Statistics table is NOT OK!!\n", THIS_FILE);
    }

    sqlite3_finalize(res);

    return ret_val;
}

//          casting a value from RGB to the actuator 0...max
//
int cast_rgb_value(int color_intensity, long ccid) {
    char *err_msg = 0;
    sqlite3_stmt *res;
    int ret_val, max_val;
    char *sql = "SELECT max FROM db_web.Webccsettings WHERE ccid=?;";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        //  it's parameter
        sqlite3_bind_int(res, 1, ccid);

    } else {
        //TODO error log
        fprintf(stderr, "Failed to execute statement SELECT * FROM Webccsettings: %s\n", sqlite3_errmsg(db));
        return color_intensity;
    }

        int step = sqlite3_step(res);
        if (step == SQLITE_ROW) {
            max_val = sqlite3_column_int(res, 0);
            if ( max_val > 1 ) {
            // recalculate value
                ret_val = (int)(color_intensity * max_val / 255);   // because 255 is max value for RGB
            }
            else ret_val = color_intensity;
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sWebccsettings for actuator id=%ld exist in table!\n", THIS_FILE, ccid);
        } else {
            // if now record in webccsettings table ret_val = input_val
            ret_val = color_intensity;
            if (NEED_FULL_LOG) print_time();
            fprintf(stdout, "%sNo settings with actuator id=%ld in webccsettings table!\n", THIS_FILE, ccid);
        }

    sqlite3_finalize(res);

    if (ret_val > max_val) ret_val = max_val;

    return ret_val;
}

//          handle parameters of action
//          if something wrong returned value -1
//
int parameters_handler (int actid, int operid/*, int act_is_once*/) {

    char *err_msg = 0;
    sqlite3_stmt *res, *res2;
    char *sql;
    long param[12];                     // 12 integer parameters
    char pvalue1[50];                   // char pvalue1
    char pvalue2[50];                   // char pvalue2
    char pvalue3[50];                   // char pvalue3
    char pvalue4[50];                   // char pvalue4
    char pvalue5[50];                   // char pvalue5
    char pvalue6[50];                   // char pvalue6
    int ret_val;
    long rtc_intrval;                   // interval used in RTC handler (EveryMinute, Hourly, Daily, etc.)
    int tmp, tmp1, tmp2;
    int condition, result;
    redisReply *reply;
    char *err;
    int ival, ival1;

    //     prepare and execute query for Parameters table
    //
    sql =  "SELECT id, pnumber, pvalue, ptype, ptag, connchannelId, pvariableType, ptimerRepeater, lastExecTime FROM Parameters WHERE actionId=?";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

    if (rc == SQLITE_OK) {
        // this is parameterized query: ? (first parameter) - in sql replace with actid
        sqlite3_bind_int(res, 1, actid);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    //     moving by Parameters table
    //

    //int rec_count = 0;

    while (sqlite3_step(res) == SQLITE_ROW) {

        if (NEED_FULL_LOG) fprintf(stdout, "%s%s|", THIS_FILE, sqlite3_column_text(res, 0));

        if (NEED_FULL_LOG) fprintf(stdout, "%s|", sqlite3_column_text(res, 2));
        if (NEED_FULL_LOG) fprintf(stdout, "%s|", sqlite3_column_text(res, 3));
        if (NEED_FULL_LOG) fprintf(stdout, "%s|", sqlite3_column_text(res, 4));
        if (NEED_FULL_LOG) fprintf(stdout, "%s|", sqlite3_column_text(res, 5));
        if (NEED_FULL_LOG) fprintf(stdout, "%s|", sqlite3_column_text(res, 7));
        if (NEED_FULL_LOG) fprintf(stdout, "%s\n", sqlite3_column_text(res, 8));

        //  Commands parameters pusher 
        switch (operid) {
            case 0: {
                if (NEED_FULL_LOG) fprintf(stdout, "%s^------ This is ALG_NOP parameter %s\n", THIS_FILE, sqlite3_column_text(res, 1));
                break;
            }
            case 9: { 
                if (NEED_FULL_LOG) fprintf(stdout, "%s^------ This is ALG_RTC parameter %s\n", THIS_FILE, sqlite3_column_text(res, 1));
                switch ( sqlite3_column_int(res, 1) ) {         // pnumber checking
                    // record in query with first parameters (field pnumber=1)
                    case 1: {
                        // store ptag field as time stamp in param[0] always
                        param[0] = sqlite3_column_int(res, 4);
                        // store ptimerRepeater in pvalue1 - this is Once, EveryMinute and so on
                        strcpy(pvalue1, sqlite3_column_text(res, 7));
                        // store lastExecTime field in param[1]
                        param[1] = sqlite3_column_int(res, 8);
                        // store id field of Parameters table in param[3]
                        param[3] = sqlite3_column_int(res, 0);
                        // store extra time (pvalue field) of Parameters table in param[6]
                        if ( sqlite3_column_type(res, 2) == SQLITE_NULL || strcmp(sqlite3_column_text(res, 2), "") == 0 ) param[6] = 0;
                        else param[6] = sqlite3_column_int(res, 2);
                        break;
                    }
                    // record in query with additional parameters (field pnumber=2)
                    case 2: {
                        // type of additional parameters in pvalue2
                        strcpy(pvalue2, sqlite3_column_text(res, 3));
                        // it's Program Counter
                        //if ( strcmp(sqlite3_column_text(res, 3), "PC") == 0 ){
                        if ( strcmp(pvalue2, "Call") == 0 || strcmp(pvalue2, "Goto") == 0 ){
                            // store ptag field as address subroutine in param[2]
                            param[2] = sqlite3_column_int(res, 4);
                        }
                        // it's Logging procedure
                        //if ( strcmp(sqlite3_column_text(res, 3), "Log") == 0 ){
                        if ( strcmp(pvalue2, "Log") == 0 ){
                            // store pvalue field as name of Log procedure in pvalue3
                            strcpy(pvalue3, sqlite3_column_text(res, 2));
                        }
                        break;
                    }
                    // for RTC/Set variant additional parameters more then in Log, Call, Goto
                    case 3: {
                        if ( strcmp(pvalue2, "Set") == 0 ){
                            // RTC/Set Actuator here
                            if ( strcmp(sqlite3_column_text(res, 3), "Actuator") == 0 ) {
                                // store connchannelId in param[4]
                                param[4] = sqlite3_column_int(res, 5);
                            }
                        }
                        break;
                    }
                    case 4: {
                        if ( strcmp(pvalue2, "Set") == 0 ){
                            // RTC/Set Sensor here
                            // store type of 2 Set operand in pvalue3 - this is Constant, Sensor and so on
                            strcpy(pvalue3, sqlite3_column_text(res, 3));
                            if ( strcmp(sqlite3_column_text(res, 3), "Constant") == 0 ){
                                param[5] = sqlite3_column_int(res, 4);
                            }
                            // RTC/Set Constant
                            if ( strcmp(sqlite3_column_text(res, 3), "Sensor") == 0 ){
                                // store connchannelId in param[5]
                                param[5] = sqlite3_column_int(res, 5);
                            }
                        }
                        break;
                    }
                }
                break;
            }

            case 11: { 
                if (NEED_FULL_LOG) fprintf(stdout, "%s^------ This is ALG_BITS parameter %s\n", THIS_FILE, sqlite3_column_text(res, 1));
                switch ( sqlite3_column_int(res, 1) ) {         // pnumber checking
                    // record in query with additional parameters (field pnumber=1)
                    case 1: {
                        // (~|&) stored in pvalue field
                        strcpy(pvalue1, sqlite3_column_text(res, 2));

                        break;
                    }
                    // record in query with first parameters (field pnumber=2)
                    case 2: {
                        // parameter type field for A =       ...it's result
                        strcpy(pvalue2, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Actuator") == 0 ) {
                            // store connchannelId
                            param[0] = sqlite3_column_int(res, 5);
                        } else if ( strcmp(sqlite3_column_text(res, 3), "Variable") == 0 ){
                            // store variable name (field pvalue) in pvalue3
                            strcpy(pvalue3, sqlite3_column_text(res, 2));
                        }
                        break;
                    }
                    // record in query with additional parameters (field pnumber=3)
                    case 3: {
                        // parameter type field for B
                        strcpy(pvalue4, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Sensor") == 0 ){
                            // store connchannelId in param[1]
                            param[1] = sqlite3_column_int(res, 5);
                        } else if ( strcmp(sqlite3_column_text(res, 3), "Variable") == 0 ){
                            // store variable name (field pvalue) in pvalue5
                            strcpy(pvalue5, sqlite3_column_text(res, 2));
                        }
                        break;
                    }
                    case 4: {
                        // parameter type field for C
                        strcpy(pvalue6, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Sensor") == 0 ){
                            // store connchannelId in param[2]
                            param[2] = sqlite3_column_int(res, 5);
                        } else if ( strcmp(sqlite3_column_text(res, 3), "Constant") == 0 ){
                            // store ptag field with value of constant in param[2]
                            param[2] = sqlite3_column_int(res, 4);
                        }
                        break;
                   }
                }
                break;
            }

            case 12: { 
                if (NEED_FULL_LOG) fprintf(stdout, "%s^------ This is ALG_SET parameter %s\n", THIS_FILE, sqlite3_column_text(res, 1));
                switch ( sqlite3_column_int(res, 1) ) {         // pnumber checking
                    // record in query with first parameter (field pnumber=1)
                    case 1: {
                        // in pvalue1 store ptype field (Actuator or Variable)
                        strcpy(pvalue1, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Actuator") == 0 ) {
                            // store connchannelId
                            param[0] = sqlite3_column_int(res, 5);
                        } else if ( strcmp(sqlite3_column_text(res, 3), "Variable") == 0 || strcmp(sqlite3_column_text(res, 3), "Timer") == 0 ) {
                            // store variable name (field pvalue) in pvalue3
                            strcpy(pvalue3, sqlite3_column_text(res, 2));
                        }
                        break;
                    }
                    // record in query with additional parameters (field pnumber=2)
                    case 2: {
                        // store type of 2 operand in pvalue2 - this is Constant, Sensor and so on
                        strcpy(pvalue2, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Constant") == 0 ){
                            // store ptag with value of constant in param[1] 
                            param[1] = sqlite3_column_int(res, 4);
                        }
                        if ( strcmp(sqlite3_column_text(res, 3), "Sensor") == 0 ){
                            // store connchannelId in param[1]
                            param[1] = sqlite3_column_int(res, 5);
                        }
                        if ( strcmp(sqlite3_column_text(res, 3), "Variable") == 0 ){
                            // store variable name (field pvalue) in pvalue4
                            strcpy(pvalue4, sqlite3_column_text(res, 2));
                        }
                        break;
                    }
                }
                break;
            }

            case 13: { 
                if (NEED_FULL_LOG) fprintf(stdout, "%s^------ This is ALG_LOGIC parameter %s\n", THIS_FILE, sqlite3_column_text(res, 1));
                switch ( sqlite3_column_int(res, 1) ) {         // pnumber checking
                    // record in query with first parameter (field pnumber=1)
                    case 1: {
                        // in pvalue1 store sign of operation (field pvalue)
                        strcpy(pvalue1, sqlite3_column_text(res, 2));
                        break;
                    }
                    // record in query with first parameter (field pnumber=2)
                    case 2: {
                        // in pvalue2 store ptype field of operand 1 (Sensor or Variable)
                        strcpy(pvalue2, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Sensor") == 0 ) {
                            // store connchannelId in param[0]
                            param[0] = sqlite3_column_int(res, 5);
                        } else if ( strcmp(sqlite3_column_text(res, 3), "Variable") == 0 || strcmp(sqlite3_column_text(res, 3), "Timer") == 0 ) {
                            // store variable name (field pvalue) in pvalue3
                            strcpy(pvalue3, sqlite3_column_text(res, 2));
                        }
                        break;
                    }
                    // record in query with additional parameters (field pnumber=3)
                    case 3: {
                        // store type of 2 operand in pvalue4 - this is Constant, Sensor
                        strcpy(pvalue4, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Constant") == 0 ){
                            // store ptag with value of constant in param[1] 
                            param[1] = sqlite3_column_int(res, 4);
                        }
                        if ( strcmp(sqlite3_column_text(res, 3), "Sensor") == 0 ){
                            // store connchannelId in param[1]
                            param[1] = sqlite3_column_int(res, 5);
                        }
                        break;
                    }
                    // record in query with additional parameters (field pnumber=4)
                    case 4: {
                        // store type of operand 4 in pvalue5 - this is Call, Log, Goto
                        strcpy(pvalue5, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Log") == 0 ){
                            // store pvalue with Log string in pvalue6 
                            strcpy(pvalue6, sqlite3_column_text(res, 2));
                        } else {
                            // store ptag field in param[2]
                            param[2] = sqlite3_column_int(res, 4);
                        }
                        break;
                    }
                }
                break;
            }

            case 15: { 
                if (NEED_FULL_LOG) fprintf(stdout, "%s^------ This is RGB2A parameter %s\n", THIS_FILE, sqlite3_column_text(res, 1));
                switch ( sqlite3_column_int(res, 1) ) {         // pnumber checking
                    // record in query with first parameter (field pnumber=1)
                    case 1: {
                        // in pvalue1 store variable name for RGB value (field pvalue)
                        strcpy(pvalue1, sqlite3_column_text(res, 2));
                        break;
                    }
                    // record in query with additional parameter (field pnumber=2)
                    case 2: {
                        // in pvalue2 store ptype field of operand 1 (Actuator) R channel
                        strcpy(pvalue2, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Actuator") == 0 ) {
                            // store connchannelId in param[0]
                            param[0] = sqlite3_column_int(res, 5);
                        }
                        break;
                    }
                    // record in query with additional parameters (field pnumber=3)
                    case 3: {
                        // in pvalue3 store ptype field of operand 2 (Actuator) G channel
                        strcpy(pvalue3, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Actuator") == 0 ) {
                            // store connchannelId in param[1]
                            param[1] = sqlite3_column_int(res, 5);
                        }
                        break;
                    }
                    // record in query with additional parameters (field pnumber=3)
                    case 4: {
                        // in pvalue3 store ptype field of operand 3 (Actuator) B channel
                        strcpy(pvalue4, sqlite3_column_text(res, 3));
                        if ( strcmp(sqlite3_column_text(res, 3), "Actuator") == 0 ) {
                            // store connchannelId in param[2]
                            param[2] = sqlite3_column_int(res, 5);
                        }
                        break;
                    }
                }
                break;
            }

            case 17: {
                if (NEED_FULL_LOG) fprintf(stdout, "%s^------ This is ALG_RETURN parameter %s\n", THIS_FILE, sqlite3_column_text(res, 1));

                break;
            }

            case 1000: {
                if (NEED_FULL_LOG) fprintf(stdout, "%s^------ This is ALG_END parameter %s\n", THIS_FILE, sqlite3_column_text(res, 1));
                // scen_end = 1;
                break;
            }

        }  //  Commands parameters pusher end 

        if (NEED_FULL_LOG) fprintf(stdout, "%s---------------\n", THIS_FILE);

        //rec_count++;
    }

    //        freeing DBase
    sqlite3_finalize(res);

    //    Commands interpreter here
    switch (operid) {
        case 0: {
            // ALG_NOP handler
            //
            ret_val = 1;
            break;
        }
        case 9: {
            // ALG_RTC handler
            //
            //printf ("RTC handler start!!!\n");
            if ( strcmp (pvalue1, "Once") == 0 ) {
                rtc_intrval = ltime();
            } else if ( strcmp (pvalue1, "Every5Seconds") == 0 ) {
                rtc_intrval = FIVE_SECONDS;
            } else if ( strcmp (pvalue1, "Every10Seconds") == 0 ) {
                rtc_intrval = TEN_SECONDS;
            } else if ( strcmp (pvalue1, "EveryMinute") == 0 ) {
                rtc_intrval = MINUTE;
            } else if ( strcmp (pvalue1, "Hourly") == 0 ) {
                rtc_intrval = HOUR;
            } else if ( strcmp (pvalue1, "Daily") == 0 ) {
                rtc_intrval = DAY;
            } else if ( strcmp (pvalue1, "Weekly") == 0 ) {
                rtc_intrval = WEEK;
            } else if ( strcmp (pvalue1, "Monthly") == 0 ) {
                rtc_intrval = MONTH;
            }

            ret_val = 1;
            // RTC debug information!!!
            if (NEED_FULL_LOG) fprintf(stdout, "ltime() = %ld\n", ltime());
            if (NEED_FULL_LOG) fprintf(stdout, "rtc_intrval = %ld\n", rtc_intrval);
            if (NEED_FULL_LOG) fprintf(stdout, "Rest (long)((timenow - param[0]) %% rtc_intrval ) = %ld\n", (long)((ltime() - param[0]) % rtc_intrval ));
            if (NEED_FULL_LOG) fprintf(stdout, "(long)(start_main_cycle_time - timenow + MAIN_LOOP_DELAY/1000000 + main_cycle_time + param[6] + 1) = %ld\n", (long)(start_main_cycle_time - ltime() + MAIN_LOOP_DELAY/1000000 + main_cycle_time + param[6] + 1));
            if (NEED_FULL_LOG) fprintf(stdout, "After last event (ltime() - param[1] - param[6]) = %ld\n", (long)(ltime() - param[1] - param[6]) );
            if (NEED_FULL_LOG) fprintf(stdout, "Next event (param[1] + rtc_intrval) = %ld\n",  (long)(param[1] + rtc_intrval));

            if ( param[1] == 0 ) {
                // if it was first time 
                // main_cycle_time and other additions must to help don't loose important event
                // >= 0 because we need module of tally
                int timenow = ltime();
                // new formula of RTC check
                if ( (long)(timenow - param[0]) >= 0 && (long)((timenow - param[0]) % rtc_intrval ) < (long)(start_main_cycle_time - timenow + MAIN_LOOP_DELAY/1000000 + main_cycle_time + param[6] + 1) ) { 
                //if ( (long)(ltime() - param[0]) >= 0 && (long)((ltime() - param[0]) % rtc_intrval ) < main_cycle_time ) { 
                    if (NEED_FULL_LOG) print_time();
                    fprintf (stdout, "%sEveryMinute, Hourly and other event - OK!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", THIS_FILE);
                    // checking of RTC target type here
                    // if CALL subroutine - last command nStr to stack
                    if ( strcmp (pvalue2, "Call") == 0 ) {
                        //scen_stack = scen_pc;
                        command_stack_push(scen_pc);
                        scen_pc = param[2] - 1;    // -1 because we must touch concrete action in query with "> nStr" 
                    } else
                    if ( strcmp (pvalue2, "Goto") == 0 ) {
                        scen_pc = param[2] - 1;    // -1 because we must touch concrete action in query with "> nStr" 
                    } else
                    if ( strcmp (pvalue2, "Log") == 0 ) {
                        // Log event handler
                        char * log_time;
                        log_time  = malloc(sizeof(char)*30);
                        get_time(log_time);
                        openlog(THIS_FILE, LOG_NDELAY, LOG_LOCAL1);
                        syslog(LOG_INFO, "%s%s log event", log_time, pvalue3);
                        closelog();
                        free(log_time);
                    } else
                    // now it's not work > because Set removed from RTC!!! 
                    if ( strcmp (pvalue2, "Set") == 0 ) {
                        // RTC/Set handler
                        if ( strcmp (pvalue3, "Constant") == 0 ) {
                            if ( set_actuator( param[4], param[5]) >= 0 ) ret_val = 1;
                            else ret_val = -1;
                        }
                        if ( strcmp (pvalue3, "Sensor") == 0 ) {
                            tmp = get_sensor_data( param[5] );
                            if (tmp == -1) {
                                //    error!
                                ret_val = -1;
                                break;
                            }
                            //      now we looking for sstate_N key
                            //
                            reply = redisCommand(c, "GET ccstate_%d", param[5]);
                            if (NEED_FULL_LOG) print_time();
                            if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[5], reply->str);

                            if ( reply->type != REDIS_REPLY_NIL ) {
                                ival = (int)strtod(reply->str, &err);
                                freeReplyObject(reply);
                            } else {
                                //    error!
                                ret_val = -1;
                                freeReplyObject(reply);
                                break;
                            }
                            // error handler
                            //if (set_actuator(param[4], tmp) >= 0 ) ret_val = 1;
                            if (set_actuator(param[4], ival) >= 0 ) ret_val = 1;
                            else ret_val = -1;
                        }
                    }

                    // write ltime to DBase
                    //if ( write_last_exec_time(param[3]) == -1 ) return -1;
                    // write corrected time to DBase
                    if ( write_last_corr_time(param[3], param[0], rtc_intrval) == -1 ) return -1;
                }
            } else {
                if( (ltime() - param[1]) > rtc_intrval || (ltime() > param[1] && ltime() < param[1] + param[6]) ) { 

                    if ( (ltime() - param[0]) > 0 ) {
                        if (NEED_FULL_LOG) print_time();
                        fprintf (stdout, "%sEveryMinute, Hourly and other event - OK!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", THIS_FILE);
                        // checking of RTC target type here
                        // if CALL subroutine - last command nStr to stack
                        if ( strcmp (pvalue2, "Call") == 0 ) {
                            //scen_stack = scen_pc;
                            command_stack_push(scen_pc);
                            scen_pc = param[2] - 1;    // -1 because we must touch concrete action in query with "> nStr" 
                        } else
                        if ( strcmp (pvalue2, "Goto") == 0 ) {
                            scen_pc = param[2] - 1;    // -1 because we must touch concrete action in query with "> nStr" 
                        } else
                        if ( strcmp (pvalue2, "Log") == 0 ) {
                            // Log event handler
                            char * log_time;
                            log_time  = malloc(sizeof(char)*30);
                            get_time(log_time);
                            openlog(THIS_FILE, LOG_NDELAY, LOG_LOCAL1);
                            syslog(LOG_INFO, "%s%s log event", log_time, pvalue3);
                            closelog();
                            free(log_time);
                        } else
                        // now it's not work > because Set removed from RTC!!! 
                        if ( strcmp (pvalue2, "Set") == 0 ) {
                            // RTC/Set handler
                            if ( strcmp (pvalue3, "Constant") == 0 ) {
                                if ( set_actuator( param[4], param[5]) >= 0 ) ret_val = 1;
                                else ret_val = -1;
                            }
                            if ( strcmp (pvalue3, "Sensor") == 0 ) {
                                tmp = get_sensor_data( param[5] );

                                if (tmp == -1) {
                                    //    error!
                                    ret_val = -1;
                                    break;
                                }
                                //      now we looking for sstate_N key
                                //
                                reply = redisCommand(c, "GET ccstate_%d", param[5]);
                                if (NEED_FULL_LOG) print_time();
                                if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[5], reply->str);

                                if ( reply->type != REDIS_REPLY_NIL ) {
                                    ival = (int)strtod(reply->str, &err);
                                    freeReplyObject(reply);

                                } else {
                                    //    error!
                                    ret_val = -1;
                                    freeReplyObject(reply);
                                    break;
                                }
                                // error handler
                                //if (set_actuator(param[4], tmp) >= 0 ) ret_val = 1;
                                if (set_actuator(param[4], ival) >= 0 ) ret_val = 1;
                                else ret_val = -1;
                            }
                        }
                        // write corrected time to DBase
                        if ( write_last_corr_time(param[3], param[0], rtc_intrval) == -1 ) return -1;

                    }
                } 
            }

            break;
        }
        case 10: {
            // ALG_MATH HANDLER
            //
                //    B operand of "A = B operation C"
                if ( strcmp(pvalue4, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[1] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for sstate_N key
                    //
                    reply = redisCommand(c, "GET ccstate_%d", param[1]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }
                } else if ( strcmp(pvalue4, "Variable") == 0 ) {
                    // right value from -32768 to 32767
                    //if (NEED_FULL_LOG) print_time();
                    //if (NEED_FULL_LOG) fprintf(stdout, "%spvalue5 is %s\n", THIS_FILE, pvalue4);
                    tmp = get_variable_from_redis (pvalue5);
                    if ( tmp == -32769 ) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    ival = tmp;
                }
                //    C operand
                if ( strcmp(pvalue6, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[2] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for sstate_N key
                    //
                    reply = redisCommand(c, "GET ccstate_%d", param[2]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[2], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival1 = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }
                } else if ( strcmp(pvalue4, "Constant") == 0 ) {
                    ival1 = param[2];
                }

                if ( strcmp (pvalue1, "+") == 0 ) {
                    // "+" operation
                    result = ival + ival1;
                } else if ( strcmp (pvalue1, "-") == 0 ) {
                    // "-" operation
                    result = ival - ival1;
                } else if ( strcmp (pvalue1, "/") == 0 ) {
                    // "/" handler
                    // TODO whole part
                    result = (int)(ival / ival1);
                } else if ( strcmp (pvalue1, "*") == 0 ) {
                    // "*" handler
                    result = ival * ival1;
                } else if ( strcmp (pvalue1, "%") == 0 ) {
                    // "%" handler
                    result = ival % ival1;
                }

                //    result (A) returned
                if ( strcmp(pvalue2, "Actuator") == 0 ) {
                    // TODO error handler
                    if ( set_actuator( param[0], result) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                } else if ( strcmp(pvalue2, "Variable") == 0 ) {
                    // TODO error handler
                    if ( set_variable_in_redis(pvalue3, result) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }

            break;

        }
        case 11: {
            // ALG_BITS HANDLER
            //

            // unary operation - "logical not"
            if ( strcmp (pvalue1, "~") == 0 ) {
                //    B operand of "A = B ~"
                //if (NEED_FULL_LOG) print_time();
                //if (NEED_FULL_LOG) fprintf(stdout, "%spvalue4 is %s\n", THIS_FILE, pvalue4);
                if ( strcmp(pvalue4, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[1] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for sstate_N key
                    //
                    reply = redisCommand(c, "GET ccstate_%d", param[1]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }
                } else if ( strcmp(pvalue4, "Variable") == 0 ) {
                    // right value from -32768 to 32767
                    //if (NEED_FULL_LOG) print_time();
                    //if (NEED_FULL_LOG) fprintf(stdout, "%spvalue5 is %s\n", THIS_FILE, pvalue4);
                    tmp = get_variable_from_redis (pvalue5);
                    if ( tmp == -32769 ) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    ival = tmp;
                }

                //    logical NOT realized below - result (A) returned
                if ( strcmp(pvalue2, "Actuator") == 0 ) {
                    // TODO error handler
                    if ( set_actuator( param[0], !ival) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                } else if ( strcmp(pvalue2, "Variable") == 0 ) {
                    // TODO error handler
                    if ( set_variable_in_redis(pvalue3, !ival) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
            } else if ( strcmp (pvalue1, "&") == 0 ) {
                //    B operand of "A = B & C"
                if ( strcmp(pvalue4, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[1] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for sstate_N key
                    //
                    reply = redisCommand(c, "GET ccstate_%d", param[1]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }
                } else if ( strcmp(pvalue4, "Variable") == 0 ) {
                    // right value from -32768 to 32767
                    //if (NEED_FULL_LOG) print_time();
                    //if (NEED_FULL_LOG) fprintf(stdout, "%spvalue5 is %s\n", THIS_FILE, pvalue4);
                    tmp = get_variable_from_redis (pvalue5);
                    if ( tmp == -32769 ) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    ival = tmp;
                }
                //    C operand
                if ( strcmp(pvalue6, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[2] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for sstate_N key
                    //
                    reply = redisCommand(c, "GET ccstate_%d", param[2]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[2], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival1 = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }
                } else if ( strcmp(pvalue4, "Constant") == 0 ) {
                    ival1 = param[2];
                }

                //    logical AND realized below - result (A) returned
                if ( strcmp(pvalue2, "Actuator") == 0 ) {
                    // TODO error handler
                    if ( set_actuator( param[0], ival & ival1) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                } else if ( strcmp(pvalue2, "Variable") == 0 ) {
                    // TODO error handler
                    if ( set_variable_in_redis(pvalue3, ival & ival1) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
            } else if ( strcmp (pvalue1, "|") == 0 ) {
                //    B operand of "A = B | C"
                if ( strcmp(pvalue4, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[1] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for sstate_N key
                    //
                    reply = redisCommand(c, "GET ccstate_%d", param[1]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }
                } else if ( strcmp(pvalue4, "Variable") == 0 ) {
                    // right value from -32768 to 32767
                    //if (NEED_FULL_LOG) print_time();
                    //if (NEED_FULL_LOG) fprintf(stdout, "%spvalue5 is %s\n", THIS_FILE, pvalue4);
                    tmp = get_variable_from_redis (pvalue5);
                    if ( tmp == -32769 ) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    ival = tmp;
                }
                //    C operand
                if ( strcmp(pvalue6, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[2] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for sstate_N key
                    //
                    reply = redisCommand(c, "GET ccstate_%d", param[2]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[2], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival1 = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }
                } else if ( strcmp(pvalue4, "Constant") == 0 ) {
                    ival1 = param[2];
                }

                //    logical OR realized below - result (A) returned
                if ( strcmp(pvalue2, "Actuator") == 0 ) {
                    // TODO error handler
                    if ( set_actuator( param[0], ival | ival1) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                } else if ( strcmp(pvalue2, "Variable") == 0 ) {
                    // TODO error handler
                    if ( set_variable_in_redis(pvalue3, ival | ival1) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
            }

            break;

        }
        case 12: {
            // ALG_SET HANDLER
            //
            if ( strcmp (pvalue1, "Actuator") == 0 ) {
                if ( strcmp (pvalue2, "Constant") == 0 ) {
                    if ( set_actuator( param[0], param[1]) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
                if ( strcmp (pvalue2, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[1] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for ccstate_N key
                    //      because get_sensor_data() write to this key 
                    reply = redisCommand(c, "GET ccstate_%d", param[1]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }

                    // error handler
                    //if (set_actuator(param[0], tmp) >= 0 ) ret_val = 1;
                    if (set_actuator(param[0], ival) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
                if ( strcmp (pvalue2, "Variable") == 0 ) {
                    // right value from -32768 to 32767
                    tmp = get_variable_from_redis( pvalue4 );
                    if ( tmp == -32769 ) {
                        //    error!
                        ret_val = -1; 
                        break;
                    }
                    if (set_actuator(param[0], tmp) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
            } else if ( strcmp (pvalue1, "Variable") == 0 ) {
                if ( strcmp (pvalue2, "Constant") == 0 ) {
                    if ( set_variable_in_redis( pvalue3, param[1]) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
                if ( strcmp (pvalue2, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[1] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for ccstate_N key
                    //      because get_sensor_data() write to this key 
                    reply = redisCommand(c, "GET ccstate_%d", param[1]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }

                    // error handler
                    //if (set_variable_in_redis(pvalue3, tmp) >= 0 ) ret_val = 1;
                    if (set_variable_in_redis(pvalue3, ival) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
                if ( strcmp (pvalue2, "Variable") == 0 ) {
                    // right value from -32768 to 32767
                    tmp = get_variable_from_redis (pvalue4);
                    if ( tmp == -32769 ) {
                        //    error!
                        ret_val = -1; 
                        break;
                    }
                    if ( set_variable_in_redis(pvalue3, tmp) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
            } else if ( strcmp (pvalue1, "Timer") == 0 ) {
                if ( strcmp (pvalue2, "Constant") == 0 ) {
                    if ( set_timer_in_redis( pvalue3, param[1]) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
                if ( strcmp (pvalue2, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[1] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for ccstate_N key
                    //      because get_sensor_data() write to this key 
                    reply = redisCommand(c, "GET ccstate_%d", param[1]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }

                    // error handler
                    if (set_timer_in_redis(pvalue3, ival) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
                if ( strcmp (pvalue2, "Variable") == 0 ) {
                    // right value from -32768 to 32767
                    tmp = get_variable_from_redis (pvalue4);
                    if ( tmp == -32769 ) {
                        //    error!
                        ret_val = -1; 
                        break;
                    }
                    if ( set_timer_in_redis(pvalue3, tmp) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
            } else if ( strcmp (pvalue1, "Statistic") == 0 ) {
                if ( strcmp (pvalue2, "Sensor") == 0 ) {
                    tmp = get_sensor_data( param[1] );

                    if (tmp == -1) {
                        //    error!
                        ret_val = -1;
                        break;
                    }
                    //      now we looking for ccstate_N key
                    //      because get_sensor_data() write to this key 
                    reply = redisCommand(c, "GET ccstate_%d", param[1]);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                    if ( reply->type != REDIS_REPLY_NIL ) {
                        ival = (int)strtod(reply->str, &err);
                        freeReplyObject(reply);

                    } else {
                        //    error!
                        ret_val = -1;
                        freeReplyObject(reply);
                        break;
                    }

                    // error handler
                    //if (set_actuator(param[0], tmp) >= 0 ) ret_val = 1;
                    if (set_statistic(param[1], ival) >= 0 ) ret_val = 1;
                    else ret_val = -1;
                }
            }
            break;
        }
        case 13: {
            // ALG_LOGIC HANDLER
            //
            condition = 0;

            if ( strcmp (pvalue2, "Variable") == 0 ) {
                // right value from -32768 to 32767
                tmp1 = get_variable_from_redis( pvalue3 );
                if ( tmp1 == -32769 ) {
                    ret_val = -1; 
                    break;
                }
            } else if ( strcmp (pvalue2, "Timer") == 0 ) {
                // right value from -32768 to 32767
                tmp1 = get_timer_from_redis( pvalue3 );
                if ( tmp1 == -32769 ) {
                    ret_val = -1; 
                    break;
                }
            } else if ( strcmp (pvalue2, "Sensor") == 0 ) {
                //tmp1 = get_sensor_data( param[0] );

                //if (tmp1 == -1) {
                if ( get_sensor_data( param[0] ) == -1 ) {
                    //    error!
                    ret_val = -1;
                    break;
                }
                //      now we looking for sstate_N key
                //
                reply = redisCommand(c, "GET ccstate_%d", param[0]);
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[0], reply->str);

                if ( reply->type != REDIS_REPLY_NIL ) {
                    ival = (int)strtod(reply->str, &err);
                    freeReplyObject(reply);

                } else {
                    //    error!
                    ret_val = -1;
                    freeReplyObject(reply);
                    break;
                }

                tmp1 = ival;
            }

            if ( strcmp ( pvalue4, "Constant" ) == 0 ) {
                tmp2 = param[1];
            } else if ( strcmp (pvalue2, "Sensor") == 0 ) {
                //tmp2 = get_sensor_data( param[1] );

                //if (tmp2 == -1) {
                if ( get_sensor_data( param[1] ) == -1 ) {
                    //    error!
                    ret_val = -1;
                    break;
                }
                //      now we looking for sstate_N key
                //
                reply = redisCommand(c, "GET ccstate_%d", param[1]);
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%ld is %s\n", THIS_FILE, param[1], reply->str);

                if ( reply->type != REDIS_REPLY_NIL ) {
                    ival = (int)strtod(reply->str, &err);
                    freeReplyObject(reply);

                } else {
                    //    error!
                    ret_val = -1;
                    freeReplyObject(reply);
                    break;
                }

                tmp2 = ival;
            }

            // create logical condition 1 (true) or 0 (false)
            if ( strcmp (pvalue1, ">") == 0 ) {
                // ">" handler
                if ( tmp1 > tmp2 ) condition = 1;
            } else if ( strcmp (pvalue1, ">=") == 0 ) {
                // ">=" handler
                if ( tmp1 >= tmp2 ) condition = 1;
            } else if ( strcmp (pvalue1, "<") == 0 ) {
                // "<" handler
                if ( tmp1 < tmp2 ) condition = 1;
            } else if ( strcmp (pvalue1, "<=") == 0 ) {
                // "<=" handler
                if ( tmp1 <= tmp2 ) condition = 1;
            } else if ( strcmp (pvalue1, "!=") == 0 ) {
                // "!=" handler
                if ( tmp1 != tmp2 ) condition = 1;
            } else if ( strcmp (pvalue1, "==") == 0 ) {
                // "==" handler
                if ( tmp1 == tmp2 ) condition = 1;
            }

            // condition handler
            if ( condition ) {

                // if CALL subroutine - last command nStr to command stack
                if ( strcmp (pvalue5, "Call") == 0 ) {
                    //scen_stack = scen_pc;
                    command_stack_push(scen_pc);
                    scen_pc = param[2] - 1;    // -1 because we must touch concrete action in query with "> nStr" 
                } else
                if ( strcmp (pvalue5, "Goto") == 0 ) {
                    scen_pc = param[2] - 1;    // -1 because we must touch concrete action in query with "> nStr" 
                } else
                if ( strcmp (pvalue5, "Log") == 0 ) {
                    // Log event handler
                    char * log_time;
                    log_time  = malloc(sizeof(char)*30);
                    get_time(log_time);
                    openlog(THIS_FILE, LOG_NDELAY, LOG_LOCAL1);
                    syslog(LOG_INFO, "%s%s log event", log_time, pvalue6);
                    closelog();
                    free(log_time);
                } 
            }
            // if we become here - return 1 (it's all right)
            ret_val = 1;

            break;
        }
        case 15: {
            // RGB_TO_Actuator HANDLER
            //
            int r, g, b;
            char *rgb_str;

            reply = redisCommand(c, "GET var_%s", pvalue1);
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sGET %s is %s\n", THIS_FILE, pvalue1, reply->str);

            if ( reply->type != REDIS_REPLY_NIL ) {
                rgb_str = malloc(sizeof(char) * reply->len);
                strcpy(rgb_str, reply->str);
                freeReplyObject(reply);

            } else {
                //    error!
                ret_val = -1;
                freeReplyObject(reply);
                break;
            }

            sscanf(rgb_str, "#%02x%02x%02x", &r, &g, &b);
            free(rgb_str);

            if ( r > 255) r = 255;
            if ( g > 255) g = 255;
            if ( b > 255) b = 255;
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf (stdout, "%sr=%d, g=%d, b=%d\n", THIS_FILE, r, g, b);

            if ( strcmp (pvalue2, "Actuator") == 0 ) {
                r = cast_rgb_value(r, param[0]);
                if ( set_actuator( param[0], r) >= 0 ) ;
                else { ret_val = -1; break; }
            }
            if ( strcmp (pvalue3, "Actuator") == 0 ) {
                g = cast_rgb_value(g, param[1]);
                if ( set_actuator( param[1], g) >= 0 ) ;
                else { ret_val = -1; break; }
            }
            if ( strcmp (pvalue4, "Actuator") == 0 ) {
                b = cast_rgb_value(b, param[2]);
                if ( set_actuator( param[2], b) >= 0 ) ret_val = 1;
                else { ret_val = -1;  break; }
            }
            break;
        }
        case 17: {
            // ALG_RETURN HANDLER
            //scen_pc = scen_stack;
            scen_pc = command_stack_pop();
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sreturning to nStr=%d of main prog\n", THIS_FILE, scen_pc);
            ret_val = 1;
            break;
        }
        case 1000: {
           // ALG_END HANDLER
           scen_end = 1;
           ret_val = 1;
           break;
        }
    }

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sparameters_handler() returned %d\n", THIS_FILE, ret_val); 
    return ret_val;

}


int actions_handler(int sid) {

    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    char *action_name;
    int act_id, oper_id;
    int ret_val;
    int action_once;

    //          global scenario variable init
    scen_pc = 0;
    scen_end = 0;
    //scen_stack = 0;
    //         command stack tuning now
    while (command_stack_pop() != -1);

    int actions_count = 0;

    while ( !scen_end ) {

        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%saction handler started\n", THIS_FILE);

        //      prepare query for Actions table
        //
        sql = "SELECT id, label, nStr, actOnce, operationId FROM Actions WHERE scenarioId=? AND nStr>? ORDER BY nStr";

        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

        if (rc == SQLITE_OK) {
            // it's parameterized query
            sqlite3_bind_int(res, 1, sid);
            sqlite3_bind_int(res, 2, scen_pc);
        } else {
            //  error handler
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
            return -1;
        }

        //      execute query for Actions table   
        //

        //while (sqlite3_step(res) == SQLITE_ROW) 
        //          handle only one action
        if (sqlite3_step(res) == SQLITE_ROW) {

            if (NEED_FULL_LOG) fprintf(stdout, "%s%s|", THIS_FILE, sqlite3_column_text(res, 0));

            //printf("%s|", sqlite3_column_text(res, 1));

            if (NEED_FULL_LOG) fprintf(stdout, "%d      <------- Actions.id and actOnce\n", sqlite3_column_int(res, 3));
            act_id = sqlite3_column_int(res, 0);
            oper_id = sqlite3_column_int(res, 4);

            if (NEED_FULL_LOG) fprintf(stdout, "%s---------------\n", THIS_FILE);

            action_once = sqlite3_column_int(res, 3);
            // story nStr last (ordinary) command
            scen_pc = sqlite3_column_int(res, 2);

            // create rc many time, but one time finalize(res)  -- very BAD (DB all time in use!!!) -- old place see DONE
            sqlite3_finalize(res);

            if ( action_once ) {
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%saction_once is %d, action will be deleted\n", THIS_FILE, action_once);
            }

            //  Parameters handler for this action
            // REMEMBER if (parameters_handler() && Action Once) => DELETE FROM Scenarios WHERE id = sid  
            // -1 is the error!!!
            if ( parameters_handler(act_id, oper_id/*, action_once*/) >= 0 ) {
                ret_val = 1;
                // if act once - delete scenario
                if ( action_once ) {
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%saction once handler will DEL scenario\n", THIS_FILE);
                    // scenario is ended
                    scen_end = 1;
                    // fill the array of delete records
                    del_scen_rec[del_array_no] = sid;
                    del_array_no ++;
                }
            } else {
                ret_val = -1;      // all are bad!!!
                goto DONE;
            }

        }
        actions_count ++;

        if ( actions_count > MAX_SCENARIO_ACTIONS ) {
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%smay be END operator is OMITED!!!\n", THIS_FILE);
            goto DONE;
        }
    }
DONE:
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sactions_handler() said - END of scenario!\n", THIS_FILE);
    //sqlite3_finalize(res);

    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sactions_handler() returned %d\n", THIS_FILE, ret_val); 
    return ret_val;

}

//          fill the array of delete records by 0
//
void fill_del_scen_rec_zeros() {
    int j;
    for (j = 0; j < MAX_DEL_RECORDS; j++) del_scen_rec[j] = 0;
}

//          the function return day of Week
//
void  get_week_day(char *ptr) {

    time_t rawtime = time(NULL);

    if (rawtime == -1) {
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sThe time() function failed\n", THIS_FILE);

        return;
    }

    struct tm *ptm = localtime(&rawtime);

    if (ptm == NULL) {
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sThe localtime() function failed\n", THIS_FILE);

        return;
    }

    sprintf(ptr, "%d", ptm->tm_wday);
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sWeek day: %s\n", THIS_FILE, ptr);

}

void exit_program(int sig) {
    char *log_time;

    //(void) signal(SIGINT, SIG_DFL);
    //fprintf(stdout,"\n%sRequested to cancel the thread: SIG %d (SIGINT)\n", THIS_FILE, sig);
    //          Disconnects and frees the redis context
    redisFree(c);
    //          Closing Dbase
    sqlite3_close(db);
    //sqlite3_close(db_web);

    log_time = malloc(sizeof(char)*30);
    get_time(log_time);
    openlog(THIS_FILE, LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_INFO, "%sRequested to cancel the thread: SIG %d (SIGINT)", log_time, sig);
    closelog();
    free(log_time);

    exit(0); 
}

int main(int argc, char **argv) {

    //sqlite3 *db;
    char *err_msg = 0;
    sqlite3_stmt *res;
    char *sql;
    //char *scen_name;
    int scen_id;
    int i, pCur, pHiwtr;
    char *log_msg;
    //          for action once execution
    int once_exec_action_needed;
    //          don't loose state UI controls
    int reread_uicontrols_list;
    //          user page alive status
    int ui_wdog_counter;
    //          scenario_id <--- if not 0, then new scenario or activate scenario handler started
    long activate_scenario_id;
    int activate_scenario_data;

    //          scenario_id <--- if not 0, then delete scenario handler started
    long delete_scenario_id;
    //           for measure time of main loop
    long /*start_main_cycle_time,*/ fin_main_cycle_time;

    //          redis tuning
    redisReply *reply, *reply1, *reply2;
    char *err;

    //          fill the array of delete scenario records by 0
    //
    fill_del_scen_rec_zeros();
    del_array_no = 0;

    //          previous states of controls in user Page
    //
    //int prev_uicontrol_state[MAX_UICONTROLS_IN_USERPAGE] = {-1};
    //          now prev_states in REDIS keys (see after REDIS initalization)

    //          command stack tuning
    ps.top = -1;

    //          CTRL-C handler
    //
    (void) signal(SIGINT, exit_program);

    //          redis initialization 
    //
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

        //      now del redis keys user_page_controls_set
        //

        reply = redisCommand(c,"DEL user_page_controls_set");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sDEL user_page_controls_set is %s\n", THIS_FILE, reply->str);

        freeReplyObject(reply);

    //          sqlite initialization 
    //          for scenarios dbase
    //rc = sqlite3_open("/home/si/work/jexecutor/test.db", &db);
    rc = sqlite3_open(JEXEC_DB_FILE, &db);
    fprintf(stdout, "%stry to open scenario db %s\n",THIS_FILE, JEXEC_DB_FILE);

    if (rc != SQLITE_OK) {

        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);

        return 1;
    } else    fprintf(stdout, "%sscenario db open successfully!\n", THIS_FILE);

    //          for web conf dbase
    //rc_web = sqlite3_open("/home/root/leo/nodejs/express-example/db.sqlite", &db_web);

    //if (rc_web != SQLITE_OK) {

    //    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_web));
    //    sqlite3_close(db_web);

    //    return 1;
    //} else    fprintf(stdout, "%sweb conf db open successfully!\n", THIS_FILE);

    //          now web_db is added thru ATTACHE
    //
    //          try to attach
    //          web dbase

    //          now it's time to synchronize SAP tables
    //
    //          delete all from SAP
    //
    sql = "DELETE from Scenarios;";
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK ) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        return 1;
    } else    fprintf(stdout, "%sScenarios table is cleared successfully!\n", THIS_FILE);

    sql = "DELETE from Actions;";
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK ) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        return 1;
    } else    fprintf(stdout, "%sActions table is cleared successfully!\n", THIS_FILE);

    sql = "DELETE from Parameters;";
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK ) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        return 1;
    } else    fprintf(stdout, "%sParameters table is cleared successfully!\n", THIS_FILE);

   
    //sql = "ATTACH '/home/si/work/leo_express_app/db.sqlite' AS db_web;";
    sql = "ATTACH '"WEBAP_DB_FILE"' AS db_web;";
    fprintf(stdout, "%s\n", sql);
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK ) {

        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        return 1;
    } else    fprintf(stdout, "%sweb db attached successfully!\n", THIS_FILE);
   


        sql = "SELECT id, name from db_web.Scenarios;";

        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        }

        //     duplicate every record of Scenarios table   
        //
        int dup_id;
        while (sqlite3_step(res) == SQLITE_ROW) {
            dup_id = sqlite3_column_int(res, 0);
            duplicate_scenario  (dup_id);
            duplicate_actions   (dup_id);
            duplicate_parameters(dup_id);
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sscenario id=%d duplicated!\n", THIS_FILE, sqlite3_column_int(res, 0)); 
            //fprintf(stdout, "scenario %d\n", sqlite3_column_int(res, 0));

        }

        print_time();
        fprintf(stdout, "%sSAP synchronization ended...\n", THIS_FILE);
        sqlite3_finalize(res);

        //exit_program(SIGTERM);

    //         now prepare GUI control states handler (after REDIS initialization)
    reset_prev_uicontrol_states();

    print_time();
    fprintf(stdout, "%s%s main loop executor STARTED!!!\n", THIS_FILE, VER);

    //log_time = malloc(sizeof(char)*30);
    //get_time(log_time);
    //openlog(THIS_FILE, LOG_NDELAY, LOG_LOCAL1);
    //syslog(LOG_INFO, "%s%sLEO main loop executor STARTED!!!", log_time, VER);
    //closelog();
    //free(log_time);

    log_msg = malloc(50*sizeof(char));
    sprintf(log_msg, "%s main loop executor STARTED!!!", VER);
    logger(log_msg);
    free(log_msg);

    //         set foreign keys on
    //         journal mode wal

    sqlite3_foreign_keys_on();
    //sqlite3_journal_mode_wal_on();
    //sqlite3_set_cache_size();

    //          now is time to start from zero
    //          sensor_state_is_need_to_check[all] = 1;
    int even_or_odd = 0;
    fill_sensor_state_changes_ones();
    if (NEED_FULL_LOG) print_time();
    if (NEED_FULL_LOG) fprintf(stdout, "%sAll sensor_state_is_need_to_check[] is set to 1!\n", THIS_FILE);

    //          now to all ccstate_N keys wee must write 'inactive'
    //
    //set_all_ccstates_inactive();

    //for (i = 0; i < 10; i++) {
    //
    for (; ; ) {
    //          take a start time of main loop
    start_main_cycle_time = ltime();

    //          prepare query for Scenarios table
    //
        sql = "SELECT id, name, priority, workWeekdays FROM Scenarios WHERE active=1 ORDER BY priority";

        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

        if (rc == SQLITE_OK) {
            //sqlite3_bind_int(res, 1, 3);
        } else {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        }

        //     execute query for Scenarios table   
        //
        puts("===============================================================");

        int rec_count = 0;

        while (sqlite3_step(res) == SQLITE_ROW) {

            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sscenario handler started!\n", THIS_FILE); 

            //strcpy(scen_name, sqlite3_column_text(res, 1));
            //printf("%ld\n", strlen(scen_name));

            printf("%s|", sqlite3_column_text(res, 0));
            scen_id = sqlite3_column_int(res, 0);
            //printf("%d\n", scen_id);

            printf("%s|", sqlite3_column_text(res, 1));
            printf("%s|", sqlite3_column_text(res, 2));
            printf("%s\n", sqlite3_column_text(res, 3));

            //TODO replace puts!!!
            puts("===============================================================");

            //  handle actions for this scenario if day of Week is worked day
            char *buf;
            buf = malloc(sizeof(char) * 2);
            // 8-th day of week is not exist
            strcpy(buf, "8");
            get_week_day(buf);
            if ( strstr(sqlite3_column_text(res, 3), buf) != NULL )  actions_handler(scen_id);

            puts("===============================================================");

            rec_count++;
        }

        print_time();
        fprintf(stdout, "%d records of Scenarios is handled...\n", rec_count);
        fprintf(stdout,"***************************************************************\n");
        fprintf(stdout,"               *********************************               \n");
        fprintf(stdout,"                            *******                            \n");
        sqlite3_finalize(res);

        // see http://www.sqlite.org/c3ref/db_status.html
        if (NEED_FULL_LOG) sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_USED, &pCur, &pHiwtr, 0);
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sSQLite cache used %d\n", THIS_FILE, pCur); 

        //if (NEED_FULL_LOG) sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_HIT, &pCur, &pHiwtr, 0);
        //if (NEED_FULL_LOG) print_time();
        //if (NEED_FULL_LOG) fprintf(stdout, "%sSQLite cache hits value %d\n", THIS_FILE, pCur); 

        //      deleting scenarios of once time actions
        for ( i = 0; i < del_array_no; i++) {
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sscenario with id %d will be deleted\n", THIS_FILE, del_scen_rec[i]); 
            delete_scenario (del_scen_rec[i]);
        }
        fill_del_scen_rec_zeros();
        del_array_no = 0;

        //      now controller_hard_reset signal handler
        int c_hard_reset;

        reply = redisCommand(c, "GET controller_hard_reset");

        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET controller_hard_reset is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            c_hard_reset = (int)strtod(reply->str, &err);

        } else {
            c_hard_reset = 0;
        }

        freeReplyObject(reply);

        if (c_hard_reset) {
            if ( set_default_ip_addr() && set_default_admin_passwd() && set_all_scenarios_not_active() ) {
                redisCommand(c, "SET controller_hard_reset 0");
                if (NEED_FULL_LOG) print_time();
                if (NEED_FULL_LOG) fprintf(stdout, "%sSET controller_hard_reset 0\n", THIS_FILE);
            }
        }

        //      now see redis keys for add new scenarios records
        //      with once execution option
        //

        reply = redisCommand(c,"GET once_exec_action_needed");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET once_exec_action_needed is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            once_exec_action_needed = (int)strtod(reply->str, &err);

        } else {
            once_exec_action_needed = 0;
        }

        freeReplyObject(reply);

        if (once_exec_action_needed) {
            db_act_once_add();
            //  omit UI events from user page and others to increase speed
            //  of once execute action
            goto EndExeLoop;
        }

        //      now see redis list user_page_control_in_check
        //      to track sensor in active user page
        //
        //      if not user on user page
        reply = redisCommand(c, "GET user_page_alive");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET user_page_alive is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            ui_wdog_counter = (int)strtod(reply->str, &err);

        } else {
            ui_wdog_counter = 0;
        }

        freeReplyObject(reply);

        //      this is varian of user leave page by closing browser
        //
        if (!ui_wdog_counter) goto UiWdogDone;

        reply = redisCommand(c, "GET reread_uicontrols_list");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET reread_uicontrols_list is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            reread_uicontrols_list = (int)strtod(reply->str, &err);

        } else {
            reread_uicontrols_list = 0;
        }

        freeReplyObject(reply);

        if (reread_uicontrols_list) {
            //  all states of UI controls must be clear
            //for(i = 0; i < MAX_UICONTROLS_IN_USERPAGE; i++) prev_uicontrol_state[i] = -1;
            reset_prev_uicontrol_states();
        }

        //  now we wait event from web interface
        redisCommand(c, "SET reread_uicontrols_list 0");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sSET reread_uicontrols_list 0\n", THIS_FILE);

        // get a REDIS list items with ccId data for UI
        //reply = redisCommand(c, "LRANGE user_page_controls_in_check 0 -1");
        //now it's the set
        reply = redisCommand(c, "SMEMBERS user_page_controls_set");

        if ( reply->type == REDIS_REPLY_ERROR ) {
            printf( "Error: %s\n", reply->str );
        } else if ( reply->type != REDIS_REPLY_ARRAY ) {
            printf( "Unexpected type: %d\n", reply->type );
        } else {
            int i/*, j = 0*/;
            // in redis SET or list storied all controls in user WEB-page who wait UI change event
            // loop all of it
            for (i = 0; i < reply->elements; i++ ) {
                printf( "ConnchannelId: %s\n", reply->element[i]->str );

                int connchann = (int)strtod(reply->element[i]->str, &err);
                // read data (in connchannelId present all info)
                //int sensor_state;
                //sensor_state = get_sensor_data(connchann);

                //if (sensor_state == -1) 
                if ( get_sensor_data(connchann) == -1 ) {
                    // error log
                    if (NEED_FULL_LOG) print_time();
                    fprintf(stdout,"%sget_sensor_state(%d) error\n", THIS_FILE, connchann);
                    //break;
                    // not break - state is inactive!!!
                    redisCommand(c,"PUBLISH %s %s:%s", "user_page_controls_state_is_changed", reply->element[i]->str, "inactive");
                }
                else {
                    //      now we looking for ccstate_N key
                    //
                    reply1 = redisCommand(c, "GET ccstate_%d", connchann);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%d is %s\n", THIS_FILE, connchann, reply1->str);

                    if ( reply1->type == REDIS_REPLY_NIL ) {
                        if (NEED_FULL_LOG) print_time();
                        fprintf(stdout,"%sGET ccstate_%d error\n", THIS_FILE, connchann);
                        //break;
                        freeReplyObject(reply1);
                        reply1 = redisCommand(c, "SET ccstate_%d %s", connchann, "unknown");
                        if ( reply1 == NULL || reply1->type == REDIS_REPLY_NIL || reply1->type == REDIS_REPLY_ERROR ) {
                            // TODO error log
                            if (NEED_FULL_LOG) print_time();
                            if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET ccstate_%d\n", THIS_FILE, connchann);
                        }
                        freeReplyObject(reply1);
                        //  try once again
                        reply1 = redisCommand(c, "GET ccstate_%d", connchann);
                        if (NEED_FULL_LOG) print_time();
                        if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccstate_%d is %s\n", THIS_FILE, connchann, reply1->str);
                        if ( reply1->type == REDIS_REPLY_NIL ) {
                            if (NEED_FULL_LOG) print_time();
                            fprintf(stdout,"%sGET ccstate_%d error\n", THIS_FILE, connchann);
                            //break;
                        }
                    }

                    //      and ccprevstate_N key
                    //
                    reply2 = redisCommand(c, "GET ccprevstate_%d", connchann);
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccprevstate_%d is %s\n", THIS_FILE, connchann, reply2->str);

                    if ( reply2->type == REDIS_REPLY_NIL ) {
                        if (NEED_FULL_LOG) print_time();
                        fprintf(stdout,"%sGET ccprevstate_%d error\n", THIS_FILE, connchann);
                        //break;
                        freeReplyObject(reply2);
                        reply2 = redisCommand(c, "SET ccprevstate_%d %s", connchann, "unknown");
                        if ( reply2 == NULL || reply2->type == REDIS_REPLY_NIL || reply2->type == REDIS_REPLY_ERROR ) {
                            // TODO error log
                            if (NEED_FULL_LOG) print_time();
                            if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET ccprevstate_%d\n", THIS_FILE, connchann);
                        }
                        freeReplyObject(reply2);
                        //  try once again
                        reply2 = redisCommand(c, "GET ccprevstate_%d", connchann);
                        if (NEED_FULL_LOG) print_time();
                        if (NEED_FULL_LOG) fprintf(stdout, "%sGET ccprevstate_%d is %s\n", THIS_FILE, connchann, reply2->str);
                        if ( reply2->type == REDIS_REPLY_NIL ) {
                            if (NEED_FULL_LOG) print_time();
                            fprintf(stdout,"%sGET ccprevstate_%d error\n", THIS_FILE, connchann);
                            //break;
                        }
                    }

                    //if (prev_uicontrol_state[connchann] != sensor_state) 
                    if ( strcmp( reply1->str, reply2->str ) != 0 ) {

                        if (NEED_FULL_LOG) print_time();
                        fprintf(stdout,"%scontrol %s state of user pages is changed to %s\n", THIS_FILE, reply->element[i]->str, reply1->str);
                        // connchannel_id:value is ready to JSON
                        // TODO may be redis reply handler needed!!
                        redisCommand(c,"PUBLISH %s %s:%s", "user_page_controls_state_is_changed", reply->element[i]->str, reply1->str);
                    }

                    freeReplyObject(reply2);

                    //prev_uicontrol_state[connchann] = sensor_state;
                    reply2 = redisCommand(c, "SET ccprevstate_%d %s", connchann, reply1->str);
                    if ( reply2 == NULL || reply2->type == REDIS_REPLY_NIL || reply2->type == REDIS_REPLY_ERROR ) {
                        // TODO error log
                        if (NEED_FULL_LOG) print_time();
                        if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET ccprevstate_%d\n", THIS_FILE, connchann);

                    }

                    if (reply1) freeReplyObject(reply1);
                    if (reply2) freeReplyObject(reply2);
                }
            }
        }

        freeReplyObject(reply);

        //      in main loop we must feed wdog
        //
        ui_wdog_counter--;

        reply = redisCommand(c, "SET user_page_alive %d", ui_wdog_counter);
        if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
            // TODO error log
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET user_page_alive!\n", THIS_FILE);

        } else {
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%stry to SET user_page_alive %d - %s!\n", THIS_FILE, ui_wdog_counter, reply->str);
        }
        freeReplyObject(reply);

UiWdogDone:
        //      now we looking for rdexe_activate_scenario key
        //
        reply = redisCommand(c, "GET rdexe_activate_scenario");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET rdexe_activate_scenario is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            activate_scenario_id = (long)strtod(reply->str, &err);

        } else {
            activate_scenario_id = 0;
        }

        freeReplyObject(reply);

        if (activate_scenario_id) {

            reply = redisCommand(c, "GET rdexe_activate_scen_data");
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sGET rdexe_activate_scen_data is %s\n", THIS_FILE, reply->str);

            if ( reply->type != REDIS_REPLY_NIL ) {
                activate_scenario_data = (int)strtod(reply->str, &err);

            } else {
                activate_scenario_data = 0;
            }

            freeReplyObject(reply);

            if ( scenario_exist_check( activate_scenario_id ) ) {
                // now (27.05.2019) first of all delete scenario - to fix bug #005
                if (delete_scenario(activate_scenario_id) ) {
                    if (duplicate_scenario(activate_scenario_id) ) {
                        if (duplicate_actions(activate_scenario_id)) {
                            if (duplicate_parameters(activate_scenario_id)) {
                                if ( activate_scenario( activate_scenario_id, activate_scenario_data ) ) {

                                    // replace key with no scenario id
                                    reply = redisCommand(c, "SET rdexe_activate_scenario %d", 0);
                                    if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
                                        // TODO error log
                                        if (NEED_FULL_LOG) print_time();
                                        if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET rdexe_activate_scenario!\n", THIS_FILE);

                                    } else {
                                        if (NEED_FULL_LOG) print_time();
                                        if (NEED_FULL_LOG) fprintf(stdout, "%sSET rdexe_activate_scenario redis key to 0 - %s!\n", THIS_FILE, reply->str);
                                    }
                                    freeReplyObject(reply);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                //  new scenario handler started
                if (duplicate_scenario(activate_scenario_id) ) {
                    if (duplicate_actions(activate_scenario_id)) {
                        if (duplicate_parameters(activate_scenario_id)) {
                            // replace key with no scenario id
                            reply = redisCommand(c, "SET rdexe_activate_scenario %d", 0);
                            if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
                                // TODO error log
                                if (NEED_FULL_LOG) print_time();
                                if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET rdexe_activate_scenario!\n", THIS_FILE);

                            } else {
                                if (NEED_FULL_LOG) print_time();
                                if (NEED_FULL_LOG) fprintf(stdout, "%sSET rdexe_activate_scenario redis key to 0 - %s!\n", THIS_FILE, reply->str);
                            }
                            freeReplyObject(reply);
                            //exit(1);
                            //return 1;
                        }
                    }
                }
            }
        }

        //      now we looking for rdexe_delete_scenario key
        //
        reply = redisCommand(c, "GET rdexe_delete_scenario");
        if (NEED_FULL_LOG) print_time();
        if (NEED_FULL_LOG) fprintf(stdout, "%sGET rdexe_delete_scenario is %s\n", THIS_FILE, reply->str);

        if ( reply->type != REDIS_REPLY_NIL ) {
            delete_scenario_id = (long)strtod(reply->str, &err);

        } else {
            delete_scenario_id = 0;
        }

        freeReplyObject(reply);

        if (delete_scenario_id) {
            if (delete_scenario(delete_scenario_id) ) {
                // replace key with no scenario id
                reply = redisCommand(c, "SET rdexe_delete_scenario %d", 0);
                if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
                    // TODO error log
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET rdexe_delete_scenario!\n", THIS_FILE);

                } else {
                    if (NEED_FULL_LOG) print_time();
                    if (NEED_FULL_LOG) fprintf(stdout, "%sSET rdexe_delete_scenario redis key to 0 - %s!\n", THIS_FILE, reply->str);
                }
                freeReplyObject(reply);
            }
        }

        //      TODO it's place to add telemetry
        //      sensors & actuators value to DB

        //          take a fin time of main loop
        fin_main_cycle_time = ltime();

        // time of main loop
        main_cycle_time = fin_main_cycle_time - start_main_cycle_time;
        if ( main_cycle_time == 0 ) main_cycle_time = 1;

        reply = redisCommand(c, "SET rdexe_main_cycle_time %d", main_cycle_time);
        if ( reply == NULL || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR ) {
            // TODO error log
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sredis error in SET rdexe_main_cycle_time!\n", THIS_FILE);

        }
        freeReplyObject(reply);

        //      it's time to sleep main loop now
EndExeLoop:
        //      usleep() - DO NOT TOUCH!!!
        //      this is excellent parameter
        usleep(MAIN_LOOP_DELAY);
        if (NEED_FULL_LOG) printf("Executor waiting...\n");

        //usleep(500000);
        //      place to say - "need poll sensors"
        //      now it's once in 2 main loops
        //
        if ( even_or_odd ) {
            fill_sensor_state_changes_ones();
            if (NEED_FULL_LOG) print_time();
            if (NEED_FULL_LOG) fprintf(stdout, "%sAll sensor_state_is_need_to_check[] is set to 1!\n", THIS_FILE);
        }

        if ( even_or_odd ) even_or_odd = 0;
        else even_or_odd = 1;
    }

    //           Closing Dbases
    sqlite3_close(db);
    //sqlite3_close(db_web);
    //           Disconnects and frees redis context
    redisFree(c); 

    return 0;
}
