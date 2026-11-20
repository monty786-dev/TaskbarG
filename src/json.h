/* Minimal JSON tree (parse + dump) used for WinShell config.json.
 * Output matches Python's json.dump(indent=2, ensure_ascii=True). */
#ifndef WS_JSON_H
#define WS_JSON_H
#include <stddef.h>

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;

typedef struct JNode {
    JType t;
    char *key;              /* key inside an object (UTF-8) */
    char *s;                /* string value (UTF-8) */
    double n;               /* number value */
    int b;                  /* bool value */
    int is_int;             /* number was written without '.'/exponent */
    struct JNode *child;    /* first child (array / object) */
    struct JNode *next;     /* next sibling */
} JNode;

JNode *json_new(JType t);
JNode *json_str(const char *s);
JNode *json_num(double n, int is_int);
JNode *json_bool(int b);
JNode *json_parse(const char *text);
void   json_free(JNode *n);
JNode *json_clone(const JNode *n);
JNode *json_get(const JNode *obj, const char *key);
/* Add (or replace in place) a member in an object. Takes ownership of child. */
JNode *json_put(JNode *obj, const char *key, JNode *child);
/* Append to an array. Takes ownership of child. */
JNode *json_push(JNode *arr, JNode *child);
void   json_remove(JNode *obj, const char *key);
int    json_count(const JNode *n);
char  *json_dump(const JNode *n);   /* malloc'd, free() it */

#endif
