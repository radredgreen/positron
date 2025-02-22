/* 
 * This file is part of the positron distribution (https://github.com/radredgreen/positron).
 * Copyright (c) 2024 RadRedGreen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <inttypes.h>
#include <stddef.h>
#include <assert.h>
/**
 * @file
 * Prototypes and structures for the ring buffer module.
 * Credit: https://github.com/AndersKaloer/Ring-Buffer
 */


#ifndef POSRINGBUFFER_H
#define POSRINGBUFFER_H

#ifdef __cplusplus
extern "C"
{
#endif

#define RING_BUFFER_ASSERT(x) assert(x)

/**
 * Checks if the buffer_size is a power of two.
 * Due to the design only <tt> RING_BUFFER_SIZE-1 </tt> items
 * can be contained in the buffer.
 * buffer_size must be a power of two.
*/
#define RING_BUFFER_IS_POWER_OF_TWO(buffer_size) ((buffer_size & (buffer_size - 1)) == 0)

/**
 * The type which is used to hold the size
 * and the indicies of the buffer.
 */
typedef size_t ring_buffer_vi_size_t;

/**
 * Used as a modulo operator
 * as <tt> a % b = (a & (b − 1)) </tt>
 * where \c a is a positive index in the buffer and
 * \c b is the (power of two) size of the buffer.
 */
#define RING_BUFFER_MASK(rb) (rb->buffer_mask)

/**
 * Simplifies the use of <tt>struct ring_buffer_vi_element_t</tt>.
 */
typedef struct ring_buffer_vi_element_t ring_buffer_vi_element_t;

/**
 * Structure which holds a ring buffer.
 * The buffer contains a buffer array
 * as well as metadata for the ring buffer.
 */
struct ring_buffer_vi_element_t {
  void *loc;
  size_t len;
  uint64_t timestamp; //us
  uint32_t dur; //ms
};

/**
 * Simplifies the use of <tt>struct ring_buffer_vi_t</tt>.
 */
typedef struct ring_buffer_vi_t ring_buffer_vi_t;

/**
 * Structure which holds a ring buffer.
 * The buffer contains a buffer array
 * as well as metadata for the ring buffer.
 */
struct ring_buffer_vi_t {
  /** Buffer memory. */
  ring_buffer_vi_element_t *buffer;
  /** Buffer mask. */
  ring_buffer_vi_size_t buffer_mask;
  /** Index of tail. */
  ring_buffer_vi_size_t tail_index;
  /** Index of head. */
  ring_buffer_vi_size_t head_index;
};

/**
 * Initializes the ring buffer pointed to by <em>buffer</em>.
 * This function can also be used to empty/reset the buffer.
 * The resulting buffer can contain <em>buf_size-1</em> bytes.
 * @param buffer The ring buffer to initialize.
 * @param buf The buffer allocated for the ringbuffer.
 * @param buf_size The size of the allocated ringbuffer.
 */
void ptr_ring_buffer_vi_init(ring_buffer_vi_t *buffer, ring_buffer_vi_element_t *buf, size_t buf_size);

/**
 * Adds a byte to a ring buffer.
 * @param buffer The buffer in which the data should be placed.
 * @param data The byte to place.
 */
void ptr_ring_buffer_vi_queue(ring_buffer_vi_t *buffer,  ring_buffer_vi_element_t * data);

/**
 * Returns the oldest byte in a ring buffer.
 * @param buffer The buffer from which the data should be returned.
 * @param data A pointer to the location at which the data should be placed.
 * @return 1 if data was returned; 0 otherwise.
 */
uint8_t ptr_ring_buffer_vi_dequeue(ring_buffer_vi_t *buffer, ring_buffer_vi_element_t * data);

/**
 * Peeks a ring buffer, i.e. returns an element without removing it.
 * @param buffer The buffer from which the data should be returned.
 * @param data A pointer to the location at which the data should be placed.
 * @param index The index to peek.
 * @return 1 if data was returned; 0 otherwise.
 */
uint8_t ptr_ring_buffer_vi_peek(ring_buffer_vi_t *buffer, ring_buffer_vi_element_t * data, ring_buffer_vi_size_t index);


/**
 * Returns whether a ring buffer is empty.
 * @param buffer The buffer for which it should be returned whether it is empty.
 * @return 1 if empty; 0 otherwise.
 */
inline uint8_t ptr_ring_buffer_vi_is_empty(ring_buffer_vi_t *buffer) {
  return (buffer->head_index == buffer->tail_index);
}

/**
 * Returns whether a ring buffer is full.
 * @param buffer The buffer for which it should be returned whether it is full.
 * @return 1 if full; 0 otherwise.
 */
inline uint8_t ptr_ring_buffer_vi_is_full(ring_buffer_vi_t *buffer) {
  return ((buffer->head_index - buffer->tail_index) & RING_BUFFER_MASK(buffer)) == RING_BUFFER_MASK(buffer);
}

/**
 * Returns the number of items in a ring buffer.
 * @param buffer The buffer for which the number of items should be returned.
 * @return The number of items in the ring buffer.
 */
inline ring_buffer_vi_size_t ptr_ring_buffer_vi_num_items(ring_buffer_vi_t *buffer) {
  return ((buffer->head_index - buffer->tail_index) & RING_BUFFER_MASK(buffer));
}

#ifdef __cplusplus
}
#endif

#endif /* POSRINGBUFFER_H */
