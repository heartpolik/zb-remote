#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#define BIT(n) (1UL << (n))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define CLAMP(v,l,h) ((v)<(l)?(l):(v)>(h)?(h):(v))
#define snprintk snprintf
struct k_work { int x; }; struct k_work_delayable { int x; };
#define K_MUTEX_DEFINE(n) int n
#define K_WORK_DELAYABLE_DEFINE(n,f) struct k_work_delayable n
#define K_WORK_DEFINE(n,f) struct k_work n
#define K_FOREVER 0
#define K_MSEC(x) (x)
#define k_mutex_lock(a,b) 0
#define k_mutex_unlock(a) 0
#define k_work_submit(a) 0
#define k_work_reschedule(a,b) 0
#define k_work_cancel_delayable(a) 0
#define k_msleep(x) 0
static inline int64_t k_uptime_get(void){return 0;}
