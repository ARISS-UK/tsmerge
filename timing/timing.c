#include <time.h>
#include <inttypes.h>
#include <asm/errno.h>
#include <pthread.h>

/* Returns the current unix timestamp in ms, or 0 if error */
int64_t timestamp_ms(void)
{
    struct timespec tp;

    if(clock_gettime(CLOCK_REALTIME, &tp) != 0)
    {
        return(0);
    }

    return((int64_t) tp.tv_sec * 1000 + tp.tv_nsec / 1000000);
}

void sleep_ms(uint32_t _duration)
{
    struct timespec req, rem;
    req.tv_sec = _duration / 1000;
    req.tv_nsec = (_duration - (req.tv_sec*1000))*1000*1000;

    while(nanosleep(&req, &rem) == EINTR)
    {
        /* Interrupted by signal, shallow copy remaining time into request, and resume */
        req = rem;
    }
}

int64_t thread_timestamp(pthread_t target_thread)
{
    struct timespec ts;
    clockid_t cid;

    pthread_getcpuclockid(target_thread, &cid);
    clock_gettime(cid, &ts);
    
    return((int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
