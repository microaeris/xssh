#ifndef _XSSH_H
#define _XSSH_H

//#include "uthash.h"
#define MAX_VAR_SIZE 256

struct variableHashStruct {
    char id[MAX_VAR_SIZE];          /* key */
    char value[MAX_VAR_SIZE];
    //UT_hash_handle hh;              /* makes this structure hashable */
};

#endif /* _XSSH_H */
