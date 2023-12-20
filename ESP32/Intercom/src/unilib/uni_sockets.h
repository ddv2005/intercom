#ifndef UNI_SOCKETS_H_
#define UNI_SOCKETS_H_

#include "uni_common.h"
#include "uni_lock.h"
#include "uni_mutex.h"
#include <string>
#ifdef __POSIX
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef ESP32
#if !defined(NI_NUMERICHOST)
#define NI_NUMERICHOST 0x02
#endif
#if !defined(NETDB_SUCCESS)
#define NETDB_SUCCESS 0
#endif
#else
#include <netinet/ip.h>
#endif

#define DATA_TYPE void
#endif
#ifdef __WINDOWS
#include <ws2tcpip.h>
#include <winsock.h>
#include <In6addr.h>
#pragma comment(lib, "ws2_32.lib")
typedef u_long in_addr_t;
typedef int socklen_t;
#ifndef __GNUC__
typedef SSIZE_T ssize_t;
#endif
#define SHUT_RDWR SD_BOTH
#define DATA_TYPE	char
#define S_ADDR S_un.S_addr
#else
#define S_ADDR s_addr
#endif



class uni_socket_address
{
protected:
  union {
    in_addr m_four;
    in6_addr m_six;
  } v;
  uint8_t m_version;
public:
  uni_socket_address();
  uni_socket_address(const char *address,int af=AF_INET);
  uni_socket_address(const in_addr_t addr);
  uni_socket_address(const struct in_addr & addr);
  uni_socket_address(const struct in6_addr & addr);
  uni_socket_address(const uni_socket_address &source):m_version(source.m_version)
  {
	  v = source.v;
  }

  bool operator==(const uni_socket_address&other)
  {
	  return (m_version==other.m_version) && (m_version==4?(v.m_four.S_ADDR==other.v.m_four.S_ADDR):!memcmp(&v.m_six,&other.v.m_six,sizeof(in6_addr))); 
  }
  uint8_t getVersion() const { return m_version; }
  int set(const char *address,int af=AF_INET);
  int set(const int ai_family, const int ai_addrlen, struct sockaddr *ai_addr);
  int resolve(const char *hostname);
  bool isValid() const { return m_version != 0; }
  bool isAny() const { return !isValid()||((m_version==4)&&(v.m_four.S_ADDR==0)); }
  void clear();
  std::string asString();

  operator in_addr() const;
  operator in6_addr() const;
 };

class uni_sockaddr
{
  public:
	uni_sockaddr();
	uni_sockaddr(const uni_sockaddr&source):ptr(&m_storage)
	{
		m_storage=source.m_storage;
	}

	uni_sockaddr(const uni_socket_address & ip, uint16_t port);
    sockaddr* operator->() const { return addr; }
    operator sockaddr*()   const { return addr; }
	bool operator==(const uni_sockaddr&other) const
	{
		return (getPort()==other.getPort())&&(getIP()==other.getIP());
	}

    uni_sockaddr& operator=(uni_sockaddr other)
    {
        ptr = &m_storage;
        m_storage = other.m_storage;
        return *this;
    }

    socklen_t getSize() const;
    uni_socket_address getIP() const;
    uint16_t getPort() const;
  private:
    sockaddr_storage m_storage;
    union {
      sockaddr_storage * ptr;
      sockaddr         * addr;
      sockaddr_in      * addr4;
      sockaddr_in6     * addr6;
    };
};

class uni_socket
{
protected:
	int	m_fd;
	uni_simple_mutex m_lock;
	virtual bool openSocket(int ipAdressFamily,bool force=true)=0;
public:
	uni_socket(int fd);
	virtual ~uni_socket();
	void close();
	bool shutdown(int how=SHUT_RDWR);
	bool setOption(int option, int value, int level= SOL_SOCKET);
	bool setOption(int option, const void * valuePtr, size_t valueSize, int level= SOL_SOCKET);
	int  getHandle() { return m_fd; }
	bool getPeerAddress(uni_sockaddr & addr);
	bool getPeerAddress(uni_socket_address & addr, uint16_t & portNum);
	ssize_t send(const void *buf, size_t len, int flags=0) { return ::send(m_fd,(const DATA_TYPE*) buf,len,flags); }
	ssize_t recv(void *buf, size_t len, int flags=0) { return ::recv(m_fd,(DATA_TYPE*) buf,len,flags); }
	bool write(const void *buf, ssize_t len) { return send(buf,len)==len; }
	bool isOpen() { return m_fd>0; }
	bool listen(uint16_t port,unsigned queueSize=0,bool reuse=true);
	bool listen(const uni_socket_address & bindAddr,uint16_t port,unsigned queueSize=0,bool reuse=true);
	bool bind(uint16_t port,bool reuse=true);
	bool bind(const uni_socket_address & bindAddr,uint16_t port,bool reuse=true);
	uint16_t getPort();
};

class uni_tcp_socket:public uni_socket
{
protected:
	virtual bool openSocket(int ipAdressFamily,bool force=true);
public:
	uni_tcp_socket();
	uni_tcp_socket(int fd);
	virtual ~uni_tcp_socket();
	bool accept(uni_tcp_socket &listener);
	bool connect(const uni_socket_address & addr,uint16_t port);
};

class uni_udp_socket:public uni_socket
{
protected:
	virtual bool openSocket(int ipAdressFamily,bool force=true);
public:
	uni_udp_socket();
	uni_udp_socket(int fd);
	virtual ~uni_udp_socket();
	ssize_t recvfrom(void *buf, size_t len, int flags,uni_sockaddr & addr)
	{ 
		socklen_t size = addr.getSize();
		return ::recvfrom(m_fd,(DATA_TYPE*) buf,len,flags,addr,&size);
	}
	ssize_t sendto(const void *buf, size_t len, int flags,const uni_sockaddr & addr)
	{ 
		return ::sendto(m_fd,(DATA_TYPE*) buf,len,flags,addr,addr.getSize());
	}

};


#endif /* UNI_SOCKETS_H_ */
