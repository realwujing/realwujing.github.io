#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

static unsigned int variable;
static struct proc_dir_entry *test_dir, *test_entry;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int test_proc_read(char *buf, char **start, off_t off, int count,
		int *eof, void *data)
{
	unsigned int *ptr_var = data;
	return sprintf(buf, "test_rw = %u\n", *ptr_var);
}

static int test_proc_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	unsigned int *ptr_var = data;

	*ptr_var = simple_strtoul(buffer, NULL, 10);

	return count;
}
#else
static int test_proc_show(struct seq_file *seq, void *v)
{
	unsigned int *ptr_var = seq->private;
	seq_printf(seq, "%u\n", *ptr_var);
	return 0;
}

static ssize_t test_proc_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	struct seq_file *seq = file->private_data;
	unsigned int *ptr_var = seq->private;
	int err;
	char *kbuffer;

        if (!buffer || count > PAGE_SIZE - 1)
	                return -EINVAL;

	kbuffer = (char *)__get_free_page(GFP_KERNEL);
	if (!kbuffer)
		return -ENOMEM;

	err = -EFAULT;
	if (copy_from_user(kbuffer, buffer, count))
		goto out;
	kbuffer[count] = '\0';

	*ptr_var = simple_strtoul(kbuffer, NULL, 10);
	return count;

out:
	free_page((unsigned long)buffer);
	return err;
}

static int test_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, test_proc_show, PDE_DATA(inode));
}

static const struct file_operations test_proc_fops =
{
	.owner = THIS_MODULE,
	.open = test_proc_open,
	.read = seq_read,
	.write = test_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static __init int test_proc_init(void)
{
	test_dir = proc_mkdir("test_dir", NULL);
	if (test_dir) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
		test_entry = create_proc_entry("test_rw", 0666, test_dir);
		if (test_entry) {
			test_entry->nlink = 1;
			test_entry->data = &variable;
			test_entry->read_proc = test_proc_read;
			test_entry->write_proc = test_proc_write;
			return 0;
		}
#else
	test_entry = proc_create_data("test_rw",0666, test_dir, &test_proc_fops, &variable);
	if (test_entry)
		return 0;
#endif
	}

	return -ENOMEM;
}
module_init(test_proc_init);

static __exit void test_proc_cleanup(void)
{
	remove_proc_entry("test_rw", test_dir);
	remove_proc_entry("test_dir", NULL);
}
module_exit(test_proc_cleanup);

MODULE_AUTHOR("Barry Song <baohua@kernel.org>");
MODULE_DESCRIPTION("proc exmaple");
MODULE_LICENSE("GPL v2");
