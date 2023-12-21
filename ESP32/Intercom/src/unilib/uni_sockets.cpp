#include "uni_sockets.h"
#include "uni_log.h"
#include <stdlib.h>
#include <string.h>

#ifdef __POSIX
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#define HAS_INET_PTON
#endif
#ifdef __WINDOWS
#define NETDB_SUCCESS 0
#ifdef WINDOWS_VISTA_PLUS
#define HAS_INET_PTON
#else
#undef HAS_INET_PTON
#endif
#endif


uni_socket_address::uni_socket_address()
{
	clear();
}

uni_socket_address::uni_socket_address(const char *address, int af)
{
	set(address, af);
}

uni_socket_address::uni_socket_address(const in_addr_t addr) :
		m_version(4)
{
	v.m_four.s_addr = addr;
}

uni_socket_address::uni_socket_address(const struct in_addr & addr):m_version(4)
{
	v.m_four = addr;
}

uni_socket_address::uni_socket_address(const struct in6_addr & addr):m_version(6)
{
	v.m_six = addr;
}

std::string uni_socket_address::asString()
{
	size_t size=0;
	char * result = (char*)malloc(NI_MAXHOST);
	uni_sockaddr sa(*this,0);
	if(!getnameinfo(sa, sa.getSize(), result, NI_MAXHOST, NULL, 0, NI_NUMERICHOST))
	{
		size = strlen(result);
	}
	result[size]=0;
	std::string sresult = std::string(result);
	free(result);
	return sresult;
}


void uni_socket_address::clear()
{
	memset(&v, 0, sizeof(v));
	m_version = 0;
}

int uni_socket_address::set(const char *address, int af)
{
	switch (af)
	{
		case AF_INET:
			m_version = 4;
			break;
		case AF_INET6:
			m_version = 6;
			break;
		default:
			memset(&v, 0, sizeof(v));
			return URC_ERROR;
	}
#ifdef HAS_INET_PTON
	if (inet_pton(af, address, &v) != 1)
	{
		clear();
		return URC_ERROR;
	}
#else
	if(m_version==4)
		v.m_four.S_un.S_addr = inet_addr(address);
	else
		URC_ERROR;
#endif
	return URC_OK;
}

int uni_socket_address::set(const int ai_family, const int ai_addrlen,
		struct sockaddr *ai_addr)
{
	switch (ai_family)
	{
		case AF_INET6:
			if (ai_addrlen < (int) sizeof(sockaddr_in6))
			{
				break;
			}

			m_version = 6;
			v.m_six = ((struct sockaddr_in6 *) ai_addr)->sin6_addr;
			return URC_OK;

		case AF_INET:
			if (ai_addrlen < (int) sizeof(sockaddr_in))
			{
				break;
			}
			m_version = 4;
			v.m_four = ((struct sockaddr_in *) ai_addr)->sin_addr;
			return URC_OK;
	}

	clear();
	return URC_ERROR;
}

int uni_socket_address::resolve(const char *hostname)
{
	int result = URC_ERROR;
	struct addrinfo *res = NULL;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_INET;
    hints.ai_flags = AI_CANONNAME;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
	int localErrNo = getaddrinfo((const char *) hostname, NULL, &hints, &res);
	if (localErrNo == NETDB_SUCCESS)
	{
		if (res->ai_addr != NULL)
		{
			result = set(res->ai_family, res->ai_addrlen, res->ai_addr);
		}
	}
	if (res != NULL)
		freeaddrinfo(res);
	if(result!=URC_OK)
		clear();
	return result;
}

uni_socket_address::operator in_addr() const
{
  return v.m_four;
}


uni_socket_address::operator in6_addr() const
{
  return v.m_six;
}


uni_sockaddr::uni_sockaddr() :
		ptr(&m_storage)
{
	memset(&m_storage, 0, sizeof(m_storage));
}

uni_sockaddr::uni_sockaddr(const uni_socket_address & ip, uint16_t port) :
		ptr(&m_storage)
{
	memset(&m_storage, 0, sizeof(m_storage));
	if (ip.getVersion() == 6)
	{
		addr6->sin6_family = AF_INET6;
		addr6->sin6_addr = ip;
		addr6->sin6_port = htons(port);
		addr6->sin6_flowinfo = 0;
		addr6->sin6_scope_id = 0;
	}
	else
	{
		addr4->sin_family = AF_INET;
		addr4->sin_addr = ip;
		addr4->sin_port = htons(port);
	}
}

socklen_t uni_sockaddr::getSize() const
{
  switch (addr->sa_family) {
    case AF_INET :
      return sizeof(sockaddr_in);
    case AF_INET6 :
      // RFC 2133 (Old IPv6 spec) size is 24
      // RFC 2553 (New IPv6 spec) size is 28
      return sizeof(sockaddr_in6);
    default :
      return sizeof(m_storage);
  }
}

uni_socket_address uni_sockaddr::getIP() const
{
  switch (addr->sa_family) {
    case AF_INET :
      return addr4->sin_addr;
    case AF_INET6 :
      return addr6->sin6_addr;
    default :
      return (in_addr_t)0;
  }
}

uint16_t uni_sockaddr::getPort() const
{
  switch (addr->sa_family) {
    case AF_INET :
      return ntohs(addr4->sin_port);
    case AF_INET6 :
      return ntohs(addr6->sin6_port);
    default :
      return 0;
  }
}
#ifdef __POSIX
int setNonBlocking(int fd)
{
  if (fd < 0)
    return -1;

  // Set non-blocking so we can use select calls to break I/O block on close
  int cmd = 1;
  if (::ioctl(fd, FIONBIO, &cmd) == 0 && ::fcntl(fd, F_SETFD, 1) == 0)
    return fd;

  ::close(fd);
  return -1;
}
#endif

//===================================================
uni_socket::uni_socket(int fd):m_fd(fd)
{

}

uni_socket::~uni_socket()
{
	close();
}

void uni_socket::close()
{
	uni_locker lock(m_lock);
	if(m_fd)
	{
		shutdown(2);
#ifdef __POSIX
		::close(m_fd);
#endif
#ifdef __WINDOWS
		::closesocket(m_fd);
#endif
		m_fd = 0;
	}
}

bool uni_socket::shutdown(int how)
{
	return !::shutdown(m_fd,how);
}

bool uni_socket::setOption(int option, int value, int level)
{
  return !::setsockopt(m_fd, level, option,
                                     (char *)&value, sizeof(value));

}

bool uni_socket::setOption(int option, const void * valuePtr, size_t valueSize, int level)
{
  return !::setsockopt(m_fd, level, option,
                                     (char *)valuePtr, (socklen_t)valueSize);
}

bool uni_socket::getPeerAddress(uni_sockaddr & addr)
{
	socklen_t size = addr.getSize();
	return !::getpeername(m_fd, addr, &size);
}

bool uni_socket::getPeerAddress(uni_socket_address & addr, uint16_t & portNum)
{
  uni_sockaddr sa;
  if(getPeerAddress(sa))
  {
  	addr = sa.getIP();
  	portNum = sa.getPort();
  	return true;
  }
  else
  	return false;
}

bool uni_socket::listen(uint16_t port,unsigned queueSize,bool reuse)
{
	return listen(INADDR_ANY,port,queueSize,reuse);

}

bool uni_socket::listen(const uni_socket_address & bindAddr,uint16_t port,unsigned queueSize,bool reuse)
{
	if(openSocket(AF_INET,false))
	{
		uni_sockaddr bind_sa(bindAddr,port);
		if(bind_sa->sa_family != AF_INET6)
		{
		 // attempt to listen
		 if (!setOption(SO_REUSEADDR, reuse? 1 : 0)) {
			 close();
			 return false;
		 }
		}
		if(!::bind(m_fd, bind_sa, bind_sa.getSize()))
			return !::listen(m_fd,queueSize);
	}
	return false;
}

bool uni_socket::bind(uint16_t port,bool reuse)
{
	return bind(INADDR_ANY,port,reuse);
}

bool uni_socket::bind(const uni_socket_address & bindAddr,uint16_t port,bool reuse)
{
	if(openSocket(AF_INET,false))
	{
		uni_sockaddr bind_sa(bindAddr,port);
		if(bind_sa->sa_family != AF_INET6)
		{
		 // attempt to listen
		 if (!setOption(SO_REUSEADDR, reuse? 1 : 0)) {
			 close();
			 return false;
		 }
		}
		return !::bind(m_fd, bind_sa, bind_sa.getSize());
	}
	return false;
}

uint16_t uni_socket::getPort()
{
	if(m_fd)
	{
    uni_sockaddr sa;
    socklen_t size = sa.getSize();
    if (!::getsockname(m_fd, sa, &size))
    	return sa.getPort();
	}
	return 0;
}


//=======================================

uni_tcp_socket::uni_tcp_socket():uni_socket(0)
{
}

uni_tcp_socket::uni_tcp_socket(int fd):uni_socket(fd)
{

}

uni_tcp_socket::~uni_tcp_socket()
{

}

bool uni_tcp_socket::openSocket(int ipAdressFamily,bool force)
{
	uni_locker lock(m_lock);
	if(!force&&m_fd)
		return true;
	if(m_fd)
		close();
	m_fd = ::socket(ipAdressFamily,SOCK_STREAM, 0);
	if(m_fd<0)
	{
		m_fd = 0;
		return false;
	}
	return true;
}

bool uni_tcp_socket::accept(uni_tcp_socket &listener)
{
  uni_sockaddr sa;
  socklen_t size = sa.getSize();

  int new_fd;
  while ((new_fd = ::accept(listener.getHandle(), sa, &size)) < 0) {
     return false;
  }
  return (m_fd = new_fd)!=0;
}

bool uni_tcp_socket::connect(const uni_socket_address & addr,uint16_t port)
{
	uni_sockaddr ca(addr,port);
    openSocket(addr.getVersion()==4?AF_INET:AF_INET6);
	return ::connect(m_fd,ca,ca.getSize())==0;
}

//===================================================
uni_udp_socket::uni_udp_socket():uni_socket(0)
{
}

uni_udp_socket::uni_udp_socket(int fd):uni_socket(fd)
{

}

uni_udp_socket::~uni_udp_socket()
{

}

bool uni_udp_socket::openSocket(int ipAdressFamily,bool force)
{
	uni_locker lock(m_lock);
	if(!force&&m_fd)
		return true;
	if(m_fd)
		close();
	m_fd = ::socket(ipAdressFamily,SOCK_DGRAM, IPPROTO_UDP);
	if(m_fd<0)
	{
		m_fd = 0;
		return false;
	}
	return true;
}
