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

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
   if (NULL == buffer || NULL == entry_offset_byte_rtn) 
   {
    return NULL;
   }
    struct aesd_buffer_entry *entry;
    struct aesd_buffer_entry *target_entry = NULL;
    size_t current_offset = 0;
    uint8_t i = buffer->out_offs;
    if(buffer->full == true)
    {
        entry = &buffer->entry[i];
        if(char_offset < (current_offset + entry->size)) {
            target_entry = entry;
            *entry_offset_byte_rtn = char_offset - current_offset;
        }
        current_offset+=entry->size;
        i++;
    }
    for(; (i != buffer->in_offs) && (target_entry == NULL); i = ((i+1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED))
    {
        entry = &buffer->entry[i];
        if(char_offset < (current_offset + entry->size)) {
            target_entry = entry;
            *entry_offset_byte_rtn = char_offset - current_offset;
            break;
        }
        current_offset+=entry->size;
    }
    return target_entry;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
struct aesd_buffer_entry* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
   uint8_t circulated_inoffs;
   uint8_t circulated_outoffs;
   struct aesd_buffer_entry* popped = NULL;
   if ((NULL == buffer) || (NULL == add_entry)) {
        return NULL;
    }
    // printf("buffer->in_offs before : %d ", buffer->in_offs);
    // printf("buffer->out_offs before : %d \n", buffer->out_offs);
   circulated_inoffs = buffer->in_offs%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
   circulated_outoffs = buffer->out_offs%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (buffer->full) {
        popped = &buffer->entry[circulated_inoffs];
        circulated_outoffs = (circulated_outoffs + 1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    buffer->entry[circulated_inoffs] = *add_entry;
    circulated_inoffs = (circulated_inoffs + 1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    if (circulated_inoffs == circulated_outoffs)
    {
        buffer->full = true;
    }
    buffer->in_offs = circulated_inoffs;
    buffer->out_offs = circulated_outoffs;
    // printf("buffer->in_offs after : %d ", buffer->in_offs);
    // printf("buffer->out_offs after : %d\n", buffer->out_offs);
    return popped;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
