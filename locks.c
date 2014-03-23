#include "locks.h"
#include <stdio.h>
#include <stdlib.h>


void Lock_init(Lock *lock)
{
#ifdef __linux__
  pthread_mutex_init(&lock->mlock, NULL);
#endif
}


void Lock_acquire(Lock *lock)
{
#ifdef __linux__
  int rc;
  rc = pthread_mutex_lock(&lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_mutex_lock returned %d\n", rc);
    StackDebug(); exit(1);
  }
#endif
}

void Lock_release(Lock *lock)
{
#ifdef __linux__
  int rc;
  rc = pthread_mutex_unlock(&lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_mutex_unlock returned %d\n", rc);
    StackDebug(); exit(1);
  }
#endif
}

void Lock_release__event_wait__lock_acquire(Lock *lock, Event *event)
{
#ifdef __linux__
  int rc;
  rc = pthread_cond_wait(&event->event, &lock->mlock);
  if (rc != 0) {
    fprintf(stderr, "pthread_cond_wait returned %d\n", rc);
    StackDebug(); exit(1);
  }
#endif
}

void Event_signal(Event *ev)
{
#ifdef __linux__
  pthread_cond_broadcast(&ev->event);
#endif
}

void WaitLock_init(WaitLock *w)
{
#ifdef __linux__
#endif
}

void WaitLock_wait(WaitLock *w)
{
#ifdef __linux__
  int rc;
  rc = sem_wait(&w->wlock);
  if (rc != 0) {
    perror("sem_wait");
    StackDebug(); exit(1);
  }
#endif
}

void WaitLock_wake(WaitLock *w)
{
#ifdef __APPLE__
  if (pthread_cond_signal(&input->parent->notification_cond) != 0) {
    perror("pthread_cond_signal");
  }
#endif
#ifdef __linux__
  int rc;
  /* Wake receiver. */
  rc = sem_post(&w->wlock);
  if (rc != 0) {
    perror("sem_wait");
    StackDebug(); exit(1);
  }
#endif
}

/* Reference count with lock. */
void LockedRef_init(LockedRef *ref)
{
  ref->_count = 1;
  Lock_init(&ref->lock);
}


void LockedRef_increment(LockedRef *ref)
{
  Lock_acquire(&ref->lock);
  ref->_count += 1;
  Lock_release(&ref->lock);  
}


void LockedRef_decrement(LockedRef *ref, int * count)
{
  Lock_acquire(&ref->lock);
  ref->_count -= 1;
  *count = ref->_count;
  Lock_release(&ref->lock);
}
