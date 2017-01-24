#include "MyHeader.h"
#include "apue.h"
#include <sys/socket.h>

/* размер буфера с управляющей информацией для приема/передачи одного дескриптора */
#define CONTROLLEN CMSG_LEN(sizeof(int))
static struct cmsghdr *cmptr = NULL; /* размещается при первом вызове */
/*
 * Передает дескриптор файла другому процессу.
 * Если fd<0, то в качестве кода ошибки, отправляется -fd.
 */
int send_fd(int fd, int fd_to_send)
{
	struct iovec iov[1];
	struct msghdr msg;
	char buf[2]; /* 2-байтный протокол send_fd()/recv_fd() */
	iov[0].iov_base = buf;
	iov[0].iov_len = 2;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	if (fd_to_send < 0) 
	{
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		buf[1] = -fd_to_send; /* ненулевое значение означает ошибку */
		if (buf[1] == 0)
			buf[1] = 1; /* протокол преобразует в -256 */
	} 
	else 
	{
		if (cmptr == NULL && (cmptr = (struct cmsghdr *)malloc(CONTROLLEN)) == NULL)
			return(-1);
		cmptr->cmsg_level = SOL_SOCKET;
		cmptr->cmsg_type = SCM_RIGHTS;
		cmptr->cmsg_len = CONTROLLEN;
		msg.msg_control = cmptr;
		msg.msg_controllen = CONTROLLEN;
		*(int *)CMSG_DATA(cmptr) = fd_to_send; /* записать дескриптор */
		buf[1] = 0; /* нулевое значение означает отсутствие ошибки */
	}
	buf[0] = 0; /* нулевой байт – флаг для recv_fd() */
	if (sendmsg(fd, &msg, 0) != 2)
		return(-1);
	return(0);
}


/*
 * Принимает дескриптор файла от серверного процесса. Кроме того, любые
 * принятые данные передаются функции (*userfunc)(STDERR_FILENO, buf, nbytes).
 * Чтобы принять дескриптор, мы должны соблюдать 2-байтный протокол.
 */
int recv_fd(int fd)
{
	int newfd, nr, status;
	char *ptr;
	char buf[MAXLINE];
	status = -1;
	
    struct msghdr   msg;
    struct iovec    iov;
    union {
        struct cmsghdr  cmsghdr;
        char        control[CMSG_SPACE(sizeof (int))];
    } cmsgu;
    struct cmsghdr  *cmsg;

	
	
	for ( ; ; ) 
	{
		iov.iov_base = buf;
		iov.iov_len = 2;
		
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		if (cmptr == NULL && (cmptr = (struct cmsghdr *)malloc(CONTROLLEN)) == NULL)
			return(-1);

		msg.msg_control = cmptr;
		msg.msg_controllen = CONTROLLEN;
	    nr = recvmsg (fd, &msg, 0);
		if (nr < 0)
		{
			err_sys("ошибка вызова функции recvmsg");
		}
		else if (nr == 0) 
		{
			err_ret("соединение закрыто сервером");
			return(-1);
		}
		/*
		 * Проверить, являются ли два последних байта нулевым байтом
		 * и кодом ошибки. Нулевой байт должен быть предпоследним,
		 * а код ошибки - последним байтом в буфере.
		 * Нулевой код ошибки означает, что мы должны принять дескриптор.
		 */
		for (ptr = buf; ptr < &buf[nr]; ) 
		{
			if (*ptr++ == 0) 
			{
				if (ptr != &buf[nr-1])
					err_dump("нарушение формата сообщения");
				status = *ptr & 0xFF; /* предотвратить расширение знакового бита */
				if (status == 0) 
				{
					if (msg.msg_controllen != CONTROLLEN)
						err_dump("получен код 0, но отсутствует fd");
					newfd = *(int *)CMSG_DATA(cmptr);
				} else 
				{
					newfd = -status;
				}
				nr -= 2;
			}
		}
		if (nr > 0 && write(STDERR_FILENO, buf, nr) != nr)
			return(-1);
		if (status >= 0)	/* final data has arrived */
			return(newfd);	/* descriptor, or -status */		
	}
}


int set_nonblock(int fd)
{
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

