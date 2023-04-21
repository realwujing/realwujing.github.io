#include "pm-dlist3.h"
 
G_DEFINE_TYPE (PMDList, pm_dlist, G_TYPE_OBJECT);
 
static
void pm_dlist_init (PMDList *self)
{
        g_printf ("\t实例结构体初始化！\n");
 
        self->head = NULL;
        self->tail = NULL;
}
 
static
void pm_dlist_class_init (PMDListClass *klass)
{
        g_printf ("类结构体初始化!\n");
}