---
title: 'minifs-v3'
date: '2025/05/13 20:36:27'
updated: '2025/05/13 20:36:27'
---

# minifs-v3

```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/bio.h>

#define MINIFS_NAME "minifs"
#define MINIFS_MAGIC 0x4D494E49 /* "MINI" in ASCII */
#define MINIFS_BLOCK_SIZE 4096
#define MINIFS_MAX_FILES 100
#define MINIFS_METADATA_START 1
#define MINIFS_CONTENT_START (MINIFS_METADATA_START + MINIFS_MAX_FILES)

/* 磁盘上的超级块结构 */
struct minifs_superblock {
    __u32 magic;          /* 文件系统魔数 */
    __u32 file_count;     /* 文件数量 */
    __u32 next_ino;       /* 下一个 inode 编号 */
    __u8 reserved[4084];  /* 填充到块大小 */
};

/* 磁盘上的文件元数据 */
struct minifs_file_metadata {
    char name[256];       /* 文件名 */
    __u32 ino;            /* inode 编号 */
    __u32 mode;           /* 文件权限 */
    __u64 size;           /* 文件大小 */
    __u64 offset;         /* 文件内容在块设备中的偏移（以字节为单位） */
    __u8 valid;           /* 是否有效（1=有效，0=已删除） */
    __u8 reserved[351];   /* 填充到 512 字节 */
};

/* 内存中的文件节点结构 */
struct minifs_inode {
    struct inode vfs_inode;
    char *data;      /* 文件内容（内存缓存） */
    size_t size;     /* 文件大小 */
    loff_t offset;   /* 文件内容在块设备中的偏移 */
    int metadata_slot; /* 元数据槽索引 */
};

/* 内存中的文件元数据 */
struct minifs_file {
    char *name;      /* 文件名 */
    struct inode *inode; /* 关联的 inode */
    struct list_head list; /* 链表节点 */
};

/* 超级块结构 */
struct minifs_sb_info {
    struct super_block *sb;
    struct block_device *bdev; /* 块设备 */
    struct list_head files;   /* 动态文件列表 */
    spinlock_t lock;          /* 保护文件列表 */
    unsigned long next_ino;   /* 下一个 inode 编号 */
};

/* 转换为 minifs_inode */
static inline struct minifs_inode *MINIFS_INODE(struct inode *inode)
{
    return container_of(inode, struct minifs_inode, vfs_inode);
}

/* Inode 和超级块操作：前置声明 */
static struct dentry *minifs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
static int minifs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static int minifs_unlink(struct inode *dir, struct dentry *dentry);

static const struct inode_operations minifs_inode_ops = {
    .lookup = minifs_lookup,
    .create = minifs_create,
    .unlink = minifs_unlink,
};

static const struct super_operations minifs_sb_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_delete_inode,
};

/* 块设备读写辅助函数 */
static int minifs_read_block(struct block_device *bdev, sector_t sector, void *data, size_t size)
{
    struct bio *bio;
    struct page *page;
    int ret;

    if (!bdev) {
        printk(KERN_ERR "minifs: read_block: null block device\n");
        return -EINVAL;
    }

    bio = bio_alloc(GFP_KERNEL, 1);
    page = alloc_page(GFP_KERNEL);
    if (!bio || !page) {
        if (page)
            __free_page(page);
        if (bio)
            bio_put(bio);
        printk(KERN_ERR "minifs: read_block: allocation failed\n");
        return -ENOMEM;
    }

    bio->bi_iter.bi_sector = sector;
    bio_set_dev(bio, bdev);
    bio_add_page(bio, page, size, 0);
    ret = submit_bio_wait(bio);
    if (ret) {
        printk(KERN_ERR "minifs: read_block: submit_bio_wait failed, sector=%llu, ret=%d\n",
               (unsigned long long)sector, ret);
    } else {
        memcpy(data, page_address(page), size);
    }

    __free_page(page);
    bio_put(bio);
    return ret;
}

static int minifs_write_block(struct block_device *bdev, sector_t sector, const void *data, size_t size)
{
    struct bio *bio;
    struct page *page;
    int ret;

    if (!bdev) {
        printk(KERN_ERR "minifs: write_block: null block device\n");
        return -EINVAL;
    }

    if (sector * 512 >= 10 * 1024 * 1024) {
        printk(KERN_ERR "minifs: write_block: sector %llu exceeds image size\n",
               (unsigned long long)sector);
        return -ENOSPC;
    }

    bio = bio_alloc(GFP_KERNEL, 1);
    page = alloc_page(GFP_KERNEL);
    if (!bio || !page) {
        if (page)
            __free_page(page);
        if (bio)
            bio_put(bio);
        printk(KERN_ERR "minifs: write_block: allocation failed\n");
        return -ENOMEM;
    }

    memcpy(page_address(page), data, size);
    bio->bi_iter.bi_sector = sector;
    bio_set_dev(bio, bdev);
    bio->bi_opf = REQ_OP_WRITE | REQ_SYNC;
    bio_add_page(bio, page, size, 0);
    ret = submit_bio_wait(bio);
    if (ret) {
        printk(KERN_ERR "minifs: write_block: submit_bio_wait failed, sector=%llu, ret=%d\n",
               (unsigned long long)sector, ret);
    }

    __free_page(page);
    bio_put(bio);
    return ret;
}

/* 文件操作 */
static ssize_t minifs_file_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct minifs_inode *minifs_inode = MINIFS_INODE(inode);
    size_t len = minifs_inode->size;
    loff_t pos = *ppos;

    printk(KERN_INFO "minifs: file_read: %s, count=%zu, pos=%lld, slot=%d\n",
           file->f_path.dentry->d_name.name, count, pos, minifs_inode->metadata_slot);

    if (minifs_inode->metadata_slot < 0) {
        printk(KERN_ERR "minifs: file_read: invalid metadata slot for %s\n",
               file->f_path.dentry->d_name.name);
        return -EINVAL;
    }

    if (pos >= len)
        return 0; /* EOF */
    if (pos + count > len)
        count = len - pos;

    if (!minifs_inode->data) {
        printk(KERN_ERR "minifs: file_read: null data for %s\n",
               file->f_path.dentry->d_name.name);
        return -EIO;
    }

    if (copy_to_user(buf, minifs_inode->data + pos, count)) {
        printk(KERN_ERR "minifs: file_read: copy_to_user failed\n");
        return -EFAULT;
    }

    *ppos = pos + count;
    return count;
}

static ssize_t minifs_file_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct minifs_inode *minifs_inode = MINIFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct minifs_sb_info *sbi = sb->s_fs_info;
    struct minifs_file *file_entry;
    struct minifs_file_metadata *metadata;
    loff_t pos = *ppos;
    char *new_data;
    size_t new_size;
    int ret;
    bool file_exists = false;

    printk(KERN_INFO "minifs: file_write: %s, count=%zu, pos=%lld, slot=%d\n",
           file->f_path.dentry->d_name.name, count, pos, minifs_inode->metadata_slot);

    /* 检查文件是否在 sbi->files 中存在 */
    spin_lock(&sbi->lock);
    list_for_each_entry(file_entry, &sbi->files, list) {
        if (strcmp(file->f_path.dentry->d_name.name, file_entry->name) == 0) {
            file_exists = true;
            break;
        }
    }
    spin_unlock(&sbi->lock);

    if (!file_exists) {
        printk(KERN_ERR "minifs: file_write: file %s not found in sbi->files\n",
               file->f_path.dentry->d_name.name);
        return -ENOENT;
    }

    if (minifs_inode->metadata_slot < 0 || minifs_inode->metadata_slot >= MINIFS_MAX_FILES) {
        printk(KERN_ERR "minifs: file_write: invalid metadata slot %d for %s\n",
               minifs_inode->metadata_slot, file->f_path.dentry->d_name.name);
        return -EINVAL;
    }

    metadata = kmalloc(sizeof(struct minifs_file_metadata), GFP_KERNEL);
    if (!metadata) {
        printk(KERN_ERR "minifs: file_write: metadata allocation failed\n");
        return -ENOMEM;
    }

    new_size = pos + count > minifs_inode->size ? pos + count : minifs_inode->size;
    new_data = kmalloc(new_size, GFP_KERNEL);
    if (!new_data) {
        kfree(metadata);
        printk(KERN_ERR "minifs: file_write: data allocation failed\n");
        return -ENOMEM;
    }

    if (minifs_inode->data)
        memcpy(new_data, minifs_inode->data, minifs_inode->size);

    if (copy_from_user(new_data + pos, buf, count)) {
        kfree(new_data);
        kfree(metadata);
        printk(KERN_ERR "minifs: file_write: copy_from_user failed\n");
        return -EFAULT;
    }

    /* 写入块设备 */
    ret = minifs_write_block(sbi->bdev, minifs_inode->offset / MINIFS_BLOCK_SIZE,
                             new_data, new_size);
    if (ret) {
        kfree(new_data);
        kfree(metadata);
        printk(KERN_ERR "minifs: file_write: write_block failed, ret=%d\n", ret);
        return ret;
    }

    kfree(minifs_inode->data);
    minifs_inode->data = new_data;
    minifs_inode->size = new_size;
    inode->i_size = new_size;
    inode->i_mtime = inode->i_ctime = current_time(inode);

    /* 更新元数据 */
    memset(metadata, 0, sizeof(*metadata));
    metadata->ino = inode->i_ino;
    metadata->mode = inode->i_mode;
    metadata->size = new_size;
    metadata->offset = minifs_inode->offset;
    metadata->valid = 1;
    strncpy(metadata->name, file->f_path.dentry->d_name.name, 256);
    ret = minifs_write_block(sbi->bdev, MINIFS_METADATA_START + minifs_inode->metadata_slot,
                             metadata, sizeof(*metadata));
    if (ret) {
        printk(KERN_ERR "minifs: file_write: metadata write failed, slot=%d, ret=%d\n",
               minifs_inode->metadata_slot, ret);
        kfree(metadata);
        return ret;
    }
    kfree(metadata);

    *ppos = pos + count;
    sync_blockdev(sbi->bdev);
    return count;
}

static const struct file_operations minifs_file_ops = {
    .read = minifs_file_read,
    .write = minifs_file_write,
    .llseek = generic_file_llseek,
};

/* 目录操作 */
static int minifs_iterate(struct file *file, struct dir_context *ctx)
{
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct minifs_sb_info *sbi = sb->s_fs_info;
    struct minifs_file *file_entry;
    int pos = ctx->pos;
    int count = 0;

    printk(KERN_INFO "minifs: iterate: pos=%d\n", pos);

    /* 调试：打印文件列表 */
    spin_lock(&sbi->lock);
    list_for_each_entry(file_entry, &sbi->files, list) {
        printk(KERN_INFO "minifs: iterate: file %d: %s, ino=%lu, slot=%d\n",
               count++, file_entry->name, file_entry->inode->i_ino,
               MINIFS_INODE(file_entry->inode)->metadata_slot);
    }
    spin_unlock(&sbi->lock);

    if (pos == 0) {
        if (!dir_emit(ctx, ".", 1, inode->i_ino, DT_DIR)) {
            printk(KERN_ERR "minifs: iterate: dir_emit . failed\n");
            return -EIO;
        }
        ctx->pos = 1;
        return 0;
    }
    if (pos == 1) {
        if (!dir_emit(ctx, "..", 2, inode->i_ino, DT_DIR)) {
            printk(KERN_ERR "minifs: iterate: dir_emit .. failed\n");
            return -EIO;
        }
        ctx->pos = 2;
        return 0;
    }

    spin_lock(&sbi->lock);
    list_for_each_entry(file_entry, &sbi->files, list) {
        if (pos - 2 == 0) {
            if (!dir_emit(ctx, file_entry->name, strlen(file_entry->name),
                          file_entry->inode->i_ino, DT_REG)) {
                spin_unlock(&sbi->lock);
                printk(KERN_ERR "minifs: iterate: dir_emit %s failed\n", file_entry->name);
                return -EIO;
            }
            ctx->pos++;
            spin_unlock(&sbi->lock);
            printk(KERN_INFO "minifs: iterate: emitted %s\n", file_entry->name);
            return 0;
        }
        pos--;
    }
    spin_unlock(&sbi->lock);

    return 0;
}

static const struct file_operations minifs_dir_ops = {
    .iterate = minifs_iterate,
};

/* Inode 操作 */
static struct inode *minifs_get_inode(struct super_block *sb, int ino, umode_t mode)
{
    struct inode *inode = new_inode(sb);
    struct minifs_inode *minifs_inode;

    if (!inode) {
        printk(KERN_ERR "minifs: get_inode: inode allocation failed\n");
        return ERR_PTR(-ENOMEM);
    }

    minifs_inode = MINIFS_INODE(inode);
    inode->i_ino = ino;
    inode->i_mode = mode;
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    minifs_inode->metadata_slot = -1; /* 默认无效槽 */

    if (S_ISREG(mode)) {
        inode->i_fop = &minifs_file_ops;
        minifs_inode->data = NULL;
        minifs_inode->size = 0;
        inode->i_size = 0;
        minifs_inode->offset = 0;
    } else if (S_ISDIR(mode)) {
        inode->i_fop = &minifs_dir_ops;
        inode->i_op = &minifs_inode_ops;
        inode->i_size = 4096;
    }

    printk(KERN_INFO "minifs: get_inode: ino=%d, mode=%o\n", ino, mode);
    return inode;
}

/* 超级块操作 */
static int minifs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct minifs_sb_info *sbi;
    struct inode *root_inode;
    struct minifs_superblock *disk_sb;
    struct minifs_file_metadata *metadata;
    struct minifs_file *file_entry;
    int i, ret, loaded_files = 0;
    const char *dev_name = sb->s_bdev && sb->s_bdev->bd_disk ? sb->s_bdev->bd_disk->disk_name : "unknown";

    printk(KERN_INFO "minifs: fill_super: mounting on device %s\n", dev_name);

    sbi = kzalloc(sizeof(struct minifs_sb_info), GFP_KERNEL);
    if (!sbi) {
        printk(KERN_ERR "minifs: fill_super: sbi allocation failed\n");
        return -ENOMEM;
    }

    disk_sb = kmalloc(sizeof(struct minifs_superblock), GFP_KERNEL);
    metadata = kmalloc(sizeof(struct minifs_file_metadata), GFP_KERNEL);
    if (!disk_sb || !metadata) {
        kfree(disk_sb);
        kfree(metadata);
        kfree(sbi);
        printk(KERN_ERR "minifs: fill_super: disk_sb or metadata allocation failed\n");
        return -ENOMEM;
    }

    sb->s_fs_info = sbi;
    sbi->sb = sb;
    INIT_LIST_HEAD(&sbi->files);
    spin_lock_init(&sbi->lock);

    /* 获取块设备 */
    if (!sb->s_bdev || !sb->s_bdev->bd_disk) {
        printk(KERN_ERR "minifs: fill_super: invalid block device\n");
        kfree(disk_sb);
        kfree(metadata);
        kfree(sbi);
        return -EINVAL;
    }

    sbi->bdev = blkdev_get_by_dev(sb->s_bdev->bd_dev, FMODE_READ | FMODE_WRITE, THIS_MODULE);
    if (IS_ERR(sbi->bdev)) {
        ret = PTR_ERR(sbi->bdev);
        printk(KERN_ERR "minifs: fill_super: blkdev_get_by_dev failed, ret=%d\n", ret);
        kfree(disk_sb);
        kfree(metadata);
        kfree(sbi);
        return ret;
    }

    /* 读取超级块 */
    ret = minifs_read_block(sbi->bdev, 0, disk_sb, sizeof(*disk_sb));
    if (ret || disk_sb->magic != MINIFS_MAGIC) {
        printk(KERN_INFO "minifs: fill_super: initializing new superblock\n");
        memset(disk_sb, 0, sizeof(*disk_sb));
        disk_sb->magic = MINIFS_MAGIC;
        disk_sb->file_count = 0;
        disk_sb->next_ino = 2;
        ret = minifs_write_block(sbi->bdev, 0, disk_sb, sizeof(*disk_sb));
        if (ret) {
            printk(KERN_ERR "minifs: fill_super: superblock write failed, ret=%d\n", ret);
            blkdev_put(sbi->bdev, FMODE_READ | FMODE_WRITE);
            kfree(disk_sb);
            kfree(metadata);
            kfree(sbi);
            return ret;
        }
    }

    sbi->next_ino = disk_sb->next_ino;

    /* 读取文件元数据 */
    for (i = 0; i < MINIFS_MAX_FILES; i++) {
        memset(metadata, 0, sizeof(*metadata));
        ret = minifs_read_block(sbi->bdev, MINIFS_METADATA_START + i, metadata, sizeof(*metadata));
        if (ret) {
            printk(KERN_WARNING "minifs: fill_super: metadata read failed at slot %d, ret=%d\n", i, ret);
            continue;
        }
        if (!metadata->valid || metadata->name[0] == '\0') {
            printk(KERN_INFO "minifs: fill_super: slot %d invalid or empty\n", i);
            continue;
        }

        file_entry = kzalloc(sizeof(struct minifs_file), GFP_KERNEL);
        if (!file_entry) {
            printk(KERN_ERR "minifs: fill_super: file_entry allocation failed\n");
            continue;
        }

        file_entry->name = kstrdup(metadata->name, GFP_KERNEL);
        if (!file_entry->name) {
            kfree(file_entry);
            printk(KERN_ERR "minifs: fill_super: name allocation failed\n");
            continue;
        }

        file_entry->inode = minifs_get_inode(sb, metadata->ino, metadata->mode);
        if (IS_ERR(file_entry->inode)) {
            kfree(file_entry->name);
            kfree(file_entry);
            printk(KERN_ERR "minifs: fill_super: inode allocation failed\n");
            continue;
        }

        MINIFS_INODE(file_entry->inode)->data = kmalloc(metadata->size, GFP_KERNEL);
        if (!MINIFS_INODE(file_entry->inode)->data) {
            iput(file_entry->inode);
            kfree(file_entry->name);
            kfree(file_entry);
            printk(KERN_ERR "minifs: fill_super: data allocation failed\n");
            continue;
        }

        ret = minifs_read_block(sbi->bdev, metadata->offset / MINIFS_BLOCK_SIZE,
                                MINIFS_INODE(file_entry->inode)->data, metadata->size);
        if (ret) {
            kfree(MINIFS_INODE(file_entry->inode)->data);
            iput(file_entry->inode);
            kfree(file_entry->name);
            kfree(file_entry);
            printk(KERN_ERR "minifs: fill_super: data read failed, slot=%d, ret=%d\n", i, ret);
            continue;
        }

        MINIFS_INODE(file_entry->inode)->size = metadata->size;
        MINIFS_INODE(file_entry->inode)->offset = metadata->offset;
        MINIFS_INODE(file_entry->inode)->metadata_slot = i;
        file_entry->inode->i_size = metadata->size;

        spin_lock(&sbi->lock);
        list_add_tail(&file_entry->list, &sbi->files);
        spin_unlock(&sbi->lock);
        loaded_files++;
        printk(KERN_INFO "minifs: fill_super: loaded file %s, ino=%u, size=%llu, slot=%d\n",
               file_entry->name, metadata->ino, metadata->size, i);
    }

    /* 验证 file_count */
    if (loaded_files != disk_sb->file_count) {
        printk(KERN_WARNING "minifs: fill_super: file_count mismatch, disk=%u, loaded=%d\n",
               disk_sb->file_count, loaded_files);
        disk_sb->file_count = loaded_files;
        ret = minifs_write_block(sbi->bdev, 0, disk_sb, sizeof(*disk_sb));
        if (ret) {
            printk(KERN_ERR "minifs: fill_super: superblock update failed, ret=%d\n", ret);
        }
    }

    sb->s_magic = MINIFS_MAGIC;
    sb->s_blocksize = MINIFS_BLOCK_SIZE;
    sb->s_blocksize_bits = 12;
    sb->s_op = &minifs_sb_ops;

    root_inode = minifs_get_inode(sb, 1, S_IFDIR | 0755);
    if (IS_ERR(root_inode)) {
        ret = PTR_ERR(root_inode);
        printk(KERN_ERR "minifs: fill_super: root inode allocation failed, ret=%d\n", ret);
        blkdev_put(sbi->bdev, FMODE_READ | FMODE_WRITE);
        kfree(disk_sb);
        kfree(metadata);
        kfree(sbi);
        return ret;
    }

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        printk(KERN_ERR "minifs: fill_super: d_make_root failed\n");
        blkdev_put(sbi->bdev, FMODE_READ | FMODE_WRITE);
        kfree(disk_sb);
        kfree(metadata);
        kfree(sbi);
        return -ENOMEM;
    }

    sync_blockdev(sbi->bdev);
    kfree(disk_sb);
    kfree(metadata);
    printk(KERN_INFO "minifs: fill_super: mount successful, file_count=%d, next_ino=%lu\n",
           loaded_files, sbi->next_ino);
    return 0;
}

static struct dentry *minifs_mount(struct file_system_type *fs_type, int flags,
                                  const char *dev_name, void *data)
{
    printk(KERN_INFO "minifs: mounting filesystem on %s\n", dev_name);
    return mount_bdev(fs_type, flags, dev_name, data, minifs_fill_super);
}

static void minifs_kill_sb(struct super_block *sb)
{
    struct minifs_sb_info *sbi = sb->s_fs_info;
    struct minifs_file *file_entry, *tmp;

    printk(KERN_INFO "minifs: kill_sb: cleaning up\n");

    if (sbi) {
        spin_lock(&sbi->lock);
        list_for_each_entry_safe(file_entry, tmp, &sbi->files, list) {
            list_del(&file_entry->list);
            kfree(MINIFS_INODE(file_entry->inode)->data);
            iput(file_entry->inode);
            kfree(file_entry->name);
            kfree(file_entry);
        }
        spin_unlock(&sbi->lock);

        sync_blockdev(sbi->bdev);
        blkdev_put(sbi->bdev, FMODE_READ | FMODE_WRITE);
        kfree(sb->s_fs_info);
    }

    kill_block_super(sb);
    printk(KERN_INFO "minifs: kill_sb: cleanup complete\n");
}

/* 文件系统类型 */
static struct file_system_type minifs_type = {
    .owner = THIS_MODULE,
    .name = MINIFS_NAME,
    .mount = minifs_mount,
    .kill_sb = minifs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
};

/* Inode 操作 */
static struct dentry *minifs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    struct super_block *sb = dir->i_sb;
    struct minifs_sb_info *sbi = sb->s_fs_info;
    struct minifs_file *file_entry;
    struct inode *inode = NULL;

    printk(KERN_INFO "minifs: lookup: %s\n", dentry->d_name.name);

    spin_lock(&sbi->lock);
    list_for_each_entry(file_entry, &sbi->files, list) {
        if (strcmp(dentry->d_name.name, file_entry->name) == 0) {
            inode = file_entry->inode;
            ihold(inode);
            break;
        }
    }
    spin_unlock(&sbi->lock);

    d_add(dentry, inode);
    return NULL;
}

static int minifs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    struct super_block *sb = dir->i_sb;
    struct minifs_sb_info *sbi = sb->s_fs_info;
    struct minifs_file *file_entry;
    struct minifs_file_metadata *metadata;
    struct minifs_superblock *disk_sb;
    struct inode *inode;
    loff_t offset;
    int ret, slot = -1, i;

    printk(KERN_INFO "minifs: create: %s, mode=%o\n", dentry->d_name.name, mode);

    metadata = kmalloc(sizeof(struct minifs_file_metadata), GFP_KERNEL);
    disk_sb = kmalloc(sizeof(struct minifs_superblock), GFP_KERNEL);
    if (!metadata || !disk_sb) {
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: create: allocation failed\n");
        return -ENOMEM;
    }

    /* 检查文件是否已存在 */
    spin_lock(&sbi->lock);
    list_for_each_entry(file_entry, &sbi->files, list) {
        if (strcmp(dentry->d_name.name, file_entry->name) == 0) {
            spin_unlock(&sbi->lock);
            kfree(metadata);
            kfree(disk_sb);
            printk(KERN_ERR "minifs: create: file %s already exists\n", dentry->d_name.name);
            return -EEXIST;
        }
    }

    /* 寻找空闲元数据槽 */
    for (i = 0; i < MINIFS_MAX_FILES; i++) {
        memset(metadata, 0, sizeof(*metadata));
        ret = minifs_read_block(sbi->bdev, MINIFS_METADATA_START + i, metadata, sizeof(*metadata));
        if (ret || !metadata->valid) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        spin_unlock(&sbi->lock);
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: create: no free metadata slots\n");
        return -ENOSPC;
    }

    /* 计算文件内容偏移 */
    offset = MINIFS_CONTENT_START * MINIFS_BLOCK_SIZE;
    list_for_each_entry(file_entry, &sbi->files, list) {
        offset += MINIFS_INODE(file_entry->inode)->size;
    }

    /* 创建新 inode */
    inode = minifs_get_inode(sb, sbi->next_ino++, S_IFREG | mode);
    if (IS_ERR(inode)) {
        spin_unlock(&sbi->lock);
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: create: inode allocation failed\n");
        return PTR_ERR(inode);
    }

    /* 创建文件元数据 */
    file_entry = kzalloc(sizeof(struct minifs_file), GFP_KERNEL);
    if (!file_entry) {
        iput(inode);
        spin_unlock(&sbi->lock);
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: create: file_entry allocation failed\n");
        return -ENOMEM;
    }

    file_entry->name = kstrdup(dentry->d_name.name, GFP_KERNEL);
    if (!file_entry->name) {
        kfree(file_entry);
        iput(inode);
        spin_unlock(&sbi->lock);
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: create: name allocation failed\n");
        return -ENOMEM;
    }

    file_entry->inode = inode;
    ihold(inode);
    list_add_tail(&file_entry->list, &sbi->files);

    /* 更新磁盘元数据 */
    memset(metadata, 0, sizeof(*metadata));
    strncpy(metadata->name, file_entry->name, 256);
    metadata->ino = inode->i_ino;
    metadata->mode = mode;
    metadata->size = 0;
    metadata->offset = offset;
    metadata->valid = 1;
    ret = minifs_write_block(sbi->bdev, MINIFS_METADATA_START + slot, metadata, sizeof(*metadata));
    if (ret) {
        list_del(&file_entry->list);
        kfree(file_entry->name);
        kfree(file_entry);
        iput(inode);
        spin_unlock(&sbi->lock);
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: create: metadata write failed, slot=%d, ret=%d\n", slot, ret);
        return ret;
    }

    MINIFS_INODE(inode)->offset = offset;
    MINIFS_INODE(inode)->metadata_slot = slot;

    /* 更新超级块 */
    ret = minifs_read_block(sbi->bdev, 0, disk_sb, sizeof(*disk_sb));
    if (ret) {
        list_del(&file_entry->list);
        kfree(file_entry->name);
        kfree(file_entry);
        iput(inode);
        spin_unlock(&sbi->lock);
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: create: superblock read failed, ret=%d\n", ret);
        return ret;
    }
    disk_sb->file_count++;
    disk_sb->next_ino = sbi->next_ino;
    ret = minifs_write_block(sbi->bdev, 0, disk_sb, sizeof(*disk_sb));
    if (ret) {
        list_del(&file_entry->list);
        kfree(file_entry->name);
        kfree(file_entry);
        iput(inode);
        spin_unlock(&sbi->lock);
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: create: superblock write failed, ret=%d\n", ret);
        return ret;
    }

    spin_unlock(&sbi->lock);

    d_add(dentry, inode);
    dir->i_mtime = dir->i_ctime = current_time(dir);
    sync_blockdev(sbi->bdev);
    kfree(metadata);
    kfree(disk_sb);
    printk(KERN_INFO "minifs: create: file %s created, ino=%lu, slot=%d\n",
           dentry->d_name.name, inode->i_ino, slot);
    return 0;
}

static int minifs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct super_block *sb = dir->i_sb;
    struct minifs_sb_info *sbi = sb->s_fs_info;
    struct minifs_file *file_entry, *tmp;
    struct minifs_file_metadata *metadata;
    struct minifs_superblock *disk_sb;
    struct inode *inode = d_inode(dentry);
    int slot = -1, ret;

    printk(KERN_INFO "minifs: unlink: %s\n", dentry->d_name.name);

    metadata = kmalloc(sizeof(struct minifs_file_metadata), GFP_KERNEL);
    disk_sb = kmalloc(sizeof(struct minifs_superblock), GFP_KERNEL);
    if (!metadata || !disk_sb) {
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: unlink: allocation failed\n");
        return -ENOMEM;
    }

    if (!inode) {
        kfree(metadata);
        kfree(disk_sb);
        printk(KERN_ERR "minifs: unlink: no inode for %s\n", dentry->d_name.name);
        return -ENOENT;
    }

    spin_lock(&sbi->lock);
    list_for_each_entry_safe(file_entry, tmp, &sbi->files, list) {
        if (strcmp(dentry->d_name.name, file_entry->name) == 0) {
            slot = MINIFS_INODE(file_entry->inode)->metadata_slot;
            if (slot < 0 || slot >= MINIFS_MAX_FILES) {
                spin_unlock(&sbi->lock);
                kfree(metadata);
                kfree(disk_sb);
                printk(KERN_ERR "minifs: unlink: invalid metadata slot %d for %s\n",
                       slot, file_entry->name);
                return -EINVAL;
            }

            /* 标记元数据为无效 */
            memset(metadata, 0, sizeof(*metadata));
            metadata->valid = 0;
            ret = minifs_write_block(sbi->bdev, MINIFS_METADATA_START + slot, metadata, sizeof(*metadata));
            if (ret) {
                spin_unlock(&sbi->lock);
                kfree(metadata);
                kfree(disk_sb);
                printk(KERN_ERR "minifs: unlink: metadata write failed, slot=%d, ret=%d\n", slot, ret);
                return ret;
            }

            /* 更新超级块 */
            ret = minifs_read_block(sbi->bdev, 0, disk_sb, sizeof(*disk_sb));
            if (ret) {
                spin_unlock(&sbi->lock);
                kfree(metadata);
                kfree(disk_sb);
                printk(KERN_ERR "minifs: unlink: superblock read failed, ret=%d\n", ret);
                return ret;
            }
            if (disk_sb->file_count > 0)
                disk_sb->file_count--;
            ret = minifs_write_block(sbi->bdev, 0, disk_sb, sizeof(*disk_sb));
            if (ret) {
                spin_unlock(&sbi->lock);
                kfree(metadata);
                kfree(disk_sb);
                printk(KERN_ERR "minifs: unlink: superblock write failed, ret=%d\n", ret);
                return ret;
            }

            list_del(&file_entry->list);
            spin_unlock(&sbi->lock);

            kfree(MINIFS_INODE(file_entry->inode)->data);
            kfree(file_entry->name);
            kfree(file_entry);

            dir->i_mtime = dir->i_ctime = current_time(dir);
            d_drop(dentry);
            sync_blockdev(sbi->bdev);
            kfree(metadata);
            kfree(disk_sb);
            printk(KERN_INFO "minifs: unlink: file %s deleted, slot=%d\n", dentry->d_name.name, slot);
            return 0;
        }
    }
    spin_unlock(&sbi->lock);

    kfree(metadata);
    kfree(disk_sb);
    printk(KERN_ERR "minifs: unlink: file %s not found\n", dentry->d_name.name);
    return -ENOENT;
}

/* 模块初始化和清理 */
static int __init minifs_init(void)
{
    int ret = register_filesystem(&minifs_type);
    if (ret)
        printk(KERN_ERR "minifs: register_filesystem failed, ret=%d\n", ret);
    else
        printk(KERN_INFO "minifs: module loaded\n");
    return ret;
}

static void __exit minifs_exit(void)
{
    unregister_filesystem(&minifs_type);
    printk(KERN_INFO "minifs: module unloaded\n");
}

module_init(minifs_init);
module_exit(minifs_exit);

MODULE_AUTHOR("Grok");
MODULE_DESCRIPTION("Minimal Persistent File System for Linux 5.10");
MODULE_LICENSE("GPL");
```

---

## 测试步骤

1. **更新代码**：
   - 将 `minifs/v3/minifs.c` 替换为上述代码。

2. **确认 Makefile**：

   ```makefile
   obj-m += minifs.o

   all:
       make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

   clean:
       make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
   ```

3. **清理旧构建**：

   ```bash
   make clean
   ```

4. **编译模块**：

   ```bash
   make
   ```

5. **卸载旧模块和块设备**：

   ```bash
   sudo umount /mnt/minifs || true
   sudo losetup -d /dev/loop0 || true
   sudo rmmod minifs || true
   ```

6. **创建新镜像**（避免旧数据干扰）：

   ```bash
   sudo rm -f /tmp/minifs.img
   sudo dd if=/dev/zero of=/tmp/minifs.img bs=1M count=10
   ```

7. **加载模块和挂载**：

   ```bash
   sudo insmod minifs.ko
   sudo losetup /dev/loop0 /tmp/minifs.img
   sudo mkdir -p /mnt/minifs
   sudo mount -t minifs /dev/loop0 /mnt/minifs
   ```

8. **检查挂载**：

   ```bash
   mount | grep minifs
   ```

   **预期输出**：

   ```
   /dev/loop0 on /mnt/minifs type minifs (rw,relatime)
   ```

9. **测试增删改查和持久化**：
   - **查（Read）**：

     ```bash
     ls /mnt/minifs
     ```

     **预期输出**（新镜像为空）：

     ```
     ```

   - **增（Create）**：

     ```bash
     sudo touch /mnt/minifs/hello.txt
     echo "Hello, World!" | sudo tee /mnt/minifs/hello.txt
     ls /mnt/minifs
     cat /mnt/minifs/hello.txt
     ```

     **预期输出**：

     ```
     Hello, World!
     hello.txt
     Hello, World!
     ```

   - **改（Update）**：

     ```bash
     echo "Updated content" | sudo tee /mnt/minifs/hello.txt
     cat /mnt/minifs/hello.txt
     ```

     **预期输出**：

     ```
     Updated content
     Updated content
     ```

   - **删（Delete）**：

     ```bash
     sudo rm /mnt/minifs/hello.txt
     ls /mnt/minifs
     ```

     **预期输出**：

     ```
     ```

   - **验证持久化**：

     ```bash
     sudo touch /mnt/minifs/hello2.txt
     echo "Persistent content" | sudo tee /mnt/minifs/hello2.txt
     ls /mnt/minifs
     cat /mnt/minifs/hello2.txt
     sudo umount /mnt/minifs
     sudo mount -t minifs /dev/loop0 /mnt/minifs
     ls /mnt/minifs
     cat /mnt/minifs/hello2.txt
     ```

     **预期输出**：

     ```
     hello2.txt
     Persistent content
     hello2.txt
     Persistent content
     ```

10. **卸载和清理**：

    ```bash
    sudo umount /mnt/minifs
    sudo losetup -d /dev/loop0
    sudo rmmod minifs
    ```

11. **检查内核日志**：

    ```bash
    dmesg | tail -n 50
    ```

    - 确认是否有错误（如 `file_write: file not found` 或 `write_block failed`）。
    - 预期日志包含：

      ```
      minifs: create: file hello.txt created, ino=2, slot=0
      minifs: file_write: hello.txt, count=..., pos=0, slot=0
      minifs: fill_super: loaded file hello2.txt, ino=2, size=..., slot=0
      minifs: iterate: file 0: hello2.txt, ino=2, slot=0
      minifs: mount successful, file_count=1, next_ino=3
      ```

---

## 持久化说明

- 数据存储在 `/tmp/minifs.img`，在多次挂载间保持持久。
- 为避免重启清空 `/tmp`，可将镜像移至永久路径：

  ```bash
  sudo mv /tmp/minifs.img /root/minifs.img
  sudo losetup /dev/loop0 /root/minifs.img
  ```

---

## 调试建议

如果写入仍崩溃或 `ls` 报 I/O 错误：

1. **检查内核日志**：

   ```bash
   dmesg | tail -n 50
   ```

   - 查找 `minifs: file_write: file not found`、`invalid metadata slot` 或 `write_block: sector exceeds image size`。
   - 检查 `minifs: iterate: file ...` 日志，确认 `sbi->files` 是否包含 `hello.txt` 和 `hello2.txt`。

2. **验证文件存在**：

   ```bash
   ls /mnt/minifs
   cat /mnt/minifs/hello.txt
   cat /mnt/minifs/hello2.txt
   ```

   - 如果 `hello.txt` 不存在，说明 `minifs_create` 未被调用，需检查 `touch` 是否成功。

3. **检查块设备数据**：

   ```bash
   hexdump -C /tmp/minifs.img | head -n 50
   ```

   - 超级块（前 4096 字节）应包含 `magic=0x4D494E49`（字节 `49 4E 49 4D`）。
   - 元数据（从 4096 字节起，每 512 字节一个槽）应包含 `hello.txt` 和 `hello2.txt` 的文件名及 `valid=1`。

4. **添加更多日志**：
   如果问题未解决，可在 `minifs_file_write` 前添加 `inode` 状态检查：

   ```c
   printk(KERN_INFO "minifs: file_write: inode=%p, i_ino=%lu, mode=%o\n",
          inode, inode->i_ino, inode->i_mode);
   ```

5. **检查模块状态**：

   ```bash
   lsmod | grep minifs
   losetup -l
   ```

   **预期输出**：

   ```
   minifs                 24576  0
   NAME       SIZELIMIT OFFSET AUTOCLEAR RO BACK-FILE
   /dev/loop0         0      0         0  0 /tmp/minifs.img
   ```

---

## 限制和未来改进

- **限制**：
  - 最大文件数为 100（`MINIFS_MAX_FILES`）。
  - 块设备固定为 10MB。
  - 线性分配，无碎片整理。
  - 不支持子目录。
- **改进**：
  - 使用位图管理块分配。
  - 支持子目录（扩展 `minifs_inode`）。
  - 添加日志或校验机制。
  - 支持动态扩展块设备。

---

## 下一步

请按照上述步骤替换代码、编译并测试。重点验证：

- `sudo touch /mnt/minifs/hello.txt` 和 `echo "Hello, World!" | sudo tee /mnt/minifs/hello.txt` 是否成功。
- `ls /mnt/minifs` 是否无 I/O 错误并正确显示文件。
- 重新挂载后文件（如 `hello2.txt`）是否持久。

如果问题仍存在，请提供以下信息：

- `dmesg | tail -n 50` 的完整输出。
- `hexdump -C /tmp/minifs.img | head -n 50` 的输出。
- 具体错误命令和输出（如 `ls`、`cat` 或 `tee` 的结果）。
- 执行的确切命令序列（例如，是否运行了 `touch`）。

我将根据日志进一步优化代码，确保持久化存储和所有操作正常！
