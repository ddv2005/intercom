#include "serialib.h"
#include <unistd.h>
#include <sys/ioctl.h>


/*!
 \brief      Constructor of the class serialib.
 */
// Class constructor
serialib::serialib() {
	fd = -1;
}
/*!
 \brief      Destructor of the class serialib. It close the connection
 */
// Class desctructor
serialib::~serialib() {
	Close();
}

/*!
 \brief Open the serial port
 \param Device : Port name (COM1, COM2, ... for Windows ) or (/dev/ttyS0, /dev/ttyACM0, /dev/ttyUSB0 ... for linux)
 \param Bauds : Baud rate of the serial port.

 \n Supported baud rate for Windows :
 - 110
 - 300
 - 600
 - 1200
 - 2400
 - 4800
 - 9600
 - 14400
 - 19200
 - 38400
 - 56000
 - 57600
 - 115200
 - 128000
 - 256000

 \n Supported baud rate for Linux :\n
 - 110
 - 300
 - 600
 - 1200
 - 2400
 - 4800
 - 9600
 - 19200
 - 38400
 - 57600
 - 115200

 \return 1 success
 \return -1 device not found
 \return -2 error while opening the device
 \return -3 error while getting port parameters
 \return -4 Speed (Bauds) not recognized
 \return -5 error while writing port parameters
 \return -6 error while writing timeout parameters
 */
char serialib::Open(const char *Device, const unsigned int Bauds) {
	struct termios options; // Structure with the device's options


	// Open device
	fd = open(Device, O_RDWR | O_NOCTTY | O_NDELAY); // Open port
	if (fd == -1)
		return -2; // If the device is not open, return -1
	fcntl(fd, F_SETFL, FNDELAY); // Open the device in nonblocking mode

	// Set parameters
	//tcgetattr(fd, &options); // Get the current options of the port
	bzero(&options, sizeof(options)); // Clear all the options
	speed_t Speed;
	switch (Bauds) // Set the speed (Bauds)
	{
	case 110:
		Speed = B110;
		break;
	case 300:
		Speed = B300;
		break;
	case 600:
		Speed = B600;
		break;
	case 1200:
		Speed = B1200;
		break;
	case 2400:
		Speed = B2400;
		break;
	case 4800:
		Speed = B4800;
		break;
	case 9600:
		Speed = B9600;
		break;
	case 19200:
		Speed = B19200;
		break;
	case 38400:
		Speed = B38400;
		break;
	case 57600:
		Speed = B57600;
		break;
	case 115200:
		Speed = B115200;
		break;
	default:
		return -4;
	}
	options.c_cflag |= (CLOCAL | CREAD | CS8 ); // Configure the device : 8 bits, no parity, no control
	options.c_lflag = 0;
	options.c_iflag = 0;
	options.c_oflag = 0;
//	options.c_iflag |= (IGNPAR | IGNBRK);
	options.c_cc[VINTR] = 0; /* Ctrl-c */
	options.c_cc[VQUIT] = 0; /* Ctrl-\ */
	options.c_cc[VERASE] = 0; /* del */
	options.c_cc[VKILL] = 0; /* @ */
	options.c_cc[VEOF] = 0; /* Ctrl-d */
	options.c_cc[VTIME] = 0; /* inter-character timer unused */
	options.c_cc[VMIN] = 0; /* blocking read until 1 character arrives */
# ifdef VSWTC
	options.c_cc[VSWTC] = 0;
# endif
# ifdef VSWTCH
	options.c_cc[VSWTCH] = 0;
# endif
	options.c_cc[VSTART] = 0; /* Ctrl-q */
	options.c_cc[VSTOP] = 0; /* Ctrl-s */
	options.c_cc[VSUSP] = 0; /* Ctrl-z */
	options.c_cc[VEOL] = 0; /* '\0' */
	options.c_cc[VREPRINT] = 0; /* Ctrl-r */
	options.c_cc[VDISCARD] = 0; /* Ctrl-u */
	options.c_cc[VWERASE] = 0; /* Ctrl-w */
	options.c_cc[VLNEXT] = 0; /* Ctrl-v */
	options.c_cc[VEOL2] = 0; /* '\0' */
	tcflush(fd, TCIFLUSH);
	cfsetispeed(&options, B115200);
	cfsetospeed(&options, B115200);
	tcsetattr(fd, TCSANOW, &options); // Activate the settings

	cfsetispeed(&options, Speed);
	cfsetospeed(&options, Speed);
	tcsetattr(fd, TCSANOW, &options); // Activate the settings
	tcflush(fd, TCIFLUSH);
	return (1); // Success
}

/*!
 \brief Close the connection with the current device
 */
void serialib::Close() {
	if(fd>=0)
	{
		close(fd);
		fd = -1;
	}
}

void	serialib::clearDTR()
{
	if(fd>=0)
	{
		timespec ts;
		int status = 0;
	    ts.tv_sec =0;
	    ts.tv_nsec = 300000; // 0.3 Microseconds = 300
	    ioctl(fd, TIOCMGET, &status);
        status &= ~TIOCM_DTR;
	    ioctl(fd, TIOCMSET, &status);
	    nanosleep (&ts, NULL);
	}
}

void	serialib::setDTR()
{
	if(fd>=0)
	{
		timespec ts;
		int status = 0;
	    ts.tv_sec =0;
	    ts.tv_nsec = 300000; // 0.3 Microseconds = 300
	    ioctl(fd, TIOCMGET, &status);
        status |= TIOCM_DTR;
	    ioctl(fd, TIOCMSET, &status);
	    nanosleep (&ts, NULL);
	}
}

int serialib::WaitForInput(const unsigned int TimeOut_ms) {
	fd_set read_fdset;
	struct timeval wait;

	FD_ZERO(&read_fdset);
	FD_SET(fd,&read_fdset);
	wait.tv_sec = TimeOut_ms / 1000;
	wait.tv_usec = (TimeOut_ms % 1000) * 1000;

	int n = select(FD_SETSIZE, &read_fdset, NULL, NULL, &wait);
	return n;
}

/*!
 \brief Write an array of data on the current serial port
 \param Buffer : array of bytes to send on the port
 \param NbBytes : number of byte to send
 \return 1 success
 \return -1 error while writting data
 */
char serialib::Write(const void *Buffer, const unsigned int NbBytes) {
	if (write(fd, Buffer, NbBytes) != (ssize_t) NbBytes) // Write data
		return -1; // Error while writing
	return 1; // Write operation successfull
}

int serialib::RawRead(void *Buffer, unsigned int MaxNbBytes) {
	return read(fd, (void*) Buffer, MaxNbBytes); // Try to read a byte on the device
}

bool serialib::IsSerialAlive()
{
	if(fd>=0)
	{
		int tmp;
		return (ioctl(fd, TIOCMGET, &tmp) >=0);
	}
	else
		return false;
}

/*!
 \brief Read an array of bytes from the serial device (with timeout)
 \param Buffer : array of bytes read from the serial device
 \param MaxNbBytes : maximum allowed number of bytes read
 \param TimeOut_ms : delay of timeout before giving up the reading
 \return 1 success, return the number of bytes read
 \return 0 Timeout reached
 \return -1 error while setting the Timeout
 \return -2 error while reading the byte
 */
int serialib::Read(void *Buffer, unsigned int MaxNbBytes,
		unsigned int TimeOut_ms) {
	TimeOut Timer; // Timer used for timeout
	Timer.InitTimer(); // Initialise the timer
	unsigned int NbByteRead = 0;
	do {
		WaitForInput(10);
		unsigned char* Ptr = (unsigned char*) Buffer + NbByteRead; // Compute the position of the current byte
		int Ret = RawRead((void*) Ptr, MaxNbBytes - NbByteRead); // Try to read a byte on the device
		if (Ret == -1)
			return -2; // Error while reading
		if (Ret > 0) { // One or several byte(s) has been read on the device
			NbByteRead += Ret; // Increase the number of read bytes
			if (NbByteRead >= MaxNbBytes) // Success : bytes has been read
				return 1;
		}
	} while (Timer.ElapsedTime_ms() < TimeOut_ms || TimeOut_ms == 0);
	return 0; // Timeout reached, return 0
}

// ******************************************
//  Class TimeOut
// ******************************************


/*!
 \brief      Constructor of the class TimeOut.
 */
// Constructor
TimeOut::TimeOut() {
}

/*!
 \brief      Initialise the timer. It writes the current time of the day in the structure PreviousTime.
 */
//Initialize the timer
void TimeOut::InitTimer() {
	gettimeofday(&PreviousTime, NULL);
}

/*!
 \brief      Returns the time elapsed since initialization.  It write the current time of the day in the structure CurrentTime.
 Then it returns the difference between CurrentTime and PreviousTime.
 \return     The number of microseconds elapsed since the functions InitTimer was called.
 */
//Return the elapsed time since initialization
unsigned long int TimeOut::ElapsedTime_ms() {
	struct timeval CurrentTime;
	int sec, usec;
	gettimeofday(&CurrentTime, NULL); // Get current time
	sec = CurrentTime.tv_sec - PreviousTime.tv_sec; // Compute the number of second elapsed since last call
	usec = CurrentTime.tv_usec - PreviousTime.tv_usec; // Compute
	if (usec < 0) { // If the previous usec is higher than the current one
		usec = 1000000 - PreviousTime.tv_usec + CurrentTime.tv_usec; // Recompute the microseonds
		sec--; // Substract one second
	}
	return sec * 1000 + usec / 1000;
}

