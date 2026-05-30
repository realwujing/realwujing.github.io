# si_mem_available内存可用性追踪分析

```bash
stap -ve '
probe kernel.function("si_mem_available") {
    printf("\n==> si_mem_available() called (pid=%d)\n", pid())
}

' -DSTP_NO_BUILDID_CHECK
```

```bash
stap -l 'kernel.function("si_mem_available@mm/page_alloc.c:*")'

kernel.function("si_mem_available@mm/page_alloc.c:4846")
```

```bash
stap -L 'kernel.function("si_mem_available@mm/page_alloc.c:*")'

kernel.function("si_mem_available@mm/page_alloc.c:4846") $pages:long unsigned int[]
```

```bash
[root@ctyunos si_mem_available]# stap -l 'kernel.statement("si_mem_available@mm/page_alloc.c:*")'

kernel.statement("si_mem_available@mm/page_alloc.c:4847")
kernel.statement("si_mem_available@mm/page_alloc.c:4850")
kernel.statement("si_mem_available@mm/page_alloc.c:4855")
kernel.statement("si_mem_available@mm/page_alloc.c:4856")
kernel.statement("si_mem_available@mm/page_alloc.c:4858")
kernel.statement("si_mem_available@mm/page_alloc.c:4859")
kernel.statement("si_mem_available@mm/page_alloc.c:4872")
kernel.statement("si_mem_available@mm/page_alloc.c:4873")
kernel.statement("si_mem_available@mm/page_alloc.c:4881")
kernel.statement("si_mem_available@mm/page_alloc.c:4888")
kernel.statement("si_mem_available@mm/page_alloc.c:4893")
kernel.statement("si_mem_available@mm/page_alloc.c:4894")
```

```bash
stap -L 'kernel.statement("si_mem_available@mm/page_alloc.c:*")'

kernel.statement("si_mem_available@mm/page_alloc.c:4847") $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4850") $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4855") $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4856") $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4858") $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4859") $wmark_low:long unsigned int $pages:long unsigned int[] $zone:struct zone*
kernel.statement("si_mem_available@mm/page_alloc.c:4872") $wmark_low:long unsigned int $pages:long unsigned int[] $zone:struct zone*
kernel.statement("si_mem_available@mm/page_alloc.c:4873") $pagecache:long unsigned int $wmark_low:long unsigned int $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4881") $pagecache:long unsigned int $wmark_low:long unsigned int $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4888") $pagecache:long unsigned int $wmark_low:long unsigned int $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4893") $pagecache:long unsigned int $pages:long unsigned int[]
kernel.statement("si_mem_available@mm/page_alloc.c:4894") $pagecache:long unsigned int $pages:long unsigned int[]
```

```bash
stap -ve '
probe kernel.statement("si_mem_available@mm/page_alloc.c:*") {
    printf("%s\n", pp())
    printf("通用寄存器:\n")
    print_regs()
}
' -DSTP_NO_BUILDID_CHECK
```

```bash
bpftrace -e 'kretprobe:si_mem_available {
    printf("si_mem_available() 返回值: %ld pages (约 %ld MB)\n", 
           retval, 
           retval >> 8);  // 页数转MB: 1页=4KB, 256页=1MB
}'
```
