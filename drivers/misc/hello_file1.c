#include <linux/init.h>
#include <linux/module.h>

extern int hello_file2_int;
extern void hello_file2_fun(void);

static int hello_file1_init(void)
{
	printk("Hello, world [This is file 1] [hello_file2_int = %d]\n",
	       hello_file2_int);

	/* Call hello_file2_fun*/
	hello_file2_int = 100;
	hello_file2_fun();

	return 0;
}

static void hello_file1_exit(void)
{
	printk("Goodbye, Hello world [This is file 1]\n");
}

module_init(hello_file1_init);
module_exit(hello_file1_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Terence");
MODULE_DESCRIPTION("This is hello file1 module");
MODULE_VERSION("V1");
