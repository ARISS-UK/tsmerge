#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merger.h"

void rxBufferInit(void *buffer_void_ptr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    pthread_mutex_init(&buf->Mutex, NULL);
    pthread_cond_init(&buf->Signal, NULL);
    
    pthread_mutex_lock(&buf->Mutex);
    buf->Head = 0;
    buf->Tail = 0;
    buf->Loss = 0;
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

uint16_t rxBufferLoss(void *buffer_void_ptr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    uint16_t result;
    
    pthread_mutex_lock(&buf->Mutex);
    result = buf->Loss;
    pthread_mutex_unlock(&buf->Mutex);
    
    return result;
}

/* Lossy when buffer is full */
void rxBufferPush(void *buffer_void_ptr, uint64_t timestamp, uint8_t *data_p)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    pthread_mutex_lock(&buf->Mutex);
    if(buf->Head!=(buf->Tail-1) && !(buf->Head==(RX_BUFFER_LENGTH-1) && buf->Tail==0))
    {
        if(buf->Head==(RX_BUFFER_LENGTH-1))
            buf->Head=0;
        else
            buf->Head++;

        buf->Buffer[buf->Head].timestamp = timestamp;
        memcpy(&buf->Buffer[buf->Head].data, data_p, MX_PACKET_LEN);
        
        pthread_cond_signal(&buf->Signal);
    }
    else
    {
        buf->Loss++;
    }
    pthread_mutex_unlock(&buf->Mutex);
}

/* Lossy when buffer is full */
void rxBufferBurstPush(void *buffer_void_ptr, uint64_t timestamp, uint8_t *data_p, uint16_t data_len)
{
    uint16_t i;
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    pthread_mutex_lock(&buf->Mutex);
    
    for(i=0;i<data_len;i+=MX_PACKET_LEN)
    {
        if(buf->Head!=(buf->Tail-1) && !(buf->Head==(RX_BUFFER_LENGTH-1) && buf->Tail==0))
        {
            if(buf->Head==(RX_BUFFER_LENGTH-1))
                buf->Head=0;
            else
                buf->Head++;

            buf->Buffer[buf->Head].timestamp = timestamp;
            memcpy(&buf->Buffer[buf->Head].data, &data_p[i], MX_PACKET_LEN);
        
            if(i==0)
            {
                pthread_cond_signal(&buf->Signal);
            }
        }
        else
        {
            buf->Loss++;
        }
    }
    pthread_mutex_unlock(&buf->Mutex);
}

void rxBufferPop(void *buffer_void_ptr, rxBufferElement_t *rxBufferElementPtr)
{
    rxBuffer_t *buf;
    buf = (rxBuffer_t *)buffer_void_ptr;
    
    pthread_mutex_lock(&buf->Mutex);
    if(buf->Head!=buf->Tail)
    {
        if(buf->Tail==(RX_BUFFER_LENGTH-1))
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
    
    if(buf->Tail==(RX_BUFFER_LENGTH-1))
        buf->Tail=0;
    else
        buf->Tail++;
    *rxBufferElementPtr = buf->Buffer[buf->Tail];
    
    pthread_mutex_unlock(&buf->Mutex);
}
