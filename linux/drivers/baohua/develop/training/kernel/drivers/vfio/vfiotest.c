// The purpose of this test is to show that VFIO works and allows a simple user space DMA solution.

// This assumes the kernel has the VFIO platform driver built into it (as a module)
// The AXI DMA should be in the device tree with the iommu property.

// The hardware must have been built properly to allow the DMA to IOMMU (SMMU) to the DMA to work with
// virtual addresses. It assumes the AXI DMA is connected to HPC0 of the MPSOC.

// The vfio driver is used with the following commands prior to running this test application.
// Maybe it can be used from the device tree like UIO, but not sure yet.

// modprobe vfio_platform reset_required=0
// echo a0000000.dma > /sys/bus/platform/drivers/xilinx-vdma/unbind
// echo vfio-platform > /sys/bus/platform/devices/a0000000.dma/driver_override
// echo a0000000.dma > /sys/bus/platform/drivers_probe

#include <linux/vfio.h>
#include <linux/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>

#include <time.h>

#define VFIO_CONTAINER 		"/dev/vfio/vfio"
#define AXIDMA_VFIO_GROUP 	"/dev/vfio/11"			// dmesg | grep group, look for axi dma address
#define AXIDMA_DEVICE		"a0000000.dma"			// the address for the device that is with the group

#define DMA_TRANSFER_SIZE_4KPAGES	(1024)			// 4K pages

typedef __u8 uchar;
typedef __u32 uint;
typedef __u32 u32;
typedef __u64 u64;

int main(int argc, char **argv)
{
	int container, group, device;
	unsigned int i;

	struct vfio_group_status group_status = { .argsz = sizeof(group_status) };
	struct vfio_iommu_type1_info iommu_info = { .argsz = sizeof(iommu_info) };
	// source memory area the DMA controller will read from
	struct vfio_iommu_type1_dma_map dma_map_src = { .argsz = sizeof(dma_map_src) };
	// destination memory area the DMA controller will read to
	struct vfio_iommu_type1_dma_map dma_map_dst = { .argsz = sizeof(dma_map_dst) };

	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };

	int ret, fail_count = 0;

	/* Create a new container */
	container = open(VFIO_CONTAINER, O_RDWR);

	if (ioctl(container, VFIO_GET_API_VERSION) != VFIO_API_VERSION) {
		printf("Unknown API version\n");
		return 1;
	}

	if (!ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)) {
		printf("Doesn't support the IOMMU driver we want\n");
		return 1;
	}

	/* Open the group */
	group = open(AXIDMA_VFIO_GROUP, O_RDWR);

	/* Test the group is viable and available */
	ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		printf("Group is not viable (not all devices bound for vfio)\n");
		return 1;
	}

	/* Add the group to the container */
	ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);

	/* Enable the IOMMU model we want */
	ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);

	/* Get addition IOMMU info */
	ioctl(container, VFIO_IOMMU_GET_INFO, &iommu_info);

	int size_to_map = getpagesize() * DMA_TRANSFER_SIZE_4KPAGES;

	// Get a buffer for the source of the DMA transfer and then map into the IOMMU

	dma_map_src.vaddr = (u64)((uintptr_t)mmap(NULL, size_to_map, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0));
	dma_map_src.size = size_to_map;
	dma_map_src.iova = 0;
	dma_map_src.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;
	ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map_src);
	if(ret) {
		printf("Could not map DMA src memory\n");
		return 1;
	}

	// Get a buffer for the destination of the DMA transfer and then map it into the IOMMU

	dma_map_dst.vaddr = (u64)((uintptr_t)mmap(NULL, size_to_map, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0));
	dma_map_dst.size = size_to_map;
	dma_map_dst.iova = dma_map_src.size;
	dma_map_dst.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;
	ret = ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map_dst);
	if(ret) {
		printf("Could not map DMA dest memory\n");
		return 1;
	}

	/* Get a file descriptor for the device */
	device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, AXIDMA_DEVICE);
	printf("=== VFIO device file descriptor %d ===\n", device);

	/* Test and setup the device */
	ret = ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);

	if(ret) {
		printf("Could not get VFIO device\n");
		return 1;
	}

	printf("Device has %d region(s):\n", device_info.num_regions);

	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	uchar *base_regs;

	reg.index = 0;
	ret = ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &reg);

	if(ret) {
		printf("Couldn't get region %d info\n", reg.index);
		return 1;
	}

	printf("- Region %d: size=0x%llx offset=0x%llx flags=0x%x\n",
			reg.index,
			reg.size,
			reg.offset,
			reg.flags );

	base_regs = (uchar *)mmap(NULL, reg.size, PROT_READ | PROT_WRITE, MAP_SHARED,
			device, reg.offset);

	if (base_regs != MAP_FAILED)
		printf("Successful MMAP of AXI DMA to address %p\n", base_regs);


	int *src_ptr = (int *)((uintptr_t)dma_map_src.vaddr);
	int *dst_ptr = (int *)((uintptr_t)dma_map_dst.vaddr);

	// fill with random data
	int c;
	int tot = dma_map_src.size/sizeof(*src_ptr);
	srand(time(NULL));
	for(c = 0; c < tot; c++) {
		src_ptr[c] = rand();
	}

	printf("source value: 0x%x\n", *src_ptr);
	printf("destination value: 0x%x\n", *dst_ptr);

	// AXI DMA transfer, with tx looped back to tx, no SG, polled I/O
	// reset both tx and rx channels and wait for the reset to clear for the last one

	*(u32 *)(base_regs + 0x00) = 4;
	*(u32 *)(base_regs + 0x30) = 4;

	while (	*(u32 *)(base_regs + 0x30) & 0x4);

	// Start the rx transfer

	*(u32 *)(base_regs + 0x30) = 1;
	*(u32 *)(base_regs + 0x48) = (u32)(u64)dma_map_dst.iova;
	*(u32 *)(base_regs + 0x4c) = (u32)((u64)dma_map_dst.iova >> 32);
	*(u32 *)(base_regs + 0x58) = size_to_map;

	// Start the tx transfer

	*(u32 *)(base_regs) = 1;
	*(u32 *)(base_regs + 0x18) = (u32)(u64)dma_map_src.iova;
	*(u32 *)(base_regs + 0x1c) = (u32)((u64)dma_map_src.iova >> 32);
	*(u32 *)(base_regs + 0x28) = size_to_map;

	// Wait for the rx to finish

	while ((*(u32 *)(base_regs + 0x34) & 0x1000) != 0x1000);

	// Compare the destination to the source to make sure they match after the DMA transfer

	for(c = 0; c < tot; c++) {
		if(src_ptr[c] != dst_ptr[c]) {
			printf("test failed! - %d - 0x%x - 0x%x\n", c, src_ptr[c], dst_ptr[c]);
			fail_count++;
		}
	}
	if (!fail_count)
		printf("test success\n");

	printf("source value: 0x%x\n", *((uint *)src_ptr));
	printf("destination value: 0x%x\n", *((uint *)dst_ptr));

	// Unmap the buffers used in the IOMMU

	struct vfio_iommu_type1_dma_unmap dma_unmap_src = { .argsz = sizeof(dma_unmap_src) };
	struct vfio_iommu_type1_dma_unmap dma_unmap_dst = { .argsz = sizeof(dma_unmap_dst) };

	dma_unmap_src.size = size_to_map;
	dma_unmap_src.iova = 0;

	dma_unmap_dst.size = size_to_map;
	dma_unmap_dst.iova = dma_unmap_src.size;

	ret = ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap_src);
	if(ret) {
		printf("Could not unmap DMA src memory\n");
		return 1;
	}

	ret = ioctl(container, VFIO_IOMMU_UNMAP_DMA, &dma_unmap_dst);
	if(ret) {
		printf("Could not unmap DMA dest memory\n");
		return 1;
	}

	return 0;
}

