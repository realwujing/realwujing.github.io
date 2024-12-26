---
date: 2024/08/02 15:40:15
updated: 2024/08/02 15:40:15
---

```bash
stress-ng, version 0.18.01 (gcc 7.3.0, x86_64 Linux 4.19.90-2102.2.0.0066.ctl2.x86_64)

Usage: stress-ng [OPTION [ARG]]

General control options:
      --abort                 abort all stressors if any stressor fails
      --aggressive            enable all aggressive options
-a N, --all N                 start N workers of each stress test
-b N, --backoff N             wait of N microseconds before work starts
      --change-cpu            force child processes to use different CPU to
                              that of parent
      --class name            specify a class of stressors, use with
                              --sequential
-n,   --dry-run               do not run
      --ftrace                enable kernel function call tracing
-h,   --help                  show help
      --ignite-cpu            alter kernel controls to make CPU run hot
      --interrupts            check for error interrupts
      --ionice-class C        specify ionice class (idle, besteffort, realtime)
      --ionice-level L        specify ionice level (0 max, 7 min)
      --iostate S             show I/O statistics every S seconds
-j,   --job jobfile           run the named jobfile
      --keep-files            do not remove files or directories
-k,   --keep-name             keep stress worker names to be 'stress-ng'
      --klog-check            check kernel message log for errors
      --ksm                   enable kernel samepage merging
      --log-brief             less verbose log messages
      --log-file filename     log messages to a log file
      --log-lockless          log messages without message locking
      --maximize              enable maximum stress options
      --max-fd N              set maximum file descriptor limit
      --mbind                 set NUMA memory binding to specific nodes
-M,   --metrics               print pseudo metrics of activity
      --metrics-brief         enable metrics and only show non-zero results
      --minimize              enable minimal stress options
      --no-madvise            don't use random madvise options for each mmap
      --no-oom-adjust         disable all forms of out-of-memory score
                              adjustments
      --no-rand-seed          seed random numbers with the same constant
      --oom-avoid             Try to avoid stressors from being OOM'd
      --oom-avoid-bytes N     Number of bytes free to stop further memory
                              allocations
      --oomable               Do not respawn a stressor if it gets OOM'd
      --page-in               touch allocated pages that are not in core
      --parallel N            synonym for 'all N'
      --pathological          enable stressors that are known to hang a machine
      --perf                  display perf statistics
      --permute N             run permutations of stressors with N stressors
                              per permutation
-q,   --quiet                 quiet output
-r,   --random N              start N random workers
      --rapl                  report RAPL power domain measurements over entire
                              run (Linux x86 only)
      --raplstat S            show RAPL power domain stats every S seconds
                              (Linux x86 only)
      --sched type            set scheduler type
      --sched-prio N          set scheduler priority level N
      --sched-period N        set period for SCHED_DEADLINE to N nanosecs
                              (Linux only)
      --sched-runtime N       set runtime for SCHED_DEADLINE to N nanosecs
                              (Linux only)
      --sched-deadline N      set deadline for SCHED_DEADLINE to N nanosecs
                              (Linux only)
      --sched-reclaim         set reclaim cpu bandwidth for deadline scheduler
                              (Linux only)
      --seed N                set the random number generator seed with a 64
                              bit value
      --sequential N          run all stressors one by one, invoking N of them
      --skip-silent           silently skip unimplemented stressors
      --smart                 show changes in S.M.A.R.T. data
      --sn                    use scientific notation for metrics
      --status S              show stress-ng progress status every S seconds
      --stderr                all output to stderr
      --stdout                all output to stdout (now the default)
      --stressors             show available stress tests
      --syslog                log messages to the syslog
      --taskset               use specific CPUs (set CPU affinity)
      --temp-path path        specify path for temporary directories and files
      --thermalstat S         show CPU and thermal load stats every S seconds
      --thrash                force all pages in causing swap thrashing
-t N, --timeout T             timeout after T seconds
      --timer-slack N         set slack slack to N nanoseconds, 0 for default
      --times                 show run time summary at end of the run
      --timestamp             timestamp log output
      --tz                    collect temperatures from thermal zones (Linux
                              only)
-v,   --verbose               verbose output
      --verify                verify results (not available on all tests)
      --verifiable            show stressors that enable verification via
                              --verify
-V,   --version               show version
      --vmstat S              show memory and process statistics every S
                              seconds
-x,   --exclude list          list of stressors to exclude (not run)
      --with list             list of stressors to invoke (use with --seq or
                              --all)
-Y,   --yaml file             output results to YAML formatted file

Stressor specific options:
      --access N              start N workers that stress file access
                              permissions
      --access-ops N          stop after N file access bogo operations
      --acl N                 start N workers exercising valid ACL file mode
                              bits
      --acl-rand              randomize ordering of ACL file mode tests
      --acl-ops N             stop acl workers after N bogo operations
      --af-alg N              start N workers that stress AF_ALG socket domain
      --af-alg-dump           dump internal list from /proc/crypto to stdout
      --af-alg-ops N          stop after N af-alg bogo operations
      --affinity N            start N workers that rapidly change CPU affinity
      --affinity-delay D      delay in nanoseconds between affinity changes
      --affinity-ops N        stop after N affinity bogo operations
      --affinity-pin          keep per stressor threads pinned to same CPU
      --affinity-rand         change affinity randomly rather than sequentially
      --affinity-sleep N      sleep in nanoseconds between affinity changes
      --aio N                 start N workers that issue async I/O requests
      --aio-ops N             stop after N bogo async I/O requests
      --aio-requests N        number of async I/O requests per worker
      --aiol N                start N workers that exercise Linux async I/O
      --aiol-ops N            stop after N bogo Linux aio async I/O requests
      --aiol-requests N       number of Linux aio async I/O requests per worker
      --alarm N               start N workers exercising alarm timers
      --alarm-ops N           stop after N alarm bogo operations
      --apparmor              start N workers exercising AppArmor interfaces
      --apparmor-ops N        stop after N bogo AppArmor worker bogo operations
      --atomic                start N workers exercising GCC atomic operations
      --atomic-ops            stop after N bogo atomic bogo operations
      --bad-altstack N        start N workers exercising bad signal stacks
      --bad-altstack-ops N    stop after N bogo signal stack SIGSEGVs
      --bad-ioctl N           start N stressors that perform illegal read
                              ioctls on devices
      --bad-ioctl-ops  N      stop after N bad ioctl bogo operations
      --bad-ioctl-method M    method of selecting ioctl command [ random | inc
                              | random-inc | stride ]
      --besselmath N          start N workers exercising bessel math functions
      --besselmath-ops N      stop after N besselmath bogo bessel math
                              operations
      --besselmath-method M   select bessel math function to exercise
-B N, --bigheap N             start N workers that grow the heap using
                              realloc()
      --bigheap-bytes N       grow heap up to N bytes in total
      --bigheap-growth N      grow heap by N bytes per iteration
      --bigheap-mlock         attempt to mlock newly mapped pages
      --bigheap-ops N         stop after N bogo bigheap operations
      --binderfs N            start N workers exercising binderfs
      --binderfs-ops N        stop after N bogo binderfs operations
      --bind-mount N          start N workers exercising bind mounts
      --bind-mount-ops N      stop after N bogo bind mount operations
      --bitonicsort N         start N workers bitonic sorting 32 bit random
                              integers
      --bitonicsort-ops N     stop after N bitonic sort bogo operations
      --bitonicsort-size N    number of 32 bit integers to sort
-c N, --bitops N              start N workers that perform CPU only loading
      --bitops-method M       specify stress bitops method M, default is all
      --bitops-ops N          stop after N bitops bogo operations
      --branch N              start N workers that force branch misprediction
      --branch-ops N          stop after N branch misprediction branches
      --brk N                 start N workers performing rapid brk calls
      --brk-bytes N           grow brk region up to N bytes in total
      --brk-mlock             attempt to mlock newly mapped brk pages
      --brk-notouch           don't touch (page in) new data segment page
      --brk-ops N             stop after N brk bogo operations
      --bsearch N             start N workers that exercise a binary search
      --bsearch-method M      select bsearch method [ bsearch-libc |
                              bsearch-nonlibc ]
      --bsearch-ops N         stop after N binary search bogo operations
      --bsearch-size N        number of 32 bit integers to bsearch
-C N, --cache N               start N CPU cache thrashing workers
      --cache-size N          override the default cache size setting to N
                              bytes
      --cache-cldemote        cache line demote (x86 only)
      --cache-clflushopt      optimized cache line flush (x86 only)
      --cache-enable-all      enable all cache options
                              (fence,flush,sfence,etc..)
      --cache-fence           serialize stores
      --cache-flush           flush cache after every memory write (x86 only)
      --cache-level N         only exercise specified cache
      --cache-no-affinity     do not change CPU affinity
      --cache-ops N           stop after N cache bogo operations
      --cache-prefetch        prefetch on memory reads/writes
      --cache-sfence          serialize stores with sfence
      --cache-ways N          only fill specified number of cache ways
      --cache-clwb            cache line writeback (x86 only)
      --cacheline N           start N workers that exercise cachelines
      --cacheline-affinity    modify CPU affinity
      --cacheline-method M    use cacheline stressing method M
      --cacheline-ops N       stop after N cacheline bogo operations
      --cap N                 start N workers exercising capget
      --cap-ops N             stop cap workers after N bogo capget operations
      --cgroup N              start N workers exercising cgroup
                              mount/read/write/umounts
      --cgroup-ops N          stop after N iterations of cgroup actions
      --chattr N              start N workers thrashing chattr file mode bits
      --chattr-ops N          stop chattr workers after N bogo operations
      --chdir N               start N workers thrashing chdir on many paths
      --chdir-dirs N          select number of directories to exercise chdir on
      --chdir-ops N           stop chdir workers after N bogo chdir operations
      --chmod N               start N workers thrashing chmod file mode bits
      --chmod-ops N           stop chmod workers after N bogo operations
      --chown N               start N workers thrashing chown file ownership
      --chown-ops N           stop chown workers after N bogo operations
      --chroot N              start N workers thrashing chroot
      --chroot-ops N          stop chroot workers after N bogo operations
      --clock N               start N workers thrashing clocks and POSIX timers
      --clock-ops N           stop clock workers after N bogo operations
      --clone N               start N workers that rapidly create and reap
                              clones
      --clone-max N           set upper limit of N clones per worker
      --clone-ops N           stop after N bogo clone operations
      --close N               start N workers that exercise races on close
      --close-ops N           stop after N bogo close operations
      --context N             start N workers exercising user context
      --context-ops N         stop context workers after N bogo operations
      --copy-file N           start N workers that copy file data
      --copy-file-bytes N     specify size of file to be copied
      --copy-file-ops N       stop after N copy bogo operations
-c N, --cpu N                 start N workers that perform CPU only loading
-l P, --cpu-load P            load CPU by P %, 0=sleep, 100=full load (see -c)
      --cpu-load-slice S      specify time slice during busy load
      --cpu-method M          specify stress cpu method M, default is all
      --cpu-old-metrics       use old CPU metrics instead of normalized metrics
      --cpu-ops N             stop after N cpu bogo operations
      --cpu-online N          start N workers offlining/onlining the CPUs
      --cpu-online-affinity   set CPU affinity to the CPU to be offlined
      --cpu-online-all        attempt to exercise all CPUs include CPU 0
      --cpu-online-ops N      stop after N offline/online operations
      --crypt N               start N workers performing password encryption
      --crypt-method M        select encryption method [ all | MD5 | NT | SHA-1
                              | SHA-256 | SHA-512 | scrypt | SunMD5 | yescrypt]
      --crypt-ops N           stop after N bogo crypt operations
      --cyclic N              start N cyclic real time benchmark stressors
      --cyclic-dist N         calculate distribution of interval N nanosecs
      --cyclic-method M       specify cyclic method M, default is clock_ns
      --cyclic-ops N          stop after N cyclic timing cycles
      --cyclic-policy P       used rr or fifo scheduling policy
      --cyclic-prio N         real time scheduling priority 1..100
      --cyclic-samples N      number of latency samples to take
      --cyclic-sleep N        sleep time of real time timer in nanosecs
      --daemon N              start N workers creating multiple daemons
      --daemon-ops N          stop when N daemons have been created
      --daemon-wait           stressor wait for daemon to exit and not init
      --dccp N                start N workers exercising network DCCP I/O
      --dccp-domain D         specify DCCP domain, default is ipv4
      --dccp-if I             use network interface I, e.g. lo, eth0, etc.
      --dccp-ops N            stop after N DCCP  bogo operations
      --dccp-opts option      DCCP data send options [send|sendmsg|sendmmsg]
      --dccp-port P           use DCCP ports P to P + number of workers - 1
      --dccp-msgs N           number of DCCP messages to send per connection
      --dekker N              start N workers that exercise the Dekker
                              algorithm
      --dekker-ops N          stop after N dekker mutex bogo operations
-D N, --dentry N              start N dentry thrashing stressors
      --dentry-ops N          stop after N dentry bogo operations
      --dentry-order O        specify unlink order (reverse, forward, stride)
      --dentries N            create N dentries per iteration
      --dev N                 start N device entry thrashing stressors
      --dev-file name         specify the /dev/ file to exercise
      --dev-ops N             stop after N device thrashing bogo ops
      --dev-shm N             start N /dev/shm file and mmap stressors
      --dev-shm-ops N         stop after N /dev/shm bogo ops
      --dir N                 start N directory thrashing stressors
      --dir-dirs N            select number of directories to exercise dir on
      --dir-ops N             stop after N directory bogo operations
      --dirdeep N             start N directory depth stressors
      --dirdeep-bytes N       size of files to create per level (see
                              --dirdeep-files)
      --dirdeep-dirs N        create N directories per level
      --dirdeep-files N       create N files per level (see --dirdeep-bytes)
      --dirdeep-inodes N      create a maximum N inodes (N can also be %)
      --dirdeep-ops N         stop after N directory depth bogo operations
      --dirmany N             start N directory file populating stressors
      --dirmany-bytes         specify size of files (default 0)
      --dirmany-ops N         stop after N directory file bogo operations
      --dnotify N             start N workers exercising dnotify events
      --dnotify-ops N         stop dnotify workers after N bogo operations
      --dup N                 start N workers exercising dup/close
      --dup-ops N             stop after N dup/close bogo operations
      --dynlib N              start N workers exercising dlopen/dlclose
      --dynlib-ops N          stop after N dlopen/dlclose bogo operations
      --efivar N              start N workers that read EFI variables
      --efivar-ops N          stop after N EFI variable bogo read operations
      --eigen N               start N workers exercising eigen operations
      --eigen-method M        specify eigen stress method M, default is all
      --eigen-ops N           stop after N maxtrix bogo operations
      --eigen-size N          specify the size of the N x N eigen
      --enosys N              start N workers that call non-existent system
                              calls
      --enosys-ops N          stop after N enosys bogo operations
      --env N                 start N workers setting environment vars
      --env-ops N             stop after N env bogo operations
      --epoll N               start N workers doing epoll handled socket
                              activity
      --epoll-domain D        specify socket domain, default is unix
      --epoll-ops N           stop after N epoll bogo operations
      --epoll-port P          use socket ports P upwards
      --epoll-sockets N       specify maximum number of open sockets
      --eventfd N             start N workers stressing eventfd read/writes
      --eventfs-nonblock      poll with non-blocking I/O on eventfd fd
      --eventfd-ops N         stop eventfd workers after N bogo operations
      --exec N                start N workers spinning on fork() and exec()
      --exec-fork-method M    select exec fork method: clone fork spawn vfork
      --exec-max P            create P workers per iteration, default is 4096
      --exec-method M         select exec method: all, execve, execveat
      --exec-no-pthread       do not use pthread_create
      --exec-ops N            stop after N exec bogo operations
      --exit-group N          start N workers that exercise exit_group
      --exit-group-ops N      stop exit_group workers after N bogo exit_group
                              loops
      --expmath N             start N workers exercising exponential math
                              functions
      --expmath-ops N         stop after N expmath bogo exponential math
                              operations
      --expmath-method M      select exponential math function to exercise
      --factor N              start N workers performing large integer
                              factorization
      --factor-digits N       specific number of digits of number to factor
      --factor-ops N          stop after N factorisation operations
      --fallocate N           start N workers fallocating 16MB files
      --fallocate-bytes N     specify size of file to allocate
      --fallocate-ops N       stop after N fallocate bogo operations
      --fanotify N            start N workers exercising fanotify events
      --fanotify-ops N        stop fanotify workers after N bogo operations
      --far-branch N          start N far branching workers
      --far-branch-ops N      stop after N far branching bogo operations
      --far-branch-pages N    number of pages to populate with functions
      --fault N               start N workers producing page faults
      --fault-ops N           stop after N page fault bogo operations
      --fcntl N               start N workers exercising fcntl commands
      --fcntl-ops N           stop after N fcntl bogo operations
      --fd-fork N             start N workers exercising dup/fork/close
      --fd-fork-file file     select file to dup [ null, random, stdin, stdout,
                              zero ]
      --fd-fork-fds N         set maximum number of file descriptors to use
      --fd-fork-ops N         stop after N dup/fork/close bogo operations
      --fd-race N             start N workers sending file descriptors over
                              sockets
      --fd-race-ops N         stop after N fd_race bogo operations
      --fd-race-dev           race on /dev/* files
      --fd-race-proc          race on /proc/* files
      --fiemap N              start N workers exercising the FIEMAP ioctl
      --fiemap-bytes N        specify size of file to fiemap
      --fiemap-ops N          stop after N FIEMAP ioctl bogo operations
      --fifo N                start N workers exercising fifo I/O
      --fifo-data-size N      set fifo read/write size in bytes (default 8)
      --fifo-ops N            stop after N fifo bogo operations
      --fifo-readers N        number of fifo reader stressors to start
      --file-ioctl N          start N workers exercising file specific ioctls
      --file-ioctl-ops N      stop after N file ioctl bogo operations
      --filename N            start N workers exercising filenames
      --filename-ops N        stop after N filename bogo operations
      --filename-opts opt     specify allowed filename options
      --flock N               start N workers locking a single file
      --flock-ops N           stop after N flock bogo operations
      --flushcache N          start N CPU instruction + data cache flush
                              workers
      --flushcache-ops N      stop after N flush cache bogo operations
      --fma N                 start N workers performing floating point
                              multiply-add ops
      --fma-ops N             stop after N floating point multiply-add bogo
                              operations
      --fma-libc              use fma libc fused multiply-add helpers
-f N, --fork N                start N workers spinning on fork() and exit()
      --fork-max P            create P forked processes per iteration, default
                              is 1
      --fork-ops N            stop after N fork bogo operations
      --fork-pageout          force pageout memory resident pages
      --fork-unmap            forcibly unmap unused shared library pages
                              (dangerous)
      --fork-vm               enable extra virtual memory pressure
      --forkheavy N           start N workers that rapidly fork and reap
                              resource heavy processes
      --forkheavy-allocs N    attempt to allocate N x resources
      --forkheavy-mlock       attempt to mlock newly mapped pages
      --forkheavy-ops N       stop after N bogo fork operations
      --forkheavy-procs N     attempt to fork N processes
      --fp N                  start N workers performing floating point math
                              ops
      --fp-method M           select the floating point method to operate with
      --fp-ops N              stop after N floating point math bogo operations
      --fp-error N            start N workers exercising floating point errors
      --fp-error-ops N        stop after N fp-error bogo operations
      --fpunch N              start N workers punching holes in a 16MB file
      --fpunch-bytes N        size of file being punched
      --fpunch-ops N          stop after N punch bogo operations
      --fractal N             start N workers performing large integer
                              fractalization
      --fractal-iterations N  number of iterations
      --fractal-method M      fractal method [ mandelbrot | julia ]
      --fractal-ops N         stop after N fractalisation operations
      --fractal-xsize N       width of fractal
      --fractal-ysize N       height of fractal
      --fsize N               start N workers exercising file size limits
      --fsize-ops N           stop after N fsize bogo operations
      --fstat N               start N workers exercising fstat on files
      --fstat-dir path        fstat files in the specified directory
      --fstat-ops N           stop after N fstat bogo operations
      --full N                start N workers exercising /dev/full
      --full-ops N            stop after N /dev/full bogo I/O operations
      --funccall N            start N workers exercising 1 to 9 arg functions
      --funccall-method M     select function call method M
      --funccall-ops N        stop after N function call bogo operations
      --funcret N             start N workers exercising function return
                              copying
      --funcret-method M      select method of exercising a function return
                              type
      --funcret-ops N         stop after N function return bogo operations
      --futex N               start N workers exercising a fast mutex
      --futex-ops N           stop after N fast mutex bogo operations
      --get N                 start N workers exercising the get*() system
                              calls
      --get-ops N             stop after N get bogo operations
      --get-slow-sync         synchronize get*() system amongst N workers
      --getdent N             start N workers reading directories using
                              getdents
      --getdent-ops N         stop after N getdents bogo operations
      --getrandom N           start N workers fetching random data via
                              getrandom()
      --getrandom-ops N       stop after N getrandom bogo operations
      --goto N                start N workers that exercise heavy branching
      --goto-direction D      select goto direction forward, backward, random
      --goto-ops N            stop after 1024 x N goto bogo operations
      --gpu N                 start N GPU worker
      --gpu-devnode name      specify CPU device node name
      --gpu-frag N            specify shader core usage per pixel
      --gpu-ops N             stop after N gpu render bogo operations
      --gpu-tex-size N        specify upload texture NxN
      --gpu-upload N          specify upload texture N times per frame
      --gpu-xsize X           specify framebuffer size x
      --gpu-ysize Y           specify framebuffer size y
      --handle N              start N workers exercising name_to_handle_at
      --handle-ops N          stop after N handle bogo operations
      --hash N                start N workers that exercise various hash
                              functions
      --hash-method M         specify stress hash method M, default is all
      --hash-ops N            stop after N hash bogo operations
-d N, --hdd N                 start N workers spinning on write()/unlink()
      --hdd-bytes N           write N bytes per hdd worker (default is 1GB)
      --hdd-ops N             stop after N hdd bogo operations
      --hdd-opts list         specify list of various stressor options
      --hdd-write-size N      set the default write size to N bytes
      --heapsort N            start N workers heap sorting 32 bit random
                              integers
      --heapsort-method M     select sort method [ heapsort-libc |
                              heapsort-nonlibc
      --heapsort-ops N        stop after N heap sort bogo operations
      --heapsort-size N       number of 32 bit integers to sort
      --hrtimers N            start N workers that exercise high resolution
                              timers
      --hrtimers-adjust       adjust rate to try and maximum timer rate
      --hrtimers-ops N        stop after N bogo high-res timer bogo operations
      --hsearch N             start N workers that exercise a hash table search
      --hsearch-ops N         stop after N hash search bogo operations
      --hsearch-size N        number of integers to insert into hash table
      --icache N              start N CPU instruction cache thrashing workers
      --icache-ops N          stop after N icache bogo operations
      --icmp-flood N          start N ICMP packet flood workers
      --icmp-flood-ops N      stop after N ICMP bogo operations (ICMP packets)
      --idle-page N           start N idle page scanning workers
      --idle-page-ops N       stop after N idle page scan bogo operations
      --inode-flags N         start N workers exercising various inode flags
      --inode-flags-ops N     stop inode-flags workers after N bogo operations
      --inotify N             start N workers exercising inotify events
      --inotify-ops N         stop inotify workers after N bogo operations
      --insertionsort N       start N workers heap sorting 32 bit random
                              integers
      --insertionsort-ops N   stop after N heap sort bogo operations
      --insertionsort-size N  number of 32 bit integers to sort
-i N, --io N                  start N workers spinning on sync()
      --io-ops N              stop sync I/O after N io bogo operations
      --iomix N               start N workers that have a mix of I/O operations
      --iomix-bytes N         write N bytes per iomix worker (default is 1GB)
      --iomix-ops N           stop iomix workers after N iomix bogo operations
      --ioport N              start N workers exercising port I/O
      --ioport-ops N          stop ioport workers after N port bogo operations
      --ioport-opts O         option to select ioport access [ in | out | inout
                              ]
      --ioprio N              start N workers exercising set/get iopriority
      --ioprio-ops N          stop after N io bogo iopriority operations
      --io-uring N            start N workers that issue io-uring I/O requests
      --io-uring-entries N    specify number if io-uring ring entries
      --io-uring-ops N        stop after N bogo io-uring I/O requests
      --io-uring-rand         enable randomized io-uring I/O request ordering
      --ipsec-mb N            start N workers exercising the IPSec MB encoding
      --ipsec-mb-feature F    specify CPU feature F
      --ipsec-mb-jobs N       specify number of jobs to run per round (default
                              1)
      --ipsec-mb-method M     specify crypto/integrity method
      --ipsec-mb-ops N        stop after N ipsec bogo encoding operations
      --itimer N              start N workers exercising interval timers
      --itimer-freq F         set the itimer frequency, limited by jiffy clock
                              rate
      --itimer-ops N          stop after N interval timer bogo operations
      --itimer-rand           enable random interval timer frequency
      --jpeg N                start N workers that burn cycles with no-ops
      --jpeg-height N         image height in pixels
      --jpeg-image type       image type: one of brown, flat, gradient, noise,
                              plasma or xstripes
      --jpeg-ops N            stop after N jpeg bogo no-op operations
      --jpeg-quality Q        compression quality 1 (low) .. 100 (high)
      --jpeg-width N          image width in pixels
      --judy N                start N workers that exercise a judy array search
      --judy-ops N            stop after N judy array search bogo operations
      --judy-size N           number of 32 bit integers to insert into judy
                              array
      --kcmp N                start N workers exercising kcmp
      --kcmp-ops N            stop after N kcmp bogo operations
      --key N                 start N workers exercising key operations
      --key-ops N             stop after N key bogo operations
      --kill N                start N workers killing with SIGUSR1
      --kill-ops N            stop after N kill bogo operations
      --klog N                start N workers exercising kernel syslog
                              interface
      --klog-ops N            stop after N klog bogo operations
      --kvm N                 start N workers exercising /dev/kvm
      --kvm-ops N             stop after N kvm create/run/destroy operations
      --l1cache N             start N CPU level 1 cache thrashing workers
      --l1cache-line-size N   specify level 1 cache line size
      --l1cache-method M      l1 cache thrashing method: forward, reverse,
                              random
      --l1cache-mlock         attempt to mlock memory
      --l1cache-sets N        specify level 1 cache sets
      --l1cache-size N        specify level 1 cache size
      --l1cache-ways N        only fill specified number of cache ways
      --landlock N            start N workers stressing landlock file
                              operations
      --landlock-ops N        stop after N landlock bogo operations
      --lease N               start N workers holding and breaking a lease
      --lease-breakers N      number of lease breaking workers to start
      --lease-ops N           stop after N lease bogo operations
      --led N                 start N workers that read and set LED settings
      --led-ops N             stop after N LED bogo operations
      --link N                start N workers creating hard links
      --link-ops N            stop after N link bogo operations
      --link-sync             enable sync'ing after linking/unlinking
      --list N                start N workers that exercise list structures
      --list-method M         select list method: all, circleq, list, slist,
                              slistt, stailq, tailq
      --list-ops N            stop after N bogo list operations
      --list-size N           N is the number of items in the list
      --llc-affinity N        start N workers exercising low level cache over
                              all CPUs
      --llc-affinity-mlock    attempt to mlock pages into memory
      --llc-affinity-ops N    stop after N low-level-cache bogo operations
      --loadavg N             start N workers that create a large load average
      --loadavg-ops N         stop load average workers after N bogo operations
      --loadavg-max N         set upper limit on number of pthreads to create
      --locka N               start N workers locking a file via advisory locks
      --locka-ops N           stop after N locka bogo operations
      --lockbus N             start N workers locking a memory increment
      --lockbus-nosplit       disable split locks
      --lockbus-ops N         stop after N lockbus bogo operations
      --lockf N               start N workers locking a single file via lockf
      --lockf-nonblock        don't block if lock cannot be obtained, re-try
      --lockf-ops N           stop after N lockf bogo operations
      --lockofd N             start N workers using open file description
                              locking
      --lockofd-ops N         stop after N lockofd bogo operations
      --logmath N             start N workers exercising logarithmic math
                              functions
      --logmath-ops N         stop after N logmath bogo logarithmic math
                              operations
      --logmath-method M      select logarithmic math function to exercise
      --longjmp N             start N workers exercising setjmp/longjmp
      --longjmp-ops N         stop after N longjmp bogo operations
      --loop N                start N workers exercising loopback devices
      --loop-ops N            stop after N bogo loopback operations
      --lsearch N             start N workers that exercise a linear search
      --lsearch-method M      select lsearch method [ lsearch-libc |
                              lsearch-nonlibc ]
      --lsearch-ops N         stop after N linear search bogo operations
      --lsearch-size N        number of 32 bit integers to lsearch
      --lsm N                 start N workers that exercise lsm kernel system
                              calls
      --lsm-ops N             stop after N lsm bogo operations
      --madvise N             start N workers exercising madvise on memory
      --madvise-ops N         stop after N bogo madvise operations
      --madvise-hwpoison      enable hardware page poisoning (disabled by
                              default)
      --malloc N              start N workers exercising malloc/realloc/free
      --malloc-bytes N        allocate up to N bytes per allocation
      --malloc-max N          keep up to N allocations at a time
      --malloc-mlock          attempt to mlock pages into memory
      --malloc-ops N          stop after N malloc bogo operations
      --malloc-pthreads N     number of pthreads to run concurrently
      --malloc-thresh N       threshold where malloc uses mmap instead of sbrk
      --malloc-touch          touch pages force pages to be populated
      --malloc-zerofree       zero free'd memory
      --malloc-trim           enable malloc trimming
      --matrix N              start N workers exercising matrix operations
      --matrix-method M       specify matrix stress method M, default is all
      --matrix-ops N          stop after N maxtrix bogo operations
      --matrix-size N         specify the size of the N x N matrix
      --matrix-yx             matrix operation is y by x instead of x by y
      --matrix-3d N           start N workers exercising 3D matrix operations
      --matrix-3d-method M    specify 3D matrix stress method M, default is all
      --matrix-3d-ops N       stop after N 3D maxtrix bogo operations
      --matrix-3d-size N      specify the size of the N x N x N matrix
      --matrix-3d-zyx         matrix operation is z by y by x instead of x by y
                              by z
      --mcontend N            start N workers that produce memory contention
      --mcontend-ops N        stop memory contention workers after N bogo-ops
      --membarrier N          start N workers performing membarrier system
                              calls
      --membarrier-ops N      stop after N membarrier bogo operations
      --memcpy N              start N workers performing memory copies
      --memcpy-method M       set memcpy method (M = all, libc, builtin,
                              naive..)
      --memcpy-ops N          stop after N memcpy bogo operations
      --memfd N               start N workers allocating memory with
                              memfd_create
      --memfd-bytes N         allocate N bytes for each stress iteration
      --memfd-fds N           number of memory fds to open per stressors
      --memfd-madvise         add random madvise hints to memfd mapped pages
      --memfd-mlock           attempt to mlock pages into memory
      --memfd-ops N           stop after N memfd bogo operations
      --memfd-zap-pte         enable zap pte bug check (slow)
      --memhotplug N          start N workers that exercise memory hotplug
      --memhotplug-mmap       enable random memory mapping/unmapping
      --memhotplug-ops N      stop after N memory hotplug operations
      --memrate N             start N workers exercised memory read/writes
      --memrate-bytes N       size of memory buffer being exercised
      --memrate-ops N         stop after N memrate bogo operations
      --memrate-rd-mbs N      read rate from buffer in megabytes per second
      --memrate-wr-mbs N      write rate to buffer in megabytes per second
      --memrate-flush         flush cache before each iteration
      --memthrash N           start N workers thrashing a 16MB memory buffer
      --memthrash-method M    specify memthrash method M, default is all
      --memthrash-ops N       stop after N memthrash bogo operations
      --mergesort N           start N workers merge sorting 32 bit random
                              integers
      --mergesort-method M    select sort method [ method-libc | method-nonlibc
      --mergesort-ops N       stop after N merge sort bogo operations
      --mergesort-size N      number of 32 bit integers to sort
      --metamix N             start N workers that have a mix of file metadata
                              operations
      --metamix-bytes N       write N bytes per metamix file (default is 1MB,
                              16 files per instance)
      --metamix-ops N         stop metamix workers after N metamix bogo
                              operations
      --mincore N             start N workers exercising mincore
      --mincore-ops N         stop after N mincore bogo operations
      --mincore-random        randomly select pages rather than linear scan
      --min-nanosleep N       start N workers performing short sleeps
      --min-nanosleep-ops N   stop after N bogo sleep operations
      --min-nanosleep-max N   maximum nanosleep delay to be used
      --min-nanosleep-sched P select scheduler policy [ batch, deadline, idle,
                              fifo, other, rr ]
      --misaligned N          start N workers performing misaligned read/writes
      --misaligned-method M   use misaligned memory read/write method
      --misaligned-ops N      stop after N misaligned bogo operations
      --mknod N               start N workers that exercise mknod
      --mknod-ops N           stop after N mknod bogo operations
      --mlock N               start N workers exercising mlock/munlock
      --mlock-ops N           stop after N mlock bogo operations
      --mlockmany N           start N workers exercising many mlock/munlock
                              processes
      --mlockmany-ops N       stop after N mlockmany bogo operations
      --mlockmany-procs N     use N child processes to mlock regions
      --mmap N                start N workers stressing mmap and munmap
      --mmap-async            using asynchronous msyncs for file based mmap
      --mmap-bytes N          mmap and munmap N bytes for each stress iteration
      --mmap-stressful        enable most stressful mmap options (and slowest)
      --mmap-file             mmap onto a file using synchronous msyncs
      --mmap-madvise          enable random madvise on mmap'd region
      --mmap-mergeable        where possible, flag mmap'd pages as mergeable
      --mmap-mlock            attempt to mlock mmap'd pages
      --mmap-mmap2            use mmap2 instead of mmap (when available)
      --mmap-mprotect         enable mmap mprotect stressing
      --mmap-odirect          enable O_DIRECT on file
      --mmap-ops N            stop after N mmap bogo operations
      --mmap-osync            enable O_SYNC on file
      --mmap-slow-munmap      munmap pages inefficiently one at a time
      --mmap-write-check      set check value in each page and perform sanity
                              read check
      --mmapaddr N            start N workers stressing mmap with random
                              addresses
      --mmapaddr-mlock        attempt to mlock pages into memory
      --mmapaddr-ops N        stop after N mmapaddr bogo operations
      --mmapfiles N           start N workers stressing many mmaps and munmaps
      --mmapfiles-ops N       stop after N mmapfiles bogo operations
      --mmapfiles-populate    populate memory mappings
      --mmapfiles-shared      enable shared mappings instead of private
                              mappings
      --mmapfixed N           start N workers stressing mmap with fixed
                              mappings
      --mmapfixed-mlock       attempt to mlock pages into memory
      --mmapfixed-ops N       stop after N mmapfixed bogo operations
      --mmapfork N            start N workers stressing many forked
                              mmaps/munmaps
      --mmapfork-ops N        stop after N mmapfork bogo operations
      --mmapfork-bytes N      mmap and munmap N bytes by workers for each
                              stress iteration
      --mmaphuge N            start N workers stressing mmap with huge mappings
      --mmaphuge-file         perform mappings on a temporary file
      --mmaphuge-mlock        attempt to mlock pages into memory
      --mmaphuge-mmaps N      select number of memory mappings per iteration
      --mmaphuge-ops N        stop after N mmaphuge bogo operations
      --mmapmany N            start N workers stressing many mmaps and munmaps
      --mmapmany-mlock        attempt to mlock pages into memory
      --mmapmany-ops N        stop after N mmapmany bogo operations
      --module N              start N workers performing module requests
      --module-name F         use the specified module name F to load.
      --module-no-unload      skip unload of the module after module load
      --module-no-modver      ignore symbol version hashes
      --module-no-vermag      ignore kernel version magic
      --module-ops N          stop after N module bogo operations
      --monte-carlo N         start N workers performing monte-carlo
                              computations
      --monte-carlo-ops N     stop after N monte-carlo operations
      --monte-carlo-rand R    select random number generator [ all | drand48 |
                              getrandom | lcg | pcg32 | mwc32 | mwc64 | random |
                              xorshift ]
      --monte-carlo-samples N specify number of samples for each computation
      --monte-carlo-method M  select computation method [ pi | e | exp | sin |
                              sqrt | squircle ]
      --mpfr N                start N workers performing multi-precision
                              floating point operations
      --mpfr-ops N            stop after N multi-precision floating point
                              operations
      --mpfr-precision N      specific floating point precision as N bits
      --mprotect N            start N workers exercising mprotect on memory
      --mprotect-ops N        stop after N bogo mprotect operations
      --mq N                  start N workers passing messages using POSIX
                              messages
      --mq-ops N              stop mq workers after N bogo messages
      --mq-size N             specify the size of the POSIX message queue
      --mremap N              start N workers stressing mremap
      --mremap-bytes N        mremap N bytes maximum for each stress iteration
      --mremap-mlock          mlock remap pages, force pages to be unswappable
      --mremap-ops N          stop after N mremap bogo operations
      --mseal N               start N workers exercising sealing of mmap'd
                              memory
      --mseal-ops N           stop mseal workers after N bogo mseal operations
      --msg N                 start N workers stressing System V messages
      --msg-ops N             stop msg workers after N bogo messages
      --msg-types N           enable N different message types
      --msg-bytes N           set the message size 4..8192
      --msync N               start N workers syncing mmap'd data with msync
      --msync-bytes N         size of file and memory mapped region to msync
      --msync-ops N           stop msync workers after N bogo msyncs
      --msyncmany N           start N workers stressing msync on many mapped
                              pages
      --msyncmany-ops N       stop after N msyncmany bogo operations
      --mtx N                 start N workers exercising ISO C mutex operations
      --mtx-ops N             stop after N ISO C mutex bogo operations
      --mtx-procs N           select the number of concurrent processes
      --munmap N              start N workers stressing munmap
      --munmap-ops N          stop after N munmap bogo operations
      --mutex N               start N workers exercising mutex operations
      --mutex-affinity        change CPU affinity randomly across locks
      --mutex-ops N           stop after N mutex bogo operations
      --mutex-procs N         select the number of concurrent processes
      --nanosleep N           start N workers performing short sleeps
      --nanosleep-ops N       stop after N bogo sleep operations
      --nanosleep-threads N   number of threads to run concurrently (default 8)
      --nanosleep-method M    select nanosleep sleep time method [ all | cstate
                              | random ]
      --netdev N              start N workers exercising netdevice ioctls
      --netdev-ops N          stop netdev workers after N bogo operations
      --netlink-proc N        start N workers exercising netlink process events
      --netlink-proc-ops N    stop netlink-proc workers after N bogo events
      --netlink-task N        start N workers exercising netlink tasks events
      --netlink-task-ops N    stop netlink-task workers after N bogo events
      --nice N                start N workers that randomly re-adjust nice
                              levels
      --nice-ops N            stop after N nice bogo operations
      --nop N                 start N workers that burn cycles with no-ops
      --nop-instr INSTR       specify nop instruction to use
      --nop-ops N             stop after N nop bogo no-op operations
      --null N                exercising /dev/null with write, ioctl, lseek,
                              fcntl, fallocate and fdatasync
      --null-ops N            stop after N /dev/null bogo operations
      --null-write            just exercise /dev/null with writing
      --numa N                start N workers stressing NUMA interfaces
      --numa-bytes N          size of memory region to be exercised
      --numa-ops N            stop after N NUMA bogo operations
      --numa-shuffle-addr     shuffle page addresses to move to numa nodes
      --numa-shuffle-node     shuffle numa nodes on numa pages moves
      --oom-pipe N            start N workers exercising large pipes
      --oom-pipe-ops N        stop after N oom-pipe bogo operations
      --opcode N              start N workers exercising random opcodes
      --opcode-method M       set opcode stress method (M = random, inc, mixed,
                              text)
      --opcode-ops N          stop after N opcode bogo operations
-o N, --open N                start N workers exercising open/close
      --open-fd               open files in /proc/$pid/fd
      --open-max N            specficify maximum number of files to open
      --open-ops N            stop after N open/close bogo operations
      --pagemove N            start N workers that shuffle move pages
      --pagemove-bytes N      size of mmap'd region to exercise page moving in
                              bytes
      --pagemove-mlock        attempt to mlock pages into memory
      --pagemove-ops N        stop after N page move bogo operations
      --pageswap N            start N workers that swap pages out and in
      --pageswap-ops N        stop after N page swap bogo operations
      --pci N                 start N workers that read and mmap PCI regions
      --pci-dev name          specify the pci device 'xxxx:xx:xx.x' to exercise
      --pci-ops N             stop after N PCI bogo operations
      --personality N         start N workers that change their personality
      --personality-ops N     stop after N bogo personality calls
      --peterson N            start N workers that exercise Peterson's
                              algorithm
      --peterson-ops N        stop after N peterson mutex bogo operations
      --physpage N            start N workers performing physical page lookup
      --physpage-ops N        stop after N physical page bogo operations
      --physpage-mtrr         enable modification of MTRR types on physical
                              page
      --pidfd N               start N workers exercising pidfd system call
      --pidfd-ops N           stop after N pidfd bogo operations
      --ping-sock N           start N workers that exercises a ping socket
      --ping-sock-ops N       stop after N ping sendto messages
-p N, --pipe N                start N workers exercising pipe I/O
      --pipe-data-size N      set pipe size of each pipe write to N bytes
      --pipe-ops N            stop after N pipe I/O bogo operations
      --pipe-size N           set pipe size to N bytes
      --pipe-vmsplice         use vmsplice for pipe data transfer
-p N, --pipeherd N            start N multi-process workers exercising pipes
                              I/O
      --pipeherd-ops N        stop after N pipeherd I/O bogo operations
      --pipeherd-yield        force processes to yield after each write
      --pkey N                start N workers exercising pkey_mprotect
      --pkey-ops N            stop after N bogo pkey_mprotect bogo operations
      --plugin N              start N workers exercising random plugins
      --plugin-method M       set plugin stress method
      --plugin-ops N          stop after N plugin bogo operations
      --plugin-so file        specify plugin shared object file
-P N, --poll N                start N workers exercising zero timeout polling
      --poll-fds N            use N file descriptors
      --poll-ops N            stop after N poll bogo operations
      --powmath N             start N workers exercising power math functions
      --powmath-ops N         stop after N powmath bogo power math operations
      --powmath-method M      select power math function to exercise
      --prctl N               start N workers exercising prctl system call
      --prctls-ops N          stop prctl workers after N bogo prctl operations
      --prefetch N            start N workers exercising memory prefetching
      --prefetch-l3-size N    specify the L3 cache size of the CPU
      --prefetch-method M     specify the prefetch method
      --prefetch-ops N        stop after N bogo prefetching operations
      --prime N               start N workers that find prime numbers
      --prime-ops N           stop after N prime operations
      --prime-method M        method of searching for next prime [ factorial |
                              inc | pwr2 | pwr10 ]
      --prime-progress        show prime progress every 60 seconds (just first
                              stressor instance)
      --prime-start N         value N from where to start computing primes
      --prio-inv              start N workers exercising priority inversion
                              lock operations
      --prio-inv-ops N        stop after N priority inversion lock bogo
                              operations
      --prio-inv-policy P     select scheduler policy [ batch, idle, fifo,
                              other, rr ]
      --prio-inv-type T       lock protocol type, [ inherit | none | protect ]
      --priv-instr N          start N workers exercising privileged instruction
      --priv-instr-ops N      stop after N bogo instruction operations
      --procfs N              start N workers reading portions of /proc
      --procfs-ops N          stop procfs workers after N bogo read operations
      --pthread N             start N workers that create multiple threads
      --pthread-max P         create P threads at a time by each worker
      --pthread-ops N         stop pthread workers after N bogo threads created
      --ptrace N              start N workers that trace a child using ptrace
      --ptrace-ops N          stop ptrace workers after N system calls are
                              traced
      --pty N                 start N workers that exercise pseudoterminals
      --pty-max N             attempt to open a maximum of N ptys
      --pty-ops N             stop pty workers after N pty bogo operations
-Q N, --qsort N               start N workers qsorting 32 bit random integers
      --qsort-method M        select qsort method [ qsort-libc | qsort_bm ]
      --qsort-ops N           stop after N qsort bogo operations
      --qsort-size N          number of 32 bit integers to sort
      --quota N               start N workers exercising quotactl commands
      --quota-ops N           stop after N quotactl bogo operations
      --race-sched N          start N workers that race cpu affinity
      --race-sched-ops N      stop after N bogo race operations
      --race-sched-method M   method M: all, rand, next, prev, yoyo, randinc
      --radixsort N           start N workers radix sorting random strings
      --radixsort-method M    select sort method [ radixsort-libc |
                              radixsort-nonlibc]
      --radixsort-ops N       stop after N radixsort bogo operations
      --radixsort-size N      number of strings to sort
      --ramfs N               start N workers exercising ramfs mounts
      --ramfs-size N          set the ramfs size in bytes, e.g. 2M is 2MB
      --ramfs-fill            attempt to fill ramfs
      --ramfs-ops N           stop after N bogo ramfs mount operations
      --randlist N            start N workers that exercise random ordered list
      --randlist-compact      reduce mmap and malloc overheads
      --randlist-items N      number of items in the random ordered list
      --randlist-ops N        stop after N randlist bogo no-op operations
      --randlist-size N       size of data in each item in the list
      --rawdev N              start N workers that read a raw device
      --rawdev-method M       specify the rawdev read method to use
      --rawdev-ops N          stop after N rawdev read operations
      --rawpkt N              start N workers exercising raw packets
      --rawpkt-ops N          stop after N raw packet bogo operations
      --rawpkt-port P         use raw packet ports P to P + number of workers -
                              1
      --rawpkt-rxring N       setup raw packets with RX ring with N number of
                              blocks, this selects TPACKET_V3
      --rawsock N             start N workers performing raw socket
                              send/receives
      --rawsock-ops N         stop after N raw socket bogo operations
      --rawsock-port P        use socket P to P + number of workers - 1
      --rawudp N              start N workers exercising raw UDP socket I/O
      --rawudp-if I           use network interface I, e.g. lo, eth0, etc.
      --rawudp-ops N          stop after N raw socket UDP bogo operations
      --rawudp-port P         use raw socket ports P to P + number of workers -
                              1
      --rdrand N              start N workers exercising rdrand (x86 only)
      --rdrand-ops N          stop after N rdrand bogo operations
      --rdrand-seed           use rdseed instead of rdrand
      --readahead N           start N workers exercising file readahead
      --readahead-bytes N     size of file to readahead on (default is 1GB)
      --readahead-ops N       stop after N readahead bogo operations
      --reboot N              start N workers that exercise bad reboot calls
      --reboot-ops N          stop after N bogo reboot operations
      --regs N                start N workers exercising CPU generic registers
      --regs-ops N            stop after N x 1000 rounds of register shuffling
      --remap N               start N workers exercising page remappings
      --remap-mlock           attempt to mlock pages into memory
      --remap-ops N           stop after N remapping bogo operations
      --remap-pages N         specify N pages to remap (N must be power of 2)
-R,   --rename N              start N workers exercising file renames
      --rename-ops N          stop after N rename bogo operations
      --resched N             start N workers that spawn renicing child
                              processes
      --resched-ops N         stop after N nice bogo nice'd yield operations
      --resources N           start N workers consuming system resources
      --resources-mlock       attempt to mlock pages into memory
      --resources-ops N       stop after N resource bogo operations
      --revio N               start N workers performing reverse I/O
      --revio-bytes N         specify file size (default is 1GB)
      --revio-ops N           stop after N revio bogo operations
      --revio-opts list       specify list of various stressor options
      --ring-pipe N           start N workers exercising a ring of pipes
      --ring-pipe-num N       number of pipes to use
      --ring-pipe-ops N       stop after N ring pipe I/O bogo operations
      --ring-pipe-size N      size of data to be written and read in bytes
      --ring-pipe-splice      use splice instead of read+write
      --rmap N                start N workers that stress reverse mappings
      --rmap-ops N            stop after N rmap bogo operations
      --rmap N                start N workers that stress reverse mappings
      --rmap-ops N            stop after N rmap bogo operations
      --rotate N              start N workers performing rotate ops
      --rotate-method M       select rotate method M
      --rotate-ops N          stop after N rotate bogo operations
      --rseq N                start N workers that exercise restartable
                              sequences
      --rseq-ops N            stop after N bogo restartable sequence operations
      --rtc N                 start N workers that exercise the RTC interfaces
      --rtc-ops N             stop after N RTC bogo operations
      --schedmix N            start N workers that exercise a mix of scheduling
                              loads
      --schedmix-ops N        stop after N schedmix bogo operations
      --schedmix-procs N      select number of schedmix child processes 1..64
      --schedpolicy N         start N workers that exercise scheduling policy
      --schedpolicy-ops N     stop after N scheduling policy bogo operations
      --schedpolicy-rand      select scheduling policy randomly
      --sctp N                start N workers performing SCTP send/receives
      --sctp-domain D         specify sctp domain, default is ipv4
      --sctp-if I             use network interface I, e.g. lo, eth0, etc.
      --sctp-ops N            stop after N SCTP bogo operations
      --sctp-port P           use SCTP ports P to P + number of workers - 1
      --sctp-sched S          specify sctp scheduler
      --seal N                start N workers performing fcntl SEAL commands
      --seal-ops N            stop after N SEAL bogo operations
      --seccomp N             start N workers performing seccomp call filtering
      --seccomp-ops N         stop after N seccomp bogo operations
      --secretmem N           start N workers that use secretmem mappings
      --secretmem-ops N       stop after N secretmem bogo operations
      --seek N                start N workers performing random seek r/w IO
      --seek-ops N            stop after N seek bogo operations
      --seek-punch            punch random holes in file to stress extents
      --seek-size N           length of file to do random I/O upon
      --sem N                 start N workers doing semaphore operations
      --sem-ops N             stop after N semaphore bogo operations
      --sem-procs N           number of processes to start per worker
      --sem-sysv N            start N workers doing System V semaphore
                              operations
      --sem-sysv-ops N        stop after N System V sem bogo operations
      --sem-sysv-procs N      number of processes to start per worker
      --sendfile N            start N workers exercising sendfile
      --sendfile-ops N        stop after N bogo sendfile operations
      --sendfile-size N       size of data to be sent with sendfile
-f N, --session N             start N workers that exercise new sessions
      --session-ops N         stop after N session bogo operations
      --set N                 start N workers exercising the set*() system
                              calls
      --set-ops N             stop after N set bogo operations
      --shellsort N           start N workers shell sorting 32 bit random
                              integers
      --shellsort-ops N       stop after N shell sort bogo operations
      --shellsort-size N      number of 32 bit integers to sort
      --shm N                 start N workers that exercise POSIX shared memory
      --shm-bytes N           allocate/free N bytes of POSIX shared memory
      --shm-mlock             attempt to mlock pages into memory
      --shm-objs N            allocate N POSIX shared memory objects per
                              iteration
      --shm-ops N             stop after N POSIX shared memory bogo operations
      --shm-sysv N            start N workers that exercise System V shared
                              memory
      --shm-sysv-bytes N      allocate and free N bytes of shared memory per
                              loop
      --shm-sysv-mlock        attempt to mlock pages into memory
      --shm-sysv-ops N        stop after N shared memory bogo operations
      --shm-sysv-segs N       allocate N shared memory segments per iteration
      --sigabrt N             start N workers generating segmentation faults
      --sigabrt-ops N         stop after N bogo segmentation faults
      --sigbus N              start N workers generating bus faults
      --sigbus-ops N          stop after N bogo bus faults
      --sigchld N             start N workers that handle SIGCHLD
      --sigchld-ops N         stop after N bogo SIGCHLD signals
      --sigfd N               start N workers reading signals via signalfd
                              reads
      --sigfd-ops N           stop after N bogo signalfd reads
      --sigfpe N              start N workers generating floating point math
                              faults
      --sigfpe-ops N          stop after N bogo floating point math faults
      --sigio N               start N workers that exercise SIGIO signals
      --sigio-ops N           stop after N bogo sigio signals
      --sigill N              start N workers generating SIGILL signals
      --sigill-ops N          stop after N SIGILL signals
      --signal N              start N workers that exercise signal
      --signal-ops N          stop after N bogo signals
      --signest N             start N workers generating nested signals
      --signest-ops N         stop after N bogo nested signals
      --sigpending N          start N workers exercising sigpending
      --sigpending-ops N      stop after N sigpending bogo operations
      --sigpipe N             start N workers exercising SIGPIPE
      --sigpipe-ops N         stop after N SIGPIPE bogo operations
      --sigq N                start N workers sending sigqueue signals
      --sigq-ops N            stop after N sigqueue bogo operations
      --sigrt N               start N workers sending real time signals
      --sigrt-ops N           stop after N real time signal bogo operations
      --sigsegv N             start N workers generating segmentation faults
      --sigsegv-ops N         stop after N bogo segmentation faults
      --sigsuspend N          start N workers exercising sigsuspend
      --sigsuspend-ops N      stop after N bogo sigsuspend wakes
      --sigtrap N             start N workers generating segmentation faults
      --sigtrap-ops N         stop after N bogo segmentation faults
      --sigurg N              start N workers exercising SIGURG on MSB_OOB
                              socket sends
      --sigurg-ops N          stop after N SIGURG signals
      --sigvtalrm N           start N workers exercising SIGVTALRM signals
      --sigvtalrm-ops N       stop after N SIGVTALRM signals
      --sigxcpu N             start N workers that exercise SIGXCPU signals
      --sigxcpu-ops N         stop after N bogo SIGXCPU signals
      --sigxfsz N             start N workers that exercise SIGXFSZ signals
      --sigxfsz-ops N         stop after N bogo SIGXFSZ signals
      --skiplist N            start N workers that exercise a skiplist search
      --skiplist-ops N        stop after N skiplist search bogo operations
      --skiplist-size N       number of 32 bit integers to add to skiplist
      --sleep N               start N workers performing various duration
                              sleeps
      --sleep-max P           create P threads at a time by each worker
      --sleep-ops N           stop after N bogo sleep operations
      --smi N                 start N workers that trigger SMIs
      --smi-ops N             stop after N SMIs have been triggered
-S N, --sock N                start N workers exercising socket I/O
      --sock-domain D         specify socket domain, default is ipv4
      --sock-if I             use network interface I, e.g. lo, eth0, etc.
      --sock-msgs N           number of messages to send per connection
      --sock-nodelay          disable Nagle algorithm, send data immediately
      --sock-ops N            stop after N socket bogo operations
      --sock-opts option      socket options [send|sendmsg|sendmmsg]
      --sock-port P           use socket ports P to P + number of workers - 1
      --sock-protocol         use socket protocol P, default is tcp, can be
                              mptcp
      --sock-type T           socket type (stream, seqpacket)
      --sock-zerocopy         enable zero copy sends
      --sockabuse N           start N workers abusing socket I/O
      --sockabuse-ops N       stop after N socket abusing bogo operations
      --sockabuse-port P      use socket ports P to P + number of workers - 1
      --sockdiag N            start N workers exercising sockdiag netlink
      --sockdiag-ops N        stop sockdiag workers after N bogo messages
      --sockfd N              start N workers sending file descriptors over
                              sockets
      --sockfd-ops N          stop after N sockfd bogo operations
      --sockfd-port P         use socket fd ports P to P + number of workers -
                              1
      --sockfd-reuse          re-use file descriptors between sender and
                              receiver
      --sockmany N            start N workers exercising many socket
                              connections
      --sockmany-if I         use network interface I, e.g. lo, eth0, etc.
      --sockmany-ops N        stop after N sockmany bogo operations
      --sockmany-port         use socket ports P to P + number of workers - 1
      --sockpair N            start N workers exercising socket pair I/O
                              activity
      --sockpair-ops N        stop after N socket pair bogo operations
      --softlockup N          start N workers that cause softlockups
      --softlockup-ops N      stop after N softlockup bogo operations
      --sparsematrix N        start N workers that exercise a sparse matrix
      --sparsematrix-items N  N is the number of items in the spare matrix
      --sparsematrix-method M select storage method: all, hash, hashjudy, judy,
                              list, mmap, qhash, rb, splay
      --sparsematrix-ops N    stop after N bogo sparse matrix operations
      --sparsematrix-size N   M is the width and height X x Y of the matrix
      --spawn N               start N workers spawning stress-ng using
                              posix_spawn
      --spawn-ops N           stop after N spawn bogo operations
      --spinmem               start N workers exercising shared memory spin
                              write/read operations
      --spinmem-ops           stop after N bogo shared memory spin write/read
                              operations
      --spinmem-method        select method of write/reads, default is 32bit
      --splice N              start N workers reading/writing using splice
      --splice-bytes N        number of bytes to transfer per splice call
      --splice-ops N          stop after N bogo splice operations
      --stack N               start N workers generating stack overflows
      --stack-fill            fill stack, touches all new pages
      --stack-mlock           mlock stack, force pages to be unswappable
      --stack-ops N           stop after N bogo stack overflows
      --stack-pageout         use madvise to try to swap out stack
      --stack-unmap           unmap a page in the stack on each iteration
      --stackmmap N           start N workers exercising a filebacked stack
      --stackmmap-ops N       stop after N bogo stackmmap operations
      --mount N               start N workers exercising statmount and
                              listmount
      --mount-ops N           stop after N bogo statmount and listmount
                              operations
      --str N                 start N workers exercising lib C string functions
      --str-method func       specify the string function to stress
      --str-ops N             stop after N bogo string operations
      --stream N              start N workers exercising memory bandwidth
      --stream-index N        specify number of indices into the data (0..3)
      --stream-l3-size N      specify the L3 cache size of the CPU
      --stream-madvise M      specify mmap'd stream buffer madvise advice
      --stream-mlock          attempt to mlock pages into memory
      --stream-ops N          stop after N bogo stream operations
      --swap N                start N workers exercising swapon/swapoff
      --swap-ops N            stop after N swapon/swapoff operations
      --swap-self             attempt to swap stressors pages out
-s N, --switch N              start N workers doing rapid context switches
      --switch-freq N         set frequency of context switches
      --switch-method M       mq | pipe | sem-sysv
      --switch-ops N          stop after N context switch bogo operations
      --symlink N             start N workers creating symbolic links
      --symlink-ops N         stop after N symbolic link bogo operations
      --symlink-sync          enable sync'ing after symlinking/unsymlinking
      --sync-file N           start N workers exercise sync_file_range
      --sync-file-bytes N     size of file to be sync'd
      --sync-file-ops N       stop after N sync_file_range bogo operations
      --syncload N            start N workers that synchronize load spikes
      --syncload-msbusy M     maximum busy duration in milliseconds
      --syncload-mssleep M    maximum sleep duration in milliseconds
      --syncload-ops N        stop after N syncload bogo operations
      --sysbadaddr N          start N workers that pass bad addresses to
                              syscalls
      --sysbadaddr-ops N      stop after N sysbadaddr bogo syscalls
      --syscall N             start N workers that exercise a wide range of
                              system calls
      --syscall-method M      select method of selecting system calls to
                              exercise
      --syscall-ops N         stop after N syscall bogo operations
      --syscall-top N         display fastest top N system calls
      --sysfs N               start N workers reading files from /sys
      --sysfs-ops N           stop after sysfs bogo operations
      --sysinfo N             start N workers reading system information
      --sysinfo-ops N         stop after sysinfo bogo operations
      --sysinval N            start N workers that pass invalid args to
                              syscalls
      --sysinval-ops N        stop after N sysinval bogo syscalls
      --tee N                 start N workers exercising the tee system call
      --tee-ops N             stop after N tee bogo operations
-T N, --timer N               start N workers producing timer events
      --timer-freq F          run timer(s) at F Hz, range 1 to 1000000000
      --timer-ops N           stop after N timer bogo events
      --timer-rand            enable random timer frequency
      --timerfd N             start N workers producing timerfd events
      --timerfd-fds N         number of timerfd file descriptors to open
      --timerfd-freq F        run timer(s) at F Hz, range 1 to 1000000000
      --timerfd-ops N         stop after N timerfd bogo events
      --timerfd-rand          enable random timerfd frequency
      --time-warp N           start N workers checking for timer/clock warping
      --time-warp-ops N       stop workers after N bogo timer/clock reads
      --tlb-shootdown N       start N workers that force TLB shootdowns
      --tlb-shootdown-ops N   stop after N TLB shootdown bogo ops
      --tmpfs N               start N workers mmap'ing a file on tmpfs
      --tmpfs-mmap-async      using asynchronous msyncs for tmpfs file based
                              mmap
      --tmpfs-mmap-file       mmap onto a tmpfs file using synchronous msyncs
      --tmpfs-ops N           stop after N tmpfs bogo ops
      --touch N               start N stressors that touch and remove files
      --touch-method M        specify method to touch tile file, open | create
      --touch-ops N           stop after N touch bogo operations
      --touch-opts list       touch open options all, direct, dsync, excl,
                              noatime, sync, trunc
      --tree N                start N workers that exercise tree structures
      --tree-method M         select tree method: all,avl,binary,btree,rb,splay
      --tree-ops N            stop after N bogo tree operations
      --tree-size N           N is the number of items in the tree
      --trig N                start N workers exercising trigonometric
                              functions
      --trig-ops N            stop after N trig bogo trigonometric operations
      --trig-method M         select trigonometric function to exercise
      --tsc N                 start N workers reading the time stamp counter
      --tsc-ops N             stop after N TSC bogo operations
      --tsc-lfence            add lfence after TSC reads for serialization (x86
                              only)
      --tsc-rdscp             use rdtscp instead of rdtsc, disables tsc-lfence
                              (x86 only)
      --tsearch N             start N workers that exercise a tree search
      --tsearch-ops N         stop after N tree search bogo operations
      --tsearch-size N        number of 32 bit integers to tsearch
      --tun N                 start N workers exercising tun interface
      --tun-ops N             stop after N tun bogo operations
      --tun-tap               use TAP interface instead of TUN
      --udp N                 start N workers performing UDP send/receives
      --udp-domain D          specify domain, default is ipv4
      --udp-gro               enable UDP-GRO
      --udp-if I              use network interface I, e.g. lo, eth0, etc.
      --udp-lite              use the UDP-Lite (RFC 3828) protocol
      --udp-ops N             stop after N udp bogo operations
      --udp-port P            use ports P to P + number of workers - 1
      --udp-flood N           start N workers that performs a UDP flood attack
      --udp-flood-domain D    specify domain, default is ipv4
      --udp-flood-if I        use network interface I, e.g. lo, eth0, etc.
      --udp-flood-ops N       stop after N udp flood bogo operations
      --umount N              start N workers exercising umount races
      --umount-ops N          stop after N bogo umount operations
      --unlink N              start N unlink exercising stressors
      --unlink-ops N          stop after N unlink exercising bogo operations
      --unshare N             start N workers exercising resource unsharing
      --unshare-ops N         stop after N bogo unshare operations
      --uprobe N              start N workers that generate uprobe events
      --uprobe-ops N          stop after N uprobe events
-u N, --urandom N             start N workers reading /dev/urandom
      --urandom-ops N         stop after N urandom bogo read operations
      --userfaultfd N         start N page faulting workers with userspace
                              handling
      --userfualtfd-bytes N   size of mmap'd region to fault on
      --userfaultfd-ops N     stop after N page faults have been handled
      --usersyscall N         start N workers exercising a userspace system
                              call handler
      --usersyscall-ops N     stop after N successful SIGSYS system calls
      --utime N               start N workers updating file timestamps
      --utime-fsync           force utime meta data sync to the file system
      --utime-ops N           stop after N utime bogo operations
      --vdso N                start N workers exercising functions in the VDSO
      --vdso-func F           use just vDSO function F
      --vdso-ops N            stop after N vDSO function calls
      --vecfp N               start N workers performing vector math ops
      --vecfp-ops N           stop after N vector math bogo operations
      --vecmath N             start N workers performing vector math ops
      --vecmath-ops N         stop after N vector math bogo operations
      --vecshuf N             start N workers performing vector shuffle ops
      --vecshuf-method M      select vector shuffling method
      --vecshuf-ops N         stop after N vector shuffle bogo operations
      --vecwide N             start N workers performing vector math ops
      --vecwide-ops N         stop after N vector math bogo operations
      --verity N              start N workers exercising file verity ioctls
      --verity-ops N          stop after N file verity bogo operations
      --vfork N               start N workers spinning on vfork() and exit()
      --vfork-ops N           stop after N vfork bogo operations
      --vfork-max P           create P vforked processes per iteration, default
                              is 1
      --vforkmany N           start N workers spawning many vfork children
      --vforkmany-ops N       stop after spawning N vfork children
      --vforkmany-vm          enable extra virtual memory pressure
      --vforkmany-vm-bytes    set the default vm mmap'd size, default 64MB
-m N, --vm N                  start N workers spinning on anonymous mmap
      --vm-bytes N            allocate N bytes per vm worker (default 256MB)
      --vm-hang N             sleep N seconds before freeing memory
      --vm-keep               redirty memory instead of reallocating
      --vm-locked             lock the pages of the mapped region into memory
      --vm-madvise M          specify mmap'd vm buffer madvise advice
      --vm-method M           specify stress vm method M, default is all
      --vm-ops N              stop after N vm bogo operations
      --vm-populate           populate (prefault) page tables for a mapping
      --vma N                 start N workers that exercise kernel VMA
                              structures
      --vma-ops N             stop N workers after N mmap VMA operations
      --vm-addr N             start N vm address exercising workers
      --vm-addr-method M      select method to exercise vm addresses
      --vm-addr-mlock         attempt to mlock pages into memory
      --vm-addr-ops N         stop after N vm address bogo operations
      --vm-rw N               start N vm read/write process_vm* copy workers
      --vm-rw-bytes N         transfer N bytes of memory per bogo operation
      --vm-rw-ops N           stop after N vm process_vm* copy bogo operations
      --vm-segv N             start N workers that unmap their address space
      --vm-segv-ops N         stop after N vm-segv unmap'd SEGV faults
      --vm-splice N           start N workers reading/writing using vmsplice
      --vm-splice-bytes N     number of bytes to transfer per vmsplice call
      --vm-splice-ops N       stop after N bogo splice operations
      --vnni N                start N workers performing vector neural network
                              ops
      --vnni-intrinsic        use x86 intrinsic vnni methods, disable generic
                              methods
      --vnni-method M         specify specific vnni methods to exercise
      --vnni-ops N            stop after N vnni bogo operations
      --wait N                start N workers waiting on child being
                              stop/resumed
      --wait-ops N            stop after N bogo wait operations
      --waitcpu N             start N workers exercising wait/pause/nop
                              instructions
      --waitcpu-ops N         stop after N wait/pause/nop bogo operations
      --watchdog N            start N workers that exercise /dev/watchdog
      --watchdog-ops N        stop after N bogo watchdog operations
      --wcs N                 start N workers on lib C wide char string
                              functions
      --wcs-method func       specify the wide character string function to
                              stress
      --wcs-ops N             stop after N bogo wide character string
                              operations
      --workload N            start N workers that exercise a mix of scheduling
                              loads
      --workload-dist type    workload distribution type [random1, random2,
                              random3, cluster]
      --workload-load P       percentage load P per workload time slice
      --workload-ops N        stop after N workload bogo operations
      --workload-quanta-us N  max duration of each quanta work item in
                              microseconds
      --workload-sched P      select scheduler policy [idle, fifo, rr, other,
                              batch, deadline]
      --workload-slice-us N   duration of workload time load in microseconds
      --workload-threads N    number of workload threads workers to use,
                              default is 0 (disabled)
      --workload-method M     select a workload method, default is all
      --x86cpuid N            start N workers exercising the x86 cpuid
                              instruction
      --x86cpuid-ops N        stop after N cpuid bogo operations
      --x86syscall N          start N workers exercising functions using
                              syscall
      --x86syscall-func F     use just syscall function F
      --x86syscall-ops N      stop after N syscall function calls
      --xattr N               start N workers stressing file extended
                              attributes
      --xattr-ops N           stop after N bogo xattr operations
-y N, --yield N               start N workers doing sched_yield() calls
      --yield-ops N           stop after N bogo yield operations
      --yield-procs N         specify number of yield processes per stressor
      --yield-sched P         select scheduler policy [idle, fifo, rr, other,
                              batch, deadline]
      --zero N                start N workers exercising /dev/zero with read,
                              mmap, ioctl, lseek
      --zero-read             just exercise /dev/zero with reading
      --zero-ops N            stop after N /dev/zero bogo read operations
      --zlib N                start N workers compressing data with zlib
      --zlib-level L          specify zlib compression level 0=fast, 9=best
      --zlib-mem-level L      specify zlib compression state memory usage
                              1=minimum, 9=maximum
      --zlib-method M         specify zlib random data generation method M
      --zlib-ops N            stop after N zlib bogo compression operations
      --zlib-strategy S       specify zlib strategy 0=default, 1=filtered,
                              2=huffman only, 3=rle, 4=fixed
      --zlib-stream-bytes S   specify the number of bytes to deflate until the
                              current stream will be closed
      --zlib-window-bits W    specify zlib window bits -8-(-15) | 8-15 | 24-31
                              | 40-47
      --zombie N              start N workers that rapidly create and reap
                              zombies
      --zombie-max N          set upper limit of N zombies per worker
      --zombie-ops N          stop after N bogo zombie fork operations

Example: stress-ng --cpu 8 --iomix 4 --vm 2 --vm-bytes 128M --fork 4 --timeout 10s

Note: sizes can be suffixed with B, K, M, G and times with s, m, h, d, y
```