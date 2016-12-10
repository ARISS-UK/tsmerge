#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "merger.h"
#include "timing.h"
#include "merger_rx_buffer.h"

void *merger_rx_feed(void *Buffer_void_ptr)
{
  rxBuffer_t *rxBufPtr;
  rxBufPtr = (rxBuffer_t *)Buffer_void_ptr;
  
  rxBufferElement_t tspacket;
        
  while(1)
  {
    rxBufferWaitPop(rxBufPtr, &tspacket);
    
    //printf("RX Data content: 0x%02X%02X%02X%02X\n",
    //  tspacket.data[0], tspacket.data[1], tspacket.data[2], tspacket.data[3]);
    
    pthread_mutex_lock(&merger.lock);
    mx_feed(&merger, tspacket.timestamp, tspacket.data);
    pthread_mutex_unlock(&merger.lock);
  }
}
