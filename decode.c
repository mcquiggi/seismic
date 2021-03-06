/************************************************************************************
 * 	decode.c
 *
 *	Decode the data stream from the USGS seismometer in our back yard!
 * 	
 * 	by Kevin McQuiggin, mcquiggi@sfu.ca
 * 	
 * 	Last modified: 2016.10.16
 *
 ************************************************************************************/
 

// Includes:
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <memory.h>
#include <sys/resource.h>

#include <pmmintrin.h>
#include <emmintrin.h>
#include <xmmintrin.h>
#include <mmintrin.h>

#include <fcntl.h>  
#include <termios.h>


// Local includes:
#include "seismic.h"


// Defines:
#define		VERSION		"1.5"			// Version number
#define		DATAFILE	1			// Input from data file
#define		SOCKET		2			// Input from socket
#define		TTY		3			// Input from serial port
#define		PORT		1146			// Standard port for TCP usage
#define		TSINT		99			// Add a timestamp every TSINT readings           

#define		TTY0		"/dev/ttyUSB0"		// Serial ports to be probed for 
#define		TTY1		"/dev/ttyUSB1"		// a connection
#define		TTY2		"/dev/ttyUSB2"
#define		TTY3		"/dev/ttyUSB3"



// Function prototypes:
int 		readbit();
void 		initialize();
int 		opensocket(char *server);
int 		inittty();
char 		*bitify(unsigned char v);
unsigned char	reverse(unsigned char v);
char		*bps(const int d);
char		*strip(const unsigned char *s, int len);
char		*getutc(); 					// Get time in UTC

	

// Global variables:
	int		bitcount=0;			// Global number of bits read from input source
	
	int		verbose=0;			// Verbose output
	int		raw=0;				// Raw P frame output
	
	FILE		*seismicfd;			// Input file descriptor

	int		seismicsd;			// Socket ID for network operation

	int		eof=0;				// Flag for EOF

	int		mode;				// Indicates input mode (file or socket)
	char 		*datafile;			// Input data file in file mode
	char 		*tty;				// Input serial port in tty mode
	char		host[24];			// Host name for server, for socket I/O
	int		port=PORT;			// Port for socket I/O
	int 		ttyfd;				// File descriptor for serial port
	struct termios 	SerialPortSettings;		// Serial port settings

	// Time-realted variables:
	time_t		epoch;				// Stores current UNIX epoch
	struct tm 	*utc_tm;			// Time value structure
	struct timeval 	now;				// Seconds and microseconds
	char 		utc[32], usec[16];		// Output buffers



/*
 *
 *	F U N C T I O N S
 *
 */


// Read and return one bit from the input source:
unsigned char readbyte() {
	int		err;					// Serial port error counter
	unsigned char	b;					// Stores the byte
	
	switch(mode) {
	case SOCKET:
	case DATAFILE:
		if ((fread(&b, 1, 1, seismicfd)) != 1) {	// Read one byte into b
			if (feof(seismicfd)) {
				printf("readbyte: end of file\n");
				fclose(seismicfd);
				b=0;
				eof=1;
			}
			else {
				perror("readbyte: insufficient data read");
				fclose(seismicfd);
				b=0;
				eof=1;
			}
		}
		break;
	case TTY:
		err=0;
		while(read(ttyfd, &b, 1) == -1) {
			// err++; 
			if (err > 1000) {
				// perror("Excessive bad reads on serial port");
				// close(ttyfd);
				b=0;
				eof=1;
				return 0;
			}
		}
		break;
	default:
		printf("readbyte: unknown error\n");
		close(seismicsd);
		fclose(seismicfd);
		b=0;
		eof=1;
	}
	return b;
}	


                          

// Process command line options:
void setoptions(int argc, char **argv) {
 	 extern char	*optarg;
 	 extern int 	optind;
 	 int 		opt;
 	 static char	probedtty[32];
 	 
 	 datafile=NULL;
 	 tty=NULL;
 	 while ((opt=getopt(argc, argv, "hvf:p:t:r")) != -1) {
 	 	 switch (opt) {
 	 	 case 'v':
 	 	 	 verbose++;
 	 	 	 break;
 	 	 case 'f':
 	 	 	 datafile=optarg; // strcpy(&datafile, optarg); 	
 	 	 	 printf("Data file: %s\n", datafile);
 	 	 	 mode=DATAFILE;
 	 	 	 break;
 	 	 case 'p':
 	 	 	 port=atoi(optarg);
 	 	 	 printf("Port: %d\n", port);
 	 	 	 break;
		 case 't':
 	 	 	 tty=optarg;
 	 	 	 if (strncmp(tty, "probe", 5) == 0) {
 	 	 	 	 printf("Probing USB serial ports...\n");
 	 	 	 	 strcpy(probedtty, "");
 	 	 	 	 if (access(TTY0, F_OK) != -1)
 	 	 	 	 	 strcpy(probedtty, TTY0);
 	 	 	 	 else if (access(TTY1, F_OK) != -1)
 	 	 	 	 	 strcpy(probedtty, TTY1);
 	 	 	 	 else if (access(TTY2, F_OK) != -1)
 	 	 	 	 	 strcpy(probedtty, TTY2);
 	 	 	 	 else if (access(TTY3, F_OK) != -1)
 	 	 	 	 	 strcpy(probedtty, TTY3);
 	 	 	 	 if (strlen(probedtty) != 0)
 	 	 	 	 	 printf("Found a USB serial device at %s\n", probedtty);
 	 	 	 	 else {
 	 	 	 	 	 fprintf(stderr, "Device not found\n");
 	 	 	 	 	 exit(1);
 	 	 	 	 }
 	 	 	 }
  	 	 	 printf("Serial port: %s\n", probedtty);
 	 	 	 tty=probedtty;
 	 	 	 mode=TTY;
 	 	 	 break;
	
 	 	 case 'r':
 	 	 	 raw++;
 	 	 	 break;
 	 	 case 'h':
 	 	 default:
 	 	 	 fprintf(stderr, "usage:\t%s [-h] [-v] [-f input_file] [-p port] serverIP [-t tty | probe] [-r]\n", argv[0]);
 	 	 	 fprintf(stderr, "\t-h:\t\tdisplay command line help text\n");
 	 	 	 fprintf(stderr, "\t-v:\t\tverbosity level: more 'v's for more verbosity\n");
 	 	 	 fprintf(stderr, "\t-f <file>:\tfile for analysis\n");
 	 	 	 fprintf(stderr, "\tserverIP:\tIP address of server for TCP-based input\n");
 	 	 	 fprintf(stderr, "\t-p <port>:\tport for connection (default 1146)\n");
 	 	 	 fprintf(stderr, "\t-t <tty>|probe:\tread from serial port 'tty', or probe for USB serial connection\n");
 	 	 	 fprintf(stderr, "\t-r:\t\tdisplay raw seismic data values to stderr\n");
 	 	 	 exit(0);
 	 	 }
 	 }
 	 
 	 if (!datafile && !tty) {
 	 	 if (optind >= argc) {
 	 	 	 fprintf(stderr, "%s: expected either a server IP or a tty\n", argv[0]);
 	 	 	 exit(1);
 	 	 }
 	 	 mode=SOCKET;
 	 	 strncpy(host, argv[optind], 21); 	
 	 	 printf("Host: %s\t\tPort: %d\n", host, port);
 	 }
 	 return;
}



// Initialize pre-defined standard offset values and expected next block:
void initialize() {
	strcpy(host, "127.0.0.1");		// Default server address (localhost)
	return;
}



// Open socket to the IQ data stream server:
int opensocket(char *server) {
	struct in_addr addr;
	struct sockaddr_in sa;

	if (inet_aton(server, &addr) == 0) {
		fprintf(stderr, "opensocket: inet_aton: Invalid address\n");
		exit(EXIT_FAILURE);
	}	
  	if ((seismicsd=socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "opensocket: socket: Cannot open socket\n");
		return 0;
	}
	bzero(&sa, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(PORT);
	sa.sin_addr=addr;
	if (connect(seismicsd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		fprintf(stderr, "opensocket: connect: Can't connect to server\n");
		close(seismicsd);
		return 0;
	}
	return seismicsd;
}                                          



// Function to return the 8 bits for each byte:
char 	*bitify(unsigned char v) {
	static char r[10];
	int  i;
	
	strcpy(r, "");
	for (i=7; i >= 0; i--) 
		strcat(r, (v & (1 << i)) ? "1" : "0");
	return r;
}		


// Initialize the serial port:
int inittty() {
	ttyfd=open(tty, O_RDONLY | O_NOCTTY | O_NDELAY);
	if (ttyfd == -1) {
		printf("\nError opening %s\n", tty);
		return -1;
	}
	else {
		printf("\n%s opened successfully\n", tty);
		tcgetattr(ttyfd, &SerialPortSettings);
		cfsetispeed(&SerialPortSettings,B9600);
		cfsetospeed(&SerialPortSettings,B9600);
		SerialPortSettings.c_cflag &= ~PARENB;   				// No parity
		SerialPortSettings.c_cflag &= ~CSTOPB; 					// 1 stop bit 
		SerialPortSettings.c_cflag &= ~CSIZE; 					// 8 data bits
		SerialPortSettings.c_cflag |=  CS8;		
		SerialPortSettings.c_cflag &= ~CRTSCTS;					// No flow control
		SerialPortSettings.c_cflag |= CREAD | CLOCAL;				// Local mode
		SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);			// Raw data
		SerialPortSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
		tcsetattr(ttyfd,TCSANOW,&SerialPortSettings);
		printf("Serial port configured.\n");		
	}
	return 0;
}



// Function to return pointer to buffer containing current UTC date and time:
char	*getutc() {
	
	// Output buffer:
	static char	result[32];
	
	// Local variables:
	time_t		epoch;				// Stores current UNIX epoch
	struct tm 	*utc_tm;			// Time value structure
	struct timeval 	now;				// Seconds and microseconds
	char 		utc[32];			// buffers
	
	// Determine current time (UNIX epoch in seconds):
	if (((epoch=time(NULL)) == -1) || gettimeofday(&now, NULL)) {
		fprintf(stderr, "ERR:\tFailed to obtain current time\n");
		return NULL;
	}
	else {
		utc_tm = gmtime(&epoch);
		if (strftime(utc, sizeof(utc), "%Y-%m-%d %H:%M:%S", utc_tm))
			sprintf(result, "%s.%lu", utc, now.tv_usec);
		else {
			fprintf(stderr, "ERR:\tFailure to convert the current time.\n");
			return NULL;
		}
	}
	return result;
}



// The main program:
int main(int argc, char **argv) {

	// Local variables:
	unsigned char 	b;					// Byte from seismometer
	unsigned char	df[2];					// Two-byte data frame
	int		df0, ready;				// Flags for framing
	int		data;					// One ADC value from seismometer
	int		gain;					// ADC gain value
	int		framecount=0;				// Frame counter

	
	// Intro message:
 	printf("USGS Seismometer Decoder\n");
 	printf("Version ");
 	printf(VERSION);
 	printf("\n");
	
	// Process options:
	setoptions(argc, argv);
	
	// Intialize variables:
	initialize();	
	
	// Open input stream:
	switch(mode) {
	case DATAFILE:
		if ((seismicfd=fopen(datafile, "r")) == NULL) {
			perror("Unable to open input file");
			exit(1);
		}
		printf("File open\n");
		break;
	case SOCKET:
		if (opensocket(host) == 0) {
			perror("Unable to open socket");
			exit(1);
		}
		if ((seismicfd=fdopen(seismicsd, "r")) == NULL) {
			perror("Unable to open input stream");
			exit(1);
		}
		printf("Socket open\n");
		break;
	case TTY:
		if (inittty() !=0) {
			perror("Unable to initialize serial port");
			exit(1);
		}
		printf("Serial port initialized\n");
		break;
	default:
		printf("main: Input selection: unknown error - mode = %d\n", mode);
		eof=1;
		break;
	}
	
	
	// Start scan for valid first byte from seismometer:
	printf("Starting scan...\n");

	while(!eof) {

		// Wait for a valid two byte data frame:
		df0=0;
		ready=0;
		while (!ready) {
			b=readbyte();
			if (b & 0x01) {
				df[0]=b;
				df0=1;
			}
			else {
				if (df0) {
					df[1]=b;
					ready=1;
				}
			}
		}
		
		// Time to add a timestamp to the log file?
		if ((framecount++) == TSINT) {
			printf("%s", getutc());
			framecount=0;
		}
		else
			printf("0");		// If not, use 0 for field 1 placeholder			

		// Optionally display the two-byte data frame:
		if (raw) {
			fprintf(stderr, "Data frame: 0x%1x%1x:\t%s ", df[0], df[1], bitify(df[0]));
			fprintf(stderr, "%s\n", bitify(df[1]));
		}
		
		// Process the data frame to generate a valid seismic reading:
		data=((df[0]&0xfe)<<3) | ((df[1]&0xf0)>>4);
		gain=42+6*((df[1]&0x0e)>>1);

		// Log the reading:
		printf(",%d,%d\n", data, gain);
	}
		
	// Finished! 	
	if (raw) 
		fprintf(stderr, "Data frames processed: %d\n", framecount);

	printf("Normal termination\n");
	switch (mode) {
	case SOCKET:
		close(seismicsd);
		fclose(seismicfd);
		break;
	case TTY:
		close(ttyfd);
		break;
	case DATAFILE:	
		fclose(seismicfd);
		break;
	default:
		fprintf(stderr, "Resource closure: unknown error - mode = %d\n", mode);
		break;
	}
	return 0;
}

