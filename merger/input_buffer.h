#ifndef __MERGER_RX_BUFFER_H
#define __MERGER_RX_BUFFER_H

#include <pthread.h>
#include "merger.h"
#include "../ts/ts.h"

#define RX_BUFFER_LENGTH    4096

typedef struct
{
    /* Timestamp of packet rx */
    int64_t timestamp;
    /* Data buffer */
    uint8_t data[MX_PACKET_LEN];
    
} rxBufferElement_t;

typedef struct
{
    /* Buffer Access Lock */
    pthread_mutex_t Mutex;
    /* New Data Signal */
    pthread_cond_t Signal;
    /* Head and Tail Indexes */
    uint16_t Head, Tail;
    /* Data Loss Counter */
    uint16_t Loss;
    /* Data */
    rxBufferElement_t Buffer[RX_BUFFER_LENGTH];
} rxBuffer_t;

/** Common functions **/
void rxBufferInit(void *buffer_void_ptr);
uint8_t rxBufferNotEmpty(void *buffer_void_ptr);
uint16_t rxBufferHead(void *buffer_void_ptr);
uint16_t rxBufferTail(void *buffer_void_ptr);
uint16_t rxBufferLoss(void *buffer_void_ptr);
void rxBufferPush(void *buffer_void_ptr, uint64_t timestamp, uint8_t *data_p);
void rxBufferBurstPush(void *buffer_void_ptr, uint64_t timestamp, uint8_t *data_p, uint16_t data_len);
void rxBufferPop(void *buffer_void_ptr, rxBufferElement_t *rxBufferElementPtr);
void rxBufferWaitPop(void *buffer_void_ptr, rxBufferElement_t *rxBufferElementPtr);

#endif
