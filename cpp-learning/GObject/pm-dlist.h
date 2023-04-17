/* file name: pm-dlist.h*/
 
#ifndef PM_DLIST_H
#define PM_DLIST_H
 
typedef struct _PMDListNode PMDListNode;
struct  _PMDListNode {
        PMDListNode *prev;
        PMDListNode *next;
        void *data;
};
 
typedef struct _PMDList PMDList;
struct  _PMDList {
        PMDListNode *head;
        PMDListNode *tail;
};
 
#endif