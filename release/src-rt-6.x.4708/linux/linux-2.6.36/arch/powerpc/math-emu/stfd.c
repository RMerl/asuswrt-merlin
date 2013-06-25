#include <linux/types.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

int
stfd(void *frS, void *ea)
{

	if (copy_to_user(ea, frS, sizeof(double)))
		return -EFAULT;

	return 0;
}
