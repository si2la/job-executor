#include <sqlite3.h>
#include <stdio.h>

// for compile: :!gcc % -o parameterized -lsqlite3
// for use: :!./parameterized
// information from this sites: https://www.linuxjournal.com/content/accessing-sqlite-c - here update example!!!
// http://zetcode.com/db/sqlitec/

int main(void) {
    
    sqlite3 *db;
    char *err_msg = 0;
    sqlite3_stmt *res;
    
    int rc = sqlite3_open("test.db", &db);
    
    if (rc != SQLITE_OK) {
        
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        
        return 1;
    }
    
    char *sql = "SELECT Id, Name FROM Cars WHERE Id > ?";
        
    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    
    if (rc == SQLITE_OK) {
        // first parameter (?) in *sql (we start from the left hand side) replace with 3        
        sqlite3_bind_int(res, 1, 3);
    } else {
        
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

//    below one row handler
    
//    int step = sqlite3_step(res);
    
//    if (step == SQLITE_ROW) {
        
//        printf("%s: ", sqlite3_column_text(res, 0));
//        printf("%s\n", sqlite3_column_text(res, 1));
        
//    } 

    puts("==========================");
    int rec_count = 0;
    while (sqlite3_step(res) == SQLITE_ROW) {
            printf("%s|", sqlite3_column_text(res, 0));
            printf("%s\n", sqlite3_column_text(res, 1));

            rec_count++;
    }

    puts("==========================");
    printf("%d records is handled.\n", rec_count);

    sqlite3_finalize(res);
    sqlite3_close(db);
    
    return 0;
}
