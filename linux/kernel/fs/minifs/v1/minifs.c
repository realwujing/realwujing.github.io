#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/string.h>

#define MINIFS_NAME "minifs"
#define MINIFS_MAGIC 0x4D494E49 /* "MINI" in ASCII */

/* 静态文件内容 */
static const char *hello_content = "Hello, World!\n";
static const char *version_content = "MinFS v1.0\n";

/* 文件节点结构 */
struct minifs_inode
{
	struct inode vfs_inode;
	const char *data; /* 文件内容（仅用于文件） */
	size_t size;	  /* 文件大小 */
};

/* 超级块结构 */
struct minifs_sb_info
{
	struct super_block *sb;
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

static const struct file_operations minifs_file_ops = {
	.read = minifs_file_read,
	.llseek = generic_file_llseek,
};

/* 目录操作 */
static int minifs_iterate(struct file *file, struct dir_context *ctx)
{
	if (ctx->pos >= 4) /* 2 个特殊目录（. 和 ..）+ 2 个文件 */
		return 0;

	switch (ctx->pos)
	{
	case 0:
		dir_emit(ctx, ".", 1, file_inode(file)->i_ino, DT_DIR);
		break;
	case 1:
		dir_emit(ctx, "..", 2, file_inode(file)->i_ino, DT_DIR);
		break;
	case 2:
		dir_emit(ctx, "hello.txt", 9, 2, DT_REG);
		break;
	case 3:
		dir_emit(ctx, "version.txt", 11, 3, DT_REG);
		break;
	}

	ctx->pos++;
	return 0;
}

static const struct file_operations minifs_dir_ops = {
	.iterate = minifs_iterate,
};

/* Inode 操作：查找文件（前置声明） */
static struct dentry *minifs_lookup(struct inode *dir, struct dentry *dentry,
									unsigned int flags);

static const struct inode_operations minifs_inode_ops = {
	.lookup = minifs_lookup,
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
		if (ino == 2)
		{ /* hello.txt */
			minifs_inode->data = hello_content;
			minifs_inode->size = strlen(hello_content);
			inode->i_size = minifs_inode->size;
		}
		else if (ino == 3)
		{ /* version.txt */
			minifs_inode->data = version_content;
			minifs_inode->size = strlen(version_content);
			inode->i_size = minifs_inode->size;
		}
	}
	else if (S_ISDIR(mode))
	{
		inode->i_fop = &minifs_dir_ops;
		inode->i_op = &minifs_inode_ops; /* 关联 inode 操作 */
		inode->i_size = 4096;			 /* 目录大小 */
	}

	return inode;
}

/* 超级块操作 */
static int minifs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct minifs_sb_info *sbi;
	struct inode *root_inode;

	sbi = kzalloc(sizeof(struct minifs_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;

	sb->s_fs_info = sbi;
	sbi->sb = sb;
	sb->s_magic = MINIFS_MAGIC;
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;

	root_inode = minifs_get_inode(sb, 1, S_IFDIR | 0555);
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

	return 0;
}

static struct dentry *minifs_mount(struct file_system_type *fs_type, int flags,
								   const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, minifs_fill_super);
}

static void minifs_kill_sb(struct super_block *sb)
{
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
	struct inode *inode = NULL;

	if (strcmp(dentry->d_name.name, "hello.txt") == 0)
	{
		inode = minifs_get_inode(dir->i_sb, 2, S_IFREG | 0444);
	}
	else if (strcmp(dentry->d_name.name, "version.txt") == 0)
	{
		inode = minifs_get_inode(dir->i_sb, 3, S_IFREG | 0444);
	}

	if (IS_ERR(inode))
		return ERR_CAST(inode);

	d_add(dentry, inode);
	return NULL; /* Success */
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
MODULE_DESCRIPTION("Minimal File System for Linux 5.10");
MODULE_LICENSE("GPL");