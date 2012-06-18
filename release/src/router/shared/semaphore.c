/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <shared.h>
#include "semaphore_mfp.h"
 
semaphore_t Semaphore;
semaphore_t spinlock[SPINLOCK_SPIN_MAX];

int semaphore_create()
{
	int flag_sem = O_CREAT|O_RDWR;
	int init_value = 1;

	semaphore_unlink(SEM_NAME);
	
	if (semaphore_open(SEM_NAME, flag_sem, FILE_MODE, init_value) == -1)
	{
		perror("semaphore_create fail");
		return -1;
	}

	return 0;
}

int semaphore_close()
{
	if (Semaphore.sem_magic != SEM_MAGIC)
	{
		errno = EINVAL;
		return -1;
	}

	Semaphore.sem_magic = 0;	// in case caller tries to use it later
	if (close(Semaphore.sem_fd[0]) == -1 || close(Semaphore.sem_fd[1]) == -1)
	{
		memset(&Semaphore, '\0', sizeof(semaphore_t));
		return -1;
	}

	memset(&Semaphore, '\0', sizeof(semaphore_t));
	return 0;
}

int semaphore_open(const char *pathname, int oflag, ... )
{
	int	i, flags, save_errno;
	char	c;
	mode_t	mode;
	va_list	ap;
	unsigned int	value = 0;
																	       
	if (oflag & O_CREAT)
	{
		va_start(ap, oflag);			// init ap to final named argument
		mode = va_arg(ap, mode_t);
		value = va_arg(ap, unsigned int);
		va_end(ap);

		if (mkfifo(pathname, mode) < 0)
		{
			if (errno == EEXIST && (oflag & O_EXCL) == 0)
				oflag &= ~O_CREAT;      // already exists, OK 
			else
			{
				perror("Sen_open: mkfifo fail");
				return -1;
			}
		}
	}

	Semaphore.sem_fd[0] = Semaphore.sem_fd[1] = -1;

	if ( (Semaphore.sem_fd[0] = open(pathname, O_RDONLY | O_NONBLOCK)) < 0)
		goto error;
	if ( (Semaphore.sem_fd[1] = open(pathname, O_WRONLY | O_NONBLOCK)) < 0)
		goto error;
	if ( (flags = fcntl(Semaphore.sem_fd[0], F_GETFL, 0)) < 0)
		goto error;
	flags &= ~O_NONBLOCK;
	if (fcntl(Semaphore.sem_fd[0], F_SETFL, flags) < 0)
		goto error;
	if (oflag & O_CREAT)	// initialize semaphore
	{
		for (i = 0; i < value; i++)
			if (write(Semaphore.sem_fd[1], &c, 1) != 1)
				goto error;
	}

	Semaphore.sem_magic = SEM_MAGIC;
	return 0;

error:
	save_errno = errno;
	if (oflag & O_CREAT)
		unlink(pathname);		// if we created FIFO
	close(Semaphore.sem_fd[0]);		// ignore error
	close(Semaphore.sem_fd[1]);		// ignore error

	memset(&Semaphore, '\0',sizeof(semaphore_t));
	errno = save_errno;
	perror("semaphore_open error");

	return -1;
}

int semaphore_unlink(const char *pathname)
{
	return unlink(pathname);
}

int semaphore_post()
{
	char	c;

	if (Semaphore.sem_magic != SEM_MAGIC)
	{
		errno = EINVAL;
		return -1;
	}
																	       
	if (write(Semaphore.sem_fd[1], &c, 1) == 1)
		return 0;

	return -1;
}

int semaphore_wait()
{
	char	c;

	if (Semaphore.sem_magic != SEM_MAGIC)
	{
		errno = EINVAL;
		return -1;
	}

	if (read(Semaphore.sem_fd[0], &c, 1) == 1)
		return 0;

	return -1;
}

int spinlock_open(int idx, const char *pathname, int oflag, ... )
{
	int	i, flags, save_errno;
	char	c;
	mode_t	mode;
	va_list	ap;
	unsigned int	value = 0;

	if (oflag & O_CREAT)
	{
		va_start(ap, oflag);			// init ap to final named argument
		mode = va_arg(ap, mode_t);
		value = va_arg(ap, unsigned int);
		va_end(ap);

		if (mkfifo(pathname, mode) < 0)
		{
			if (errno == EEXIST && (oflag & O_EXCL) == 0)
				oflag &= ~O_CREAT;      // already exists, OK 
			else
			{
				perror("Sen_open: mkfifo fail");
				return -1;
			}
		}
	}

	spinlock[idx].sem_fd[0] = spinlock[idx].sem_fd[1] = -1;

	if ( (spinlock[idx].sem_fd[0] = open(pathname, O_RDONLY | O_NONBLOCK)) < 0)
		goto error;
	if ( (spinlock[idx].sem_fd[1] = open(pathname, O_WRONLY | O_NONBLOCK)) < 0)
		goto error;
	if ( (flags = fcntl(spinlock[idx].sem_fd[0], F_GETFL, 0)) < 0)
		goto error;
	flags &= ~O_NONBLOCK;
	if (fcntl(spinlock[idx].sem_fd[0], F_SETFL, flags) < 0)
		goto error;

	if (oflag & O_CREAT)	// initialize semaphore
	{
		for (i = 0; i < value; i++)
			if (write(spinlock[idx].sem_fd[1], &c, 1) != 1)
				goto error;
	}

	spinlock[idx].sem_magic = SEM_MAGIC;
	return 0;

error:
	save_errno = errno;
	if (oflag & O_CREAT)
		unlink(pathname);		// if we created FIFO
	close(spinlock[idx].sem_fd[0]);		// ignore error
	close(spinlock[idx].sem_fd[1]);		// ignore error

	memset(&spinlock[idx], '\0',sizeof(semaphore_t));
	errno = save_errno;
	perror("semaphore_open error");

	return -1;
}

int spinlock_init(int idx)
{
	int flag_sem = O_CREAT|O_RDWR;
	int init_value = 1;
	char sem_name[32];

	if (idx < 0 || idx >= SPINLOCK_SPIN_MAX)
		return -1;

	sprintf(sem_name, "%s%d", SEM_NAME, idx);
	semaphore_unlink(sem_name);
	
	if (spinlock_open(idx, sem_name, flag_sem, FILE_MODE, init_value) == -1)
	{
		perror("semaphore_create fail");
		return -1;
	}

	return 0;
}

int spinlock_destroy(int idx)
{
	if (idx < 0 || idx >= SPINLOCK_SPIN_MAX)
		return -1;

	if (spinlock[idx].sem_magic != SEM_MAGIC)
	{
		errno = EINVAL;
		return -1;
	}

	spinlock[idx].sem_magic = 0;	// in case caller tries to use it later
	if (close(spinlock[idx].sem_fd[0]) == -1 || close(spinlock[idx].sem_fd[1]) == -1)
	{
		memset(&spinlock[idx], '\0', sizeof(semaphore_t));
		return -1;
	}

	memset(&spinlock[idx], '\0', sizeof(semaphore_t));
	return 0;
}

void init_spinlock(void)
{
	int i;

	for(i=0;i<SPINLOCK_SPIN_MAX;i++)
		spinlock_init(i);
}

int spinlock_unlock(int idx)
{
	char	c;

	if (idx < 0 || idx >= SPINLOCK_SPIN_MAX)
		return -1;

	if (spinlock[idx].sem_magic != SEM_MAGIC)
	{
		errno = EINVAL;
		return -1;
	}
																	       
	if (write(spinlock[idx].sem_fd[1], &c, 1) == 1)
		return 0;

	return -1;
}

int spinlock_lock(int idx)
{
	char	c;

	if (idx < 0 || idx >= SPINLOCK_SPIN_MAX)
		return -1;

	if (spinlock[idx].sem_magic != SEM_MAGIC)
	{
		errno = EINVAL;
		return -1;
	}

	if (read(spinlock[idx].sem_fd[0], &c, 1) == 1)
		return 0;
	
	return -1;
}
