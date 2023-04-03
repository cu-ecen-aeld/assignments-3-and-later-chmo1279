/******************************************************************************
 writer; Assignment 1.2 AELD Course 1
 
##############
 Requirements:
##############

 Accepts the following arguments: 

 arg1 = a full path to a file (including filename) on the filesystem, referred 
 to as writefile; 

 arg2 = a text string which will be written within this file, referred to as writestr

 Exits with value 1 error and print statements if any of the arguments 
   above were not specified

 Creates a new file with name and path writefile with content writestr, 
   overwriting any existing file and creating the path if it doesn’t exist. 

 Exits with value 1 and error print statement if the file could not be created.

################
 Syntax Example:
################

 writer /tmp/aesd/assignment1/sample.txt ios 

   Creates file:

       /tmp/aesd/assignment1/sample.txt

   With content:
       
       ios

###############################################################################

One difference from the write.sh instructions in Assignment 1:  
You do not need to make your "writer" utility create directories which do not exist.  
You can assume the directory is created by the caller.

Setup syslog logging for your utility using the LOG_USER facility.

Use the syslog capability to write a message “Writing <string> to <file>” 
where <string> is the text string written to file (second argument) and 
<file> is the file created by the script.  This should be written with LOG_DEBUG level.

Use the syslog capability to log any unexpected errors with LOG_ERR level.
*******************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

int main( int argc, char *argv[] )  {

    // Assigning arguments to variables
    char *writefile = argv[1];
    char *writestr = argv[2];

    // Error Checking
    if( argc != 3 ) { // throw error if syntax incorrect
        syslog(LOG_ERR, "Error: Syntax = ./writer (file) (string)");
        return 1;    
    }
    else { // write to file  
        FILE *file_for_string = fopen(writefile, "w");
        if (file_for_string == NULL) { //throw error if file cannot be created
            syslog(LOG_ERR, "Error: %s", strerror( errno ));
            return 1;
        }
        fprintf(file_for_string, "%s", writestr);
        fclose(file_for_string);
        }

    syslog(LOG_DEBUG, "Writing %s to %s",writestr, writefile);
    return 0;
}
