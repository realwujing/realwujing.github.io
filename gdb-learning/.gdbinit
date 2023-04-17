# 保存历史命令
set history save on

# 保存已经设置的断点
save breakpoints file-name-to-save

# 记录执行gdb的过程
set logging on

# 覆盖之前的日志文件
# set logging overwrite on

# 让gdb的日志不会打印在终端
# set logging redirect on

# 进入不带调试信息的函数
# set step-mode on

# 调试子进程
# set follow-fork-mode child

# 同时调试父进程和子进程
# set detach-on-fork off

# 让父子进程都同时运行
# set schedule-multiple on

# 只允许一个线程运行
# set scheduler-locking on

# 调试个别的线程，并且不想在调试过程中，影响其它线程的运行
# set non-stop mode on

#每行打印一个结构体成员
set print pretty on

# 按照派生类型打印对象
set print object on

# 用比较规整的格式来显示虚函数表
set print vtbl on

# 设置显示结构体时，是否显式其内的联合体数据
set print union on

# 每个数组元素占一行
set print array on

# 打印数组的索引下标 
set print array-indexes on

# 设置字符显示，是否按“/nnn”的格式显示，如果打开，则字符串或字符数据按/nnn显示，如“/065”
# set print sevenbit-strings on

# 设置反汇编格式为 intel汇编
# set disassembly-flavor intel

# https://github.com/Lekensteyn/qt5printers
python
import sys, os.path
sys.path.insert(0, os.path.expanduser('~/.gdb'))
import qt5printers
qt5printers.register_printers(gdb.current_objfile())
end
