// sp.C
// Serial Port
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()
#include <ctype.h>

static int sp;
static struct termios tty;

// ----------------------------------------------------------------------------
int sp_open(char *sp_name)
{
  // Open the serial port. Change device path as needed (currently set to an 
  // standard FTDI USB-UART cable type device)
  sp = open(sp_name, O_RDWR); 
  if (sp < 0) {
    return(-1);
  }

  // Create new termios struc, we call it 'tty' for convention
  memset(&tty, 0, sizeof tty);

  // Read in existing settings, and handle any error
  if(tcgetattr(sp, &tty) != 0) {
    return(-2);
  }

  // Clear parity bit, disabling parity (most common)
  tty.c_cflag &= ~PARENB; 
  // Clear stop field, only one stop bit used in communication (most common)
  tty.c_cflag &= ~CSTOPB; 
  // 8 bits per byte (most common)
  tty.c_cflag |= CS8; 
  // Disable RTS/CTS hardware flow control (most common)
  tty.c_cflag &= ~CRTSCTS; 
  // Turn on READ & ignore ctrl lines (CLOCAL = 1)
  tty.c_cflag |= CREAD | CLOCAL; 

  tty.c_lflag &= ~ICANON;
  // Disable echo
  tty.c_lflag &= ~ECHO; 
  // Disable erasure
  tty.c_lflag &= ~ECHOE; 
  // Disable new-line echo
  tty.c_lflag &= ~ECHONL; 
  // Disable interpretation of INTR, QUIT and SUSP
  tty.c_lflag &= ~ISIG; 
  // Turn off s/w flow ctrl
  tty.c_iflag &= ~(IXON | IXOFF | IXANY); 
  // Disable any special handling of received bytes
  tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); 

  // Prevent special interpretation of output bytes (e.g. newline chars)
  tty.c_oflag &= ~OPOST; 
  // Prevent conversion of newline to carriage return/line feed
  tty.c_oflag &= ~ONLCR; 
  // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
  // tty.c_oflag &= ~OXTABS; 
  // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)
  // tty.c_oflag &= ~ONOEOT; 

  // Wait for up to 1s (10 deciseconds), returning as soon as any data is 
  // received.
  tty.c_cc[VTIME] = 0;    
  tty.c_cc[VMIN] = 1;

  // Set in/out baud rate to be 9600
  cfsetispeed(&tty, B9600);
  cfsetospeed(&tty, B9600);

  // Save tty settings, also checking for error
  if (tcsetattr(sp, TCSANOW, &tty) != 0) {
    return(-3);
  }
  return(0);
}

// ----------------------------------------------------------------------------
void sp_close(void)
{
  close(sp);
  sp=-1;
}

// ----------------------------------------------------------------------------
int sp_snd(u_int8_t *buf,int len)
{
  return(write(sp, buf, len));
}

// ----------------------------------------------------------------------------
int sp_rcv(u_int8_t *buf)
{
  return(read(sp, buf, 1));
}

// ----------------------------------------------------------------------------
int sp_rcvto(int to)
{
  tty.c_cc[VTIME] = to;    
  // Save tty settings, also checking for error
  if (tcsetattr(sp, TCSANOW, &tty) != 0) {
    return(-1);
  }
  return(0);
}
