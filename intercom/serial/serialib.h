#ifndef SERIALIB_H
#define SERIALIB_H


#include <sys/time.h>                                   // Used for TimeOut operations

#include <stdlib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <termios.h>
#include <string.h>
#include <iostream>
#include <fcntl.h>                                  // File control definitions


class serialib
{
public:
    serialib    ();                                                 // Constructor
    ~serialib   ();                                                 // Destructor

    // ::: Configuration and initialization :::

    char    Open        (const char *Device,const unsigned int Bauds);      // Open a device
    void    Close();                                                        // Close the device

    char    Write       (const void *Buffer, const unsigned int NbBytes); // Write an array of bytes
    int     Read        (void *Buffer,unsigned int MaxNbBytes,const unsigned int TimeOut_ms=NULL);
    int     RawRead     (void *Buffer,unsigned int MaxNbBytes);
    void	clearDTR();
    void	setDTR();

    int		WaitForInput(const unsigned int TimeOut_ms);
    bool	IsSerialAlive();

private:
#ifdef __linux__
    int             fd;
#endif

};



/*!  \class     TimeOut
     \brief     This class can manage a timer which is used as a timeout.
   */
// Class TimeOut
class TimeOut
{
public:
    TimeOut();                                                      // Constructor
    void                InitTimer();                                // Init the timer
    unsigned long int   ElapsedTime_ms();                           // Return the elapsed time since initialization
private:
    struct timeval      PreviousTime;
};



/*!
  \mainpage serialib class

  \brief
       \htmlonly
       <TABLE>
       <TR><TD>
            <a href="../serialibv1.0.zip" title="Download the serialib class">
                <TABLE>
                <TR><TD><IMG SRC="download.png" BORDER=0 WIDTH=100> </TD></TR>
                <TR><TD><P ALIGN="center">[Download]</P> </TD></TR>
                </TABLE>
            </A>
            </TD>
            <TD>
                <script type="text/javascript"><!--google_ad_client = "ca-pub-0665655683291467";
                google_ad_slot = "0230365165";
                google_ad_width = 728;
                google_ad_height = 90;
                //-->
                </script>
                <script type="text/javascript"
                src="http://pagead2.googlesyndication.com/pagead/show_ads.js">
                </script>
            </TD>
        </TR>
        </TABLE>

        \endhtmlonly

    The class serialib offers simple access to the serial port devices for windows and linux. It can be used for any serial device (Built-in serial port, USB to RS232 converter, arduino board or any hardware using or emulating a serial port)
    \image html serialib.png
    The class can be used under Windows and Linux.
    The class allows basic operations like :
    - opening and closing connection
    - reading data (characters, array of bytes or strings)
    - writing data (characters, array of bytes or strings)
    - non-blocking functions (based on timeout).


  \author   Philippe Lucidarme <serialib@googlegroups.com>
  \con
  \date     1th may 2011
  \version  1.1
*/




#endif // SERIALIB_H

