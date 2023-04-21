#ifndef PM_DLIST_H
#define PM_DLIST_H
 
#include <glib-object.h>
 
#define PM_TYPE_DLIST (pm_dlist_get_type ())
 
typedef struct _PMDListNode PMDListNode;
struct  _PMDListNode {
        PMDListNode *prev;
        PMDListNode *next;
        void *data;
};
 
typedef struct _PMDList PMDList;
struct  _PMDList {
        GObject parent_instance;
        PMDListNode *head;
        PMDListNode *tail;
};
 
typedef struct _PMDListClass PMDListClass;
struct _PMDListClass {
        GObjectClass parent_class;
};
 
GType pm_dlist_get_type (void);
 
#endif