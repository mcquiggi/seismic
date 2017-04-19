#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>  /* File Control Definitions          */
#include <termios.h>/* POSIX Terminal Control Definitions*/
#include <unistd.h> /* UNIX Standard Definitions         */
#include <errno.h>  /* ERROR Number Definitions          */
void main() {
	int fd;
	struct termios SerialPortSettings;
	int i=0;
	
	fd=open("/dev/ttyUSB1", O_RDONLY | O_NOCTTY | O_NDELAY);
	if (fd == -1)
		printf("\nError opening ttyUSB1\n");
	else {
		printf("\nttyUSB0 opened successfully\n");
		tcgetattr(fd, &SerialPortSettings);
		cfsetispeed(&SerialPortSettings,B9600);
		cfsetospeed(&SerialPortSettings,B9600);
		
		SerialPortSettings.c_cflag &= ~PARENB;   	// No Parity
		SerialPortSettings.c_cflag &= ~CSTOPB; 		// Stop bits = 1 
		SerialPortSettings.c_cflag &= ~CSIZE; 		// Clear the mask
		SerialPortSettings.c_cflag |=  CS8;   		// Data bits 8		
		SerialPortSettings.c_cflag &= ~CRTSCTS;
		SerialPortSettings.c_cflag |= CREAD | CLOCAL;
		SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);
		SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);
		tcsetattr(fd,TCSANOW,&SerialPortSettings);
		
		unsigned char read_buffer[32];                
		int  bytes_read = 0;                 
             
		printf("Reading 256 buffers...\n");
		while (i < 256) {
			bytes_read = read(fd, &read_buffer, 32);
			if (bytes_read == -1)
				; // perror("Bad read");
			else {
				printf("%02x ", read_buffer[0]);
				i++;
			}
		}
		printf("\n");
	}
	close(fd);
	printf("Done!\n");
	exit(0);
	}
