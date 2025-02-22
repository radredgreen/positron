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

#include <stdio.h> // temp for printf
#include <string.h> //for memcopy
#include "POSRingBufferVideoIn.h"

/**
 * @file
 * Implementation of ring buffer functions.
 * Credit: https://github.com/AndersKaloer/Ring-Buffer
 */

void ptr_ring_buffer_vi_init(ring_buffer_vi_t *buffer, ring_buffer_vi_element_t *buf, size_t buf_size) {
  RING_BUFFER_ASSERT(RING_BUFFER_IS_POWER_OF_TWO(buf_size) == 1);
  buffer->buffer = buf;
  buffer->buffer_mask = buf_size - 1;
  buffer->tail_index = 0;
  buffer->head_index = 0;
}

void ptr_ring_buffer_vi_queue(ring_buffer_vi_t *buffer, ring_buffer_vi_element_t * data) {
  /* Is buffer full? */
  if(ptr_ring_buffer_vi_is_full(buffer)) {
    /* Is going to overwrite the oldest byte */
    /* Increase tail index */
    buffer->tail_index = ((buffer->tail_index + 1) & RING_BUFFER_MASK(buffer));
  }

  /* Place data in buffer */
  //buffer->buffer[buffer->head_index] = data;
  //printf("ring inserting index: %d\n", buffer->head_index);
  memcpy(&buffer->buffer[buffer->head_index], data, sizeof(ring_buffer_vi_element_t));
  buffer->head_index = ((buffer->head_index + 1) & RING_BUFFER_MASK(buffer));
}

uint8_t ptr_ring_buffer_vi_dequeue(ring_buffer_vi_t *buffer, ring_buffer_vi_element_t * data) {
  if(ptr_ring_buffer_vi_is_empty(buffer)) {
    printf("ring no data to dequeue\n");
    /* No items */
    return 0;
  }
  
  //*data = buffer->buffer[buffer->tail_index];
  //printf("ring dequeuing index: %d\n", buffer->tail_index);
  memcpy(data, &buffer->buffer[buffer->tail_index], sizeof(ring_buffer_vi_element_t));
  buffer->tail_index = ((buffer->tail_index + 1) & RING_BUFFER_MASK(buffer));
  return 1;
}


uint8_t ptr_ring_buffer_vi_peek(ring_buffer_vi_t *buffer, ring_buffer_vi_element_t * data, ring_buffer_vi_size_t index) {
  if(index >= ptr_ring_buffer_vi_num_items(buffer)) {
    printf("ring no data to peek\n");
    /* No items at index */
    return 0;
  }
  
  printf("ring peeking index: %d\n", index);

  /* Add index to pointer */
  ring_buffer_vi_size_t data_index = ((buffer->tail_index + index) & RING_BUFFER_MASK(buffer));
  //*data = buffer->buffer[data_index];
  memcpy(data, &buffer->buffer[data_index], sizeof(ring_buffer_vi_element_t));
  return 1;
}

extern inline uint8_t ptr_ring_buffer_vi_is_empty(ring_buffer_vi_t *buffer);
extern inline uint8_t ptr_ring_buffer_vi_is_full(ring_buffer_vi_t *buffer);
extern inline ring_buffer_vi_size_t ptr_ring_buffer_vi_num_items(ring_buffer_vi_t *buffer);

