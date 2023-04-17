/* file name: double-list.h */
 
#ifndef DOUBLE_LIST_H
#define DOUBLE_LIST_H
 
struct double_list_node {
        struct doule_list_node *prev;
        struct double_list_node *next;
        void *data;
};
 
struct double_list {
        struct double_list_node *head;
        struct double_list_node *tail;
};
 
#endif