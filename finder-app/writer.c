#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		syslog(LOG_ERR, "Please give proper arguments\n");
		return 1;
	}

	int fd = open (argv[1], O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
	ssize_t nr;
	if(fd < 0)
	{
		int err_number = errno;
		syslog(LOG_ERR, "Fail to open: %s\n", strerror(err_number));
		return -1;
	}
	syslog(LOG_DEBUG, "Writing %s to %s\n", argv[2], argv[1]);
	nr = write (fd, argv[2], strlen (argv[2]));
	if (nr < 0)
	{
		int err_number = errno;
		syslog(LOG_ERR, "Fail to write: %s\n", strerror(err_number));
	}
	return 0;
}
