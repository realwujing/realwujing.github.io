#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

// KVM 相关的宏定义
#include <linux/kvm.h>

int main()
{
    struct kvm_sregs sregs;
    int ret;
    int kvmfd = open("/dev/kvm", O_RDWR);
    ioctl(kvmfd, KVM_GET_API_VERSION, NULL);
    int vmfd = ioctl(kvmfd, KVM_CREATE_VM, 0);
    unsigned char *ram = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int kfd = open("test.bin", O_RDONLY);
    read(kfd, ram, 4096);
    struct kvm_userspace_memory_region mem = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = 0x1000,
        .userspace_addr = (unsigned long)ram,
    };
    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &mem);
    int vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, 0);
    int mmap_size = ioctl(kvmfd, KVM_GET_VCPU_MMAP_SIZE, NULL);
    struct kvm_run *run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                                MAP_SHARED, vcpufd, 0);
    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    struct kvm_regs regs = {
        .rip = 0,
    };
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
    while (1)
    {
        ret = ioctl(vcpufd, KVM_RUN, NULL);
        if (ret == -1)
        {
            printf("exit unknown\n");
            return -1;
        }
        switch (run->exit_reason)
        {
        case KVM_EXIT_HLT:
            puts("KVM_EXIT_HLT");
            return 0;
        case KVM_EXIT_IO:
            putchar(*(((char *)run) + run->io.data_offset));
            break;
        case KVM_EXIT_FAIL_ENTRY:
            puts("entry error");
            return -1;
        default:
            puts("other error");
            printf("exit_reason: %d\n", run->exit_reason);
            return -1;
        }
    }
}