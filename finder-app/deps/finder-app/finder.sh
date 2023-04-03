#!/bin/sh
# finder.sh; Assignment 1.1 AELD Course 1
# 
###############
# Requirements:
###############
#
# Accepts the following runtime arguments:
#
#   arg1 = a path to a directory on the filesystem, referred to as filesdir 
#
#   arg2 = a text string which will be searched within these files, referred to as searchstr
#
# Exits with return value 1 error and print statements if any of the parameters 
# above were not specified
#
# Exits with return value 1 error and print statements if filesdir does not 
# represent a directory on the filesystem
#
# Prints message "The number of files are X and the number of matching lines are Y" 
# where X is the number of files in the directory and all subdirectories and 
# Y is the number of matching lines found in respective files.
#
#################
# Syntax Example:
#################
#
#  finder.sh /tmp/aesd/assignment1 linux
#
################################################################################

# setup command line arguments to variables for better readability
filesdir=$1
searchstr=$2


# error handling

# invalid number of arguments
if [ $# != 2 ]
then
    echo "Syntax: ./finder.sh (path) (search string)"
    exit 1
fi

# invalid path
if [ ! -d $filesdir ]
then
    echo "Path" $filesdir "does not exist."
    exit 1
fi

# Main script logic

# X=`grep -rl $searchstr $filesdir | wc -l` # if we watnted to find # of matched files
X=`find $filesdir -type f | wc -l`
Y=`grep -rc $searchstr $filesdir | awk -F':' '{sum+=$NF;}END{print sum;}'`
echo "The number of files are $X and the number of matching lines are $Y"
