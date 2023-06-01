/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

// struct aesd_buffer_entry
// {
//     const char *buffptr; //A location where the buffer contents in buffptr are stored
//     
//     size_t size; //Number of bytes stored in buffptr
// };
// 
// struct aesd_circular_buffer
// {
//     struct aesd_buffer_entry  entry[AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];
//          An array of pointers to memory allocated for the most recent write operations
//     
//     uint8_t in_offs; //The current location in the entry structure where the next write should be stored 
//     
//     uint8_t out_offs; //The first location in the entry structure to read from
//     
//     bool full; //set to true when the buffer entry structure is full

// };

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer,
                                    const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
}

/**
 *  Finds entry offset for byte position in buffer 
 *   Any necessary locking must be performed by caller.
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(//fpos: byte position in buffer
                            struct aesd_circular_buffer *buffer, //target buffer to search for corresponding offset.
                            size_t char_offset, //position to search for in buffer list, describing the zero reference character index if all buffer strings were concatenated end to end (start position?)
                            size_t *entry_offset_byte_rtn )//pointer specifying a location to store the byte of the returned aesd_buffer_entry buffptr member corresponding to char_offset. Only set when a matching char_offset is found in buffer.
{
    /**
    * TODO: implement per description
    */
    return NULL;//return the struct aesd_buffer_entry which represents the location described by char_offset, or NULL if this position is not available in the buffer (not enough data is written).
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
