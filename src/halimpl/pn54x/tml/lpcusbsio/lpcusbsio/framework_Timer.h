#ifndef FRAMEWORK_TIMER_H
#define FRAMEWORK_TIMER_H

#include "framework_Interface.h"

typedef void (framework_TimerCallBack)(void*);

// NOTE : not fully tested !!!

void framework_TimerCreate(void **timer);
void framework_TimerStart(void *timer,uint32_t delay,framework_TimerCallBack *cb,void *usercontext);
void framework_TimerStop(void *timer);
void framework_TimerDelete(void *timer);



#endif // ndef FRAMEWORK_TIMER_H

