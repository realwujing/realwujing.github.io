/* file name: dlist.h（版本 2）*/
 
#ifndef DLIST_H
#define DLIST_H
 
typedef struct _DListNode DListNode;
struct  _DListNode {
        DListNode *prev;
        DListNode *next;
        void *data;
};
 
typedef struct _DList DList;
struct  _DList {
        DListNode *head;
        DListNode *tail;
};
 
#endif