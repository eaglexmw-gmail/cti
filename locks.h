#ifndef _CTI_LOCK_H_
#define _CTI_LOCK_H_

#ifdef __linux__
#include <pthread.h>
#include <semaphore.h>
typedef struct {
  pthread_mutex_t mlock;
} Lock;

typedef struct {
  pthread_cond_t event;
} Event;

#endif

#ifdef __APPLE__
#include <pthread.h>
typedef struct {
  pthread_mutex_t lock;
} Lock;
#endif

#ifdef __MINGW32__
typedef struct {
} Lock;

typedef struct {
} Event;
#endif

#ifdef __MSP430__
/* Just for testing the compiler... */
typedef struct {
} Lock;

typedef struct {
} Event;

#endif

typedef struct {
  Lock lock;
  int _count;			/* should only be accessed within API functions */
} LockedRef;

extern void Lock_init(Lock *lock);
extern void Lock_acquire(Lock *lock);
extern void Lock_release(Lock *lock);

extern void Lock_release__event_wait__lock_acquire(Lock *lock, Event *ev);
extern void Event_signal(Event *ev);

extern void LockedRef_init(LockedRef *ref);
extern void LockedRef_increment(LockedRef *ref);
extern void LockedRef_decrement(LockedRef *ref, int * ok_to_free);

#endif
