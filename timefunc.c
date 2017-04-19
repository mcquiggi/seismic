#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

// Function prototype:
char	*getutc();


// Function to return pointer to buffer containing current UTC date and time:
char	*getutc() {
	
	// Output buffer:
	static char	result[32];
	
	// Local variables:
	time_t		epoch;				// Stores current UNIX epoch
	struct tm 	*utc_tm;			// Time value structure
	struct timeval 	now;				// Seconds and microseconds
	char 		utc[32];			//  buffers
	
	// Determine current time (UNIX epoch in seconds):
	if (((epoch=time(NULL)) == -1) || gettimeofday(&now, NULL)) {
		fprintf(stderr, "ERR:\tFailed to obtain current time\n");
		return NULL;
	}
	else {
		utc_tm = gmtime(&epoch);
		if (strftime(utc, sizeof(utc), "%Y-%m-%d %H:%M:%S", utc_tm))
			sprintf(result, "UTC:\t%s.%d", utc, now.tv_usec);
		else {
			fprintf(stderr, "ERR:\tFailure to convert the current time.\n");
			return NULL;
		}
	}
	return result;
}



int main(void) {
	
	// Local variables:
	time_t		epoch;				// Stores current UNIX epoch
	struct tm 	*utc_tm;			// Time value structure
	struct timeval 	now;				// Seconds and microseconds
	char 		utc[32], usec[16];		// Output buffers
	
	printf("Direct:\n");
	
	// Determine current time (UNIX epoch in seconds):
	if (((epoch=time(NULL)) == -1) || gettimeofday(&now, NULL))
		fprintf(stderr, "ERR:\tFailed to obtain current time\n");
	else {
		utc_tm = gmtime(&epoch);
		if (strftime(utc, sizeof(utc), "%Y-%m-%d %H:%M:%S", utc_tm)) {
			printf("UTC:\t%s.", utc);
			sprintf(usec,"%d", (int) now.tv_usec);
			printf("%s\n", usec);    
    		}
		else
			fprintf(stderr, "ERR:\tFailure to convert the current time.\n");
	}

	printf("Function call:\n");
	printf("%s\n", getutc());
	
	exit(EXIT_SUCCESS);
}
