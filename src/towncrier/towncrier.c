#include <stdio.h>

#include "sqlite/sqlite3.h"

// [ ] Include sqlite, if a db doesn't exist, create it
// [ ] When time is right (sunday, midnight), fork and run all-repos
// [ ] Once all-repos runs, update db with last all-repos run time
// [ ] DB also tracks when each backup was made
// [ ] When it recieves a "hello" ping fro Peasant, return a message with all the current status values

int main() {
    printf("Hello from the server\n");
}
