#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#define MINIFS_NAME "minifs"
#define MINIFS_MAGIC 0x4D494E49 /* "MINI" in ASCII */

/* 文件节点结构 */
struct minifs_inode
{
	struct inode vfs_inode;
	char *data;	 /* 文件内容 */
	size_t size; /* 文件大小 */
};

/* 文件元数据（用于动态文件管理） */
struct minifs_file
{
	char *name;			   /* 文件名 */
	struct inode *inode;   /* 关联的 inode */
	struct list_head list; /* 链表节点 */
};

/* 超级块结构 */
struct minifs_sb_info
{
	struct super_block *sb;
	struct list_head files; /* 动态文件列表 */
	spinlock_t lock;		/* 保护文件列表 */
	unsigned long next_ino; /* 下一个 inode 编号 */
};

/* 转换为 minifs_inode */
static inline struct minifs_inode *MINIFS_INODE(struct inode *inode)
{
	return container_of(inode, struct minifs_inode, vfs_inode);
}

/* 文件操作 */
static ssize_t minifs_file_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct inode *inode = file_inode(file);
	struct minifs_inode *minifs_inode = MINIFS_INODE(inode);
	size_t len = minifs_inode->size;
	loff_t pos = *ppos;

	if (pos >= len)
		return 0; /* EOF */
	if (pos + count > len)
		count = len - pos;

	if (copy_to_user(buf, minifs_inode->data + pos, count))
		return -EFAULT;

	*ppos = pos + count;
	return count;
}

static ssize_t minifs_file_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct inode *inode = file_inode(file);
	struct minifs_inode *minifs_inode = MINIFS_INODE(inode);
	loff_t pos = *ppos;
	char *new_data;
	size_t new_size;

	new_size = pos + count > minifs_inode->size ? pos + count : minifs_inode->size;
	new_data = kmalloc(new_size, GFP_KERNEL);
	if (!new_data)
		return -ENOMEM;

	if (minifs_inode->data)
		memcpy(new_data, minifs_inode->data, minifs_inode->size);

	if (copy_from_user(new_data + pos, buf, count))
	{
		kfree(new_data);
		return -EFAULT;
	}

	kfree(minifs_inode->data);
	minifs_inode->data = new_data;
	minifs_inode->size = new_size;
	inode->i_size = new_size;
	inode->i_mtime = inode->i_ctime = current_time(inode);

	*ppos = pos + count;
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

	if (pos == 0)
	{
		dir_emit(ctx, ".", 1, inode->i_ino, DT_DIR);
		ctx->pos = 1;
		return 0;
	}
	if (pos == 1)
	{
		dir_emit(ctx, "..", 2, inode->i_ino, DT_DIR);
		ctx->pos = 2;
		return 0;
	}

	spin_lock(&sbi->lock);
	list_for_each_entry(file_entry, &sbi->files, list)
	{
		if (pos - 2 == 0)
		{
			dir_emit(ctx, file_entry->name, strlen(file_entry->name),
					 file_entry->inode->i_ino, DT_REG);
			ctx->pos++;
			spin_unlock(&sbi->lock);
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

/* Inode 操作：前置声明 */
static struct dentry *minifs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
static int minifs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static int minifs_unlink(struct inode *dir, struct dentry *dentry);

static const struct inode_operations minifs_inode_ops = {
	.lookup = minifs_lookup,
	.create = minifs_create,
	.unlink = minifs_unlink,
};

/* 超级块操作 */
static const struct super_operations minifs_sb_ops = {
	.statfs = simple_statfs,
	.drop_inode = generic_delete_inode,
};

/* Inode 操作 */
static struct inode *minifs_get_inode(struct super_block *sb, int ino, umode_t mode)
{
	struct inode *inode = new_inode(sb);
	struct minifs_inode *minifs_inode;

	if (!inode)
		return ERR_PTR(-ENOMEM);

	minifs_inode = MINIFS_INODE(inode);
	inode->i_ino = ino;
	inode->i_mode = mode;
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

	if (S_ISREG(mode))
	{
		inode->i_fop = &minifs_file_ops;
		minifs_inode->data = NULL;
		minifs_inode->size = 0;
		inode->i_size = 0;
	}
	else if (S_ISDIR(mode))
	{
		inode->i_fop = &minifs_dir_ops;
		inode->i_op = &minifs_inode_ops;
		inode->i_size = 4096;
	}

	return inode;
}

/* 超级块操作 */
static int minifs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct minifs_sb_info *sbi;
	struct inode *root_inode;
	struct minifs_file *file_entry;

	sbi = kzalloc(sizeof(struct minifs_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;

	sb->s_fs_info = sbi;
	sbi->sb = sb;
	sbi->next_ino = 2; /* 1 留给根目录 */
	INIT_LIST_HEAD(&sbi->files);
	spin_lock_init(&sbi->lock);

	sb->s_magic = MINIFS_MAGIC;
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;
	sb->s_op = &minifs_sb_ops;

	root_inode = minifs_get_inode(sb, 1, S_IFDIR | 0755);
	if (IS_ERR(root_inode))
	{
		kfree(sbi);
		return PTR_ERR(root_inode);
	}

	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root)
	{
		kfree(sbi);
		return -ENOMEM;
	}

	/* 初始化两个默认文件 */
	file_entry = kzalloc(sizeof(struct minifs_file), GFP_KERNEL);
	if (!file_entry)
		return -ENOMEM;
	file_entry->name = kstrdup("hello.txt", GFP_KERNEL);
	file_entry->inode = minifs_get_inode(sb, sbi->next_ino++, S_IFREG | 0644);
	if (IS_ERR(file_entry->inode))
	{
		kfree(file_entry->name);
		kfree(file_entry);
		return PTR_ERR(file_entry->inode);
	}
	MINIFS_INODE(file_entry->inode)->data = kstrdup("Hello, World!\n", GFP_KERNEL);
	MINIFS_INODE(file_entry->inode)->size = strlen("Hello, World!\n");
	file_entry->inode->i_size = MINIFS_INODE(file_entry->inode)->size;
	spin_lock(&sbi->lock);
	list_add(&file_entry->list, &sbi->files);
	spin_unlock(&sbi->lock);

	file_entry = kzalloc(sizeof(struct minifs_file), GFP_KERNEL);
	if (!file_entry)
		return -ENOMEM;
	file_entry->name = kstrdup("version.txt", GFP_KERNEL);
	file_entry->inode = minifs_get_inode(sb, sbi->next_ino++, S_IFREG | 0644);
	if (IS_ERR(file_entry->inode))
	{
		kfree(file_entry->name);
		kfree(file_entry);
		return PTR_ERR(file_entry->inode);
	}
	MINIFS_INODE(file_entry->inode)->data = kstrdup("MinFS v1.0\n", GFP_KERNEL);
	MINIFS_INODE(file_entry->inode)->size = strlen("MinFS v1.0\n");
	file_entry->inode->i_size = MINIFS_INODE(file_entry->inode)->size;
	spin_lock(&sbi->lock);
	list_add(&file_entry->list, &sbi->files);
	spin_unlock(&sbi->lock);

	return 0;
}

static struct dentry *minifs_mount(struct file_system_type *fs_type, int flags,
								   const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, minifs_fill_super);
}

static void minifs_kill_sb(struct super_block *sb)
{
	struct minifs_sb_info *sbi = sb->s_fs_info;
	struct minifs_file *file_entry, *tmp;

	spin_lock(&sbi->lock);
	list_for_each_entry_safe(file_entry, tmp, &sbi->files, list)
	{
		list_del(&file_entry->list);
		kfree(MINIFS_INODE(file_entry->inode)->data);
		iput(file_entry->inode);
		kfree(file_entry->name);
		kfree(file_entry);
	}
	spin_unlock(&sbi->lock);

	kfree(sb->s_fs_info);
	kill_block_super(sb);
}

/* 文件系统类型 */
static struct file_system_type minifs_type = {
	.owner = THIS_MODULE,
	.name = MINIFS_NAME,
	.mount = minifs_mount,
	.kill_sb = minifs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
};

/* Inode 操作：查找文件 */
static struct dentry *minifs_lookup(struct inode *dir, struct dentry *dentry,
									unsigned int flags)
{
	struct super_block *sb = dir->i_sb;
	struct minifs_sb_info *sbi = sb->s_fs_info;
	struct minifs_file *file_entry;
	struct inode *inode = NULL;

	spin_lock(&sbi->lock);
	list_for_each_entry(file_entry, &sbi->files, list)
	{
		if (strcmp(dentry->d_name.name, file_entry->name) == 0)
		{
			inode = file_entry->inode;
			ihold(inode); /* 增加引用计数 */
			break;
		}
	}
	spin_unlock(&sbi->lock);

	d_add(dentry, inode);
	return NULL;
}

/* 创建文件 */
static int minifs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	struct super_block *sb = dir->i_sb;
	struct minifs_sb_info *sbi = sb->s_fs_info;
	struct minifs_file *file_entry;
	struct inode *inode;

	/* 检查文件是否已存在 */
	spin_lock(&sbi->lock);
	list_for_each_entry(file_entry, &sbi->files, list)
	{
		if (strcmp(dentry->d_name.name, file_entry->name) == 0)
		{
			spin_unlock(&sbi->lock);
			return -EEXIST;
		}
	}

	/* 创建新 inode */
	inode = minifs_get_inode(sb, sbi->next_ino++, S_IFREG | mode);
	if (IS_ERR(inode))
	{
		spin_unlock(&sbi->lock);
		return PTR_ERR(inode);
	}

	/* 创建文件元数据 */
	file_entry = kzalloc(sizeof(struct minifs_file), GFP_KERNEL);
	if (!file_entry)
	{
		iput(inode);
		spin_unlock(&sbi->lock);
		return -ENOMEM;
	}

	file_entry->name = kstrdup(dentry->d_name.name, GFP_KERNEL);
	if (!file_entry->name)
	{
		kfree(file_entry);
		iput(inode);
		spin_unlock(&sbi->lock);
		return -ENOMEM;
	}

	file_entry->inode = inode;
	ihold(inode); /* 增加 inode 引用计数 */
	list_add(&file_entry->list, &sbi->files);
	spin_unlock(&sbi->lock);

	d_add(dentry, inode);
	dir->i_mtime = dir->i_ctime = current_time(dir);
	return 0;
}

/* 删除文件 */
static int minifs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct minifs_sb_info *sbi = sb->s_fs_info;
	struct minifs_file *file_entry, *tmp;
	struct inode *inode = d_inode(dentry);

	if (!inode)
	{
		printk(KERN_ERR "minifs: unlink: no inode for %s\n", dentry->d_name.name);
		return -ENOENT;
	}

	spin_lock(&sbi->lock);
	list_for_each_entry_safe(file_entry, tmp, &sbi->files, list)
	{
		if (strcmp(dentry->d_name.name, file_entry->name) == 0)
		{
			list_del(&file_entry->list);
			spin_unlock(&sbi->lock);

			/* 释放资源 */
			if (MINIFS_INODE(file_entry->inode)->data)
				kfree(MINIFS_INODE(file_entry->inode)->data);
			kfree(file_entry->name);
			kfree(file_entry);

			/* 更新目录时间 */
			dir->i_mtime = dir->i_ctime = current_time(dir);

			/* 释放 inode（由 VFS 管理） */
			d_drop(dentry); /* 从缓存中移除 dentry */
			return 0;
		}
	}
	spin_unlock(&sbi->lock);

	printk(KERN_ERR "minifs: unlink: file %s not found\n", dentry->d_name.name);
	return -ENOENT;
}

/* 模块初始化和清理 */
static int __init minifs_init(void)
{
	return register_filesystem(&minifs_type);
}

static void __exit minifs_exit(void)
{
	unregister_filesystem(&minifs_type);
}

module_init(minifs_init);
module_exit(minifs_exit);

MODULE_AUTHOR("Grok");
MODULE_DESCRIPTION("Minimal File System with CRUD for Linux 5.10");
MODULE_LICENSE("GPL");