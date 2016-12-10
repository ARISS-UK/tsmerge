#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ts.h"
#include "merger_rx_buffer.h"

void rxBufferInit(void *buffer_void_ptr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    pthread_mutex_init(&buf->Mutex, NULL);
    pthread_cond_init(&buf->Signal, NULL);
    
    pthread_mutex_lock(&buf->Mutex);
    buf->Head = 0;
    buf->Tail = 0;
    pthread_mutex_unlock(&buf->Mutex);
}

uint8_t rxBufferNotEmpty(void *buffer_void_ptr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;

    uint8_t result;
    
    pthread_mutex_lock(&buf->Mutex);
    result = (buf->Head!=buf->Tail);
    pthread_mutex_unlock(&buf->Mutex);
    
    return result;
}

uint16_t rxBufferHead(void *buffer_void_ptr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    uint16_t result;
    
    pthread_mutex_lock(&buf->Mutex);
    result = buf->Head;
    pthread_mutex_unlock(&buf->Mutex);
    
    return result;
}

uint16_t rxBufferTail(void *buffer_void_ptr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    uint16_t result;
    
    pthread_mutex_lock(&buf->Mutex);
    result = buf->Tail;
    pthread_mutex_unlock(&buf->Mutex);
    
    return result;
}

void rxBufferPush(void *buffer_void_ptr, uint64_t timestamp, uint8_t *data_p)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    rxBufferElement_t newData;
    /* RX Timestamp */
    newData.timestamp = timestamp;
    /* RX Data Buffer */
    memcpy(&newData.data, data_p, 0x10 + TS_PACKET_SIZE);
    
    pthread_mutex_lock(&buf->Mutex);
    if(buf->Head==1023)
        buf->Head=0;
    else
        buf->Head++;
    buf->Buffer[buf->Head] = newData;
    
    pthread_cond_signal(&buf->Signal);
    pthread_mutex_unlock(&buf->Mutex);
}

void rxBufferPop(void *buffer_void_ptr, rxBufferElement_t *rxBufferElementPtr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    pthread_mutex_lock(&buf->Mutex);
    if(buf->Head!=buf->Tail)
    {
        if(buf->Tail==1023)
            buf->Tail=0;
        else
            buf->Tail++;
        *rxBufferElementPtr = buf->Buffer[buf->Tail];
    
        pthread_mutex_unlock(&buf->Mutex);
    }
    else
    {
        pthread_mutex_unlock(&buf->Mutex);
        
        rxBufferElementPtr = NULL;
    }
}

void rxBufferWaitPop(void *buffer_void_ptr, rxBufferElement_t *rxBufferElementPtr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    pthread_mutex_lock(&buf->Mutex);
    
    while(buf->Head==buf->Tail) /* If buffer is empty */
    {
        /* Mutex is atomically unlocked on beginning waiting for signal */
        pthread_cond_wait(&buf->Signal, &buf->Mutex);
        /* and locked again on resumption */
    }
    
    if(buf->Tail==1023)
        buf->Tail=0;
    else
        buf->Tail++;
    *rxBufferElementPtr = buf->Buffer[buf->Tail];
    
    pthread_mutex_unlock(&buf->Mutex);
}
