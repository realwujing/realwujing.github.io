#include "pm-dlist3.h"
 
int
main (void)
{
        /* GObject 库的类型管理系统的初始化 */
        g_type_init ();
 
        int i;
        PMDList *list;
 
        /* 进行三次对象实例化 */
        for (i = 0; i < 3; i++){
                list = g_object_new (PM_TYPE_DLIST, NULL);
                g_object_unref (list);
        }
 
        /* 检查实例是否为 GObject 对象 */
        list = g_object_new (PM_TYPE_DLIST, NULL);
        if (G_IS_OBJECT (list))
                g_printf ("\t这个实例是一个 GObject 对象！\n");
         
        return 0;
}