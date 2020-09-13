#include <linux/init.h>
#include <linux/module.h>

int hello_file2_int = 0;
EXPORT_SYMBOL(hello_file2_int);

void hello_file2_fun(void)
{
	printk("\n[This is hello_file2_fun] [hello_file2_int = %d]\n",
	       hello_file2_int);
}
EXPORT_SYMBOL(hello_file2_fun);

static int hello_file2_init(void)
{
	printk("Hello, world [This is file 2]\n");
	return 0;
}

static void hello_file2_exit(void)
{
	printk("Goodbye, Hello world [This is file 2]\n");
}

module_init(hello_file2_init);
module_exit(hello_file2_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Terence");
MODULE_DESCRIPTION("This is hello file2 module");
MODULE_VERSION("V1");
