[GDB查看C++对象布局_tmhanks的博客-CSDN博客](https://blog.csdn.net/tmhanks/article/details/89110833?utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7Edefault-1.no_search_link&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7Edefault-1.no_search_link)

```
GDB查看对象如下：
(gdb) set print object on
(gdb) set print vtbl on
(gdb) set print pretty on
((gdb) p base
$7 = (Base) {
_vptr.Base = 0x403a50 <vtable for Base+16>,
m_base = 0,
static m_tmp = 1
}
(gdb) info vtbl base
vtable for ‘Base’ @ 0x403a50 (subobject @ 0x7fffffffdf60):
[0]: 0x402830 Base::~Base()
[1]: 0x402930 Base::~Base()
[2]: 0x402ae0 <Base::print() const>
————————————————
版权声明：本文为CSDN博主「流水之川」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
原文链接：https://blog.csdn.net/tmhanks/article/details/89110833
```
