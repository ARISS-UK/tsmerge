#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "merger.h"
#include "timing.h"
#include "merger_rx_buffer.h"

/* This function is run on a thread, started from main()
 *
 * This function waits on rxBuffer for packets to be added by 'merger_rx_socket'
 * On a packet being added to buffer, it is popped, and fed into the merger with 'mx_feed()'
 */
void *merger_rx_feed(void *Buffer_void_ptr)
{
  rxBuffer_t *rxBufPtr;
  rxBufPtr = (rxBuffer_t *)Buffer_void_ptr;
  
  rxBufferElement_t tspacket;
        
  while(1)
  {
    rxBufferWaitPop(rxBufPtr, &tspacket);
    
    pthread_mutex_lock(&merger.lock);
    mx_feed(&merger, tspacket.timestamp, tspacket.data);
    pthread_mutex_unlock(&merger.lock);
  }
}
