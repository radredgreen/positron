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


#include "POSRingBufferAudioOut.h"

/**
 * @file
 * Implementation of ring buffer functions.
 * Credit: https://github.com/AndersKaloer/Ring-Buffer
 */

void ptr_ring_buffer_ao_init(ring_buffer_ao_t *buffer, void *buf, size_t buf_size) {
  RING_BUFFER_ASSERT(RING_BUFFER_IS_POWER_OF_TWO(buf_size) == 1);
  buffer->buffer = buf;
  buffer->buffer_mask = buf_size - 1;
  buffer->tail_index = 0;
  buffer->head_index = 0;
}

void ptr_ring_buffer_ao_queue(ring_buffer_ao_t *buffer, void * data) {
  /* Is buffer full? */
  if(ptr_ring_buffer_ao_is_full(buffer)) {
    /* Is going to overwrite the oldest byte */
    /* Increase tail index */
    buffer->tail_index = ((buffer->tail_index + 1) & RING_BUFFER_MASK(buffer));
  }

  /* Place data in buffer */
  buffer->buffer[buffer->head_index] = data;
  buffer->head_index = ((buffer->head_index + 1) & RING_BUFFER_MASK(buffer));
}

uint8_t ptr_ring_buffer_ao_dequeue(ring_buffer_ao_t *buffer, void ** data) {
  if(ptr_ring_buffer_ao_is_empty(buffer)) {
    /* No items */
    return 0;
  }
  
  *data = buffer->buffer[buffer->tail_index];
  buffer->tail_index = ((buffer->tail_index + 1) & RING_BUFFER_MASK(buffer));
  return 1;
}


uint8_t ptr_ring_buffer_ao_peek(ring_buffer_ao_t *buffer, void ** data, ring_buffer_size_t index) {
  if(index >= ptr_ring_buffer_ao_num_items(buffer)) {
    /* No items at index */
    return 0;
  }
  
  /* Add index to pointer */
  ring_buffer_size_t data_index = ((buffer->tail_index + index) & RING_BUFFER_MASK(buffer));
  *data = buffer->buffer[data_index];
  return 1;
}

extern inline uint8_t ptr_ring_buffer_ao_is_empty(ring_buffer_ao_t *buffer);
extern inline uint8_t ptr_ring_buffer_ao_is_full(ring_buffer_ao_t *buffer);
extern inline ring_buffer_size_t ptr_ring_buffer_ao_num_items(ring_buffer_ao_t *buffer);

