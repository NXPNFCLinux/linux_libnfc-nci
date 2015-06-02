/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
 /******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2014 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#ifndef GKI_COMMON_H
#define GKI_COMMON_H

#include "gki.h"
#include "dyn_mem.h"

#ifndef GKI_DEBUG
#define GKI_DEBUG   FALSE
#endif

/* Task States: (For OSRdyTbl) */
#define TASK_DEAD       0   /* b0000 */
#define TASK_READY      1   /* b0001 */
#define TASK_WAIT       2   /* b0010 */
#define TASK_DELAY      4   /* b0100 */
#define TASK_SUSPEND    8   /* b1000 */


/********************************************************************
**  Internal Error codes
*********************************************************************/
#define GKI_ERROR_BUF_CORRUPTED         0xFFFF
#define GKI_ERROR_NOT_BUF_OWNER         0xFFFE
#define GKI_ERROR_FREEBUF_BAD_QID       0xFFFD
#define GKI_ERROR_FREEBUF_BUF_LINKED    0xFFFC
#define GKI_ERROR_SEND_MSG_BAD_DEST     0xFFFB
#define GKI_ERROR_SEND_MSG_BUF_LINKED   0xFFFA
#define GKI_ERROR_ENQUEUE_BUF_LINKED    0xFFF9
#define GKI_ERROR_DELETE_POOL_BAD_QID   0xFFF8
#define GKI_ERROR_BUF_SIZE_TOOBIG       0xFFF7
#define GKI_ERROR_BUF_SIZE_ZERO         0xFFF6
#define GKI_ERROR_ADDR_NOT_IN_BUF       0xFFF5


/********************************************************************
**  Misc constants
*********************************************************************/

#define GKI_MAX_INT32           (0x7fffffffL)
#define GKI_MAX_TIMESTAMP       (0xffffffffL)

/********************************************************************
**  Buffer Management Data Structures
*********************************************************************/

typedef struct _buffer_hdr
{
    struct _buffer_hdr *p_next;   /* next buffer in the queue */
    UINT8   q_id;                 /* id of the queue */
    UINT8   task_id;              /* task which allocated the buffer*/
    UINT8   status;               /* FREE, UNLINKED or QUEUED */
    UINT8   Type;

#if GKI_BUFFER_DEBUG
    /* for tracking who allocated the buffer */
    #define _GKI_MAX_FUNCTION_NAME_LEN   (50)
    char    _function[_GKI_MAX_FUNCTION_NAME_LEN+1];
    int     _line;
#endif

} BUFFER_HDR_T;

typedef struct _free_queue
{
    BUFFER_HDR_T *p_first;      /* first buffer in the queue */
    BUFFER_HDR_T *p_last;       /* last buffer in the queue */
    UINT16          size;          /* size of the buffers in the pool */
    UINT16          total;         /* toatal number of buffers */
    UINT16          cur_cnt;       /* number of  buffers currently allocated */
    UINT16          max_cnt;       /* maximum number of buffers allocated at any time */
} FREE_QUEUE_T;


/* Buffer related defines
*/
#define ALIGN_POOL(pl_size)  ( (((pl_size) + (sizeof(UINT32)-1)) / sizeof(UINT32)) * sizeof(UINT32))
#define BUFFER_HDR_SIZE     (sizeof(BUFFER_HDR_T))                  /* Offset past header */
#define BUFFER_PADDING_SIZE (sizeof(BUFFER_HDR_T) + sizeof(UINT32)) /* Header + Magic Number */
#define MAX_USER_BUF_SIZE   ((UINT16)0xffff - BUFFER_PADDING_SIZE)  /* pool size must allow for header */
#define MAGIC_NO            0xDDBADDBA

#define BUF_STATUS_FREE     0
#define BUF_STATUS_UNLINKED 1
#define BUF_STATUS_QUEUED   2

#define GKI_USE_DEFERED_ALLOC_BUF_POOLS

/* Exception related structures (Used in debug mode only)
*/
#if (GKI_DEBUG == TRUE)
typedef struct
{
    UINT16  type;
    UINT8   taskid;
    UINT8   msg[GKI_MAX_EXCEPTION_MSGLEN];
} EXCEPTION_T;
#endif


/* Put all GKI variables into one control block
*/
typedef struct
{
    /* Task management variables
    */
    /* The stack and stack size are not used on Windows
    */
#if (!defined GKI_USE_DEFERED_ALLOC_BUF_POOLS && (GKI_USE_DYNAMIC_BUFFERS == FALSE))

#if (GKI_NUM_FIXED_BUF_POOLS > 0)
    UINT8 bufpool0[(ALIGN_POOL(GKI_BUF0_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF0_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 1)
    UINT8 bufpool1[(ALIGN_POOL(GKI_BUF1_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF1_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 2)
    UINT8 bufpool2[(ALIGN_POOL(GKI_BUF2_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF2_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 3)
    UINT8 bufpool3[(ALIGN_POOL(GKI_BUF3_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF3_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 4)
    UINT8 bufpool4[(ALIGN_POOL(GKI_BUF4_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF4_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 5)
    UINT8 bufpool5[(ALIGN_POOL(GKI_BUF5_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF5_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 6)
    UINT8 bufpool6[(ALIGN_POOL(GKI_BUF6_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF6_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 7)
    UINT8 bufpool7[(ALIGN_POOL(GKI_BUF7_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF7_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 8)
    UINT8 bufpool8[(ALIGN_POOL(GKI_BUF8_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF8_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 9)
    UINT8 bufpool9[(ALIGN_POOL(GKI_BUF9_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF9_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 10)
    UINT8 bufpool10[(ALIGN_POOL(GKI_BUF10_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF10_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 11)
    UINT8 bufpool11[(ALIGN_POOL(GKI_BUF11_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF11_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 12)
    UINT8 bufpool12[(ALIGN_POOL(GKI_BUF12_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF12_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 13)
    UINT8 bufpool13[(ALIGN_POOL(GKI_BUF13_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF13_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 14)
    UINT8 bufpool14[(ALIGN_POOL(GKI_BUF14_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF14_MAX];
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 15)
    UINT8 bufpool15[(ALIGN_POOL(GKI_BUF15_SIZE) + BUFFER_PADDING_SIZE) * GKI_BUF15_MAX];
#endif

#else
/* Definitions for dynamic buffer use */
#if (GKI_NUM_FIXED_BUF_POOLS > 0)
    UINT8 *bufpool0;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 1)
    UINT8 *bufpool1;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 2)
    UINT8 *bufpool2;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 3)
    UINT8 *bufpool3;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 4)
    UINT8 *bufpool4;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 5)
    UINT8 *bufpool5;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 6)
    UINT8 *bufpool6;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 7)
    UINT8 *bufpool7;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 8)
    UINT8 *bufpool8;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 9)
    UINT8 *bufpool9;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 10)
    UINT8 *bufpool10;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 11)
    UINT8 *bufpool11;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 12)
    UINT8 *bufpool12;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 13)
    UINT8 *bufpool13;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 14)
    UINT8 *bufpool14;
#endif

#if (GKI_NUM_FIXED_BUF_POOLS > 15)
    UINT8 *bufpool15;
#endif

#endif

    UINT8  *OSStack[GKI_MAX_TASKS];         /* pointer to beginning of stack */
    UINT16  OSStackSize[GKI_MAX_TASKS];     /* stack size available to each task */


    INT8   *OSTName[GKI_MAX_TASKS];         /* name of the task */

    UINT8   OSRdyTbl[GKI_MAX_TASKS];        /* current state of the task */
    UINT16  OSWaitEvt[GKI_MAX_TASKS];       /* events that have to be processed by the task */
    UINT16  OSWaitForEvt[GKI_MAX_TASKS];    /* events the task is waiting for*/

    UINT32  OSTicks;                        /* system ticks from start */
    UINT32  OSIdleCnt;                      /* idle counter */
    INT16   OSDisableNesting;               /* counter to keep track of interrupt disable nesting */
    INT16   OSLockNesting;                  /* counter to keep track of sched lock nesting */
    INT16   OSIntNesting;                   /* counter to keep track of interrupt nesting */

    /* Timer related variables
    */
    INT32   OSTicksTilExp;      /* Number of ticks till next timer expires */
#if (defined(GKI_DELAY_STOP_SYS_TICK) && (GKI_DELAY_STOP_SYS_TICK > 0))
    UINT32  OSTicksTilStop;     /* inactivity delay timer; OS Ticks till stopping system tick */
#endif
    INT32   OSNumOrigTicks;     /* Number of ticks between last timer expiration to the next one */

    INT32   OSWaitTmr   [GKI_MAX_TASKS];  /* ticks the task has to wait, for specific events */

    /* Only take up space timers used in the system (GKI_NUM_TIMERS defined in target.h) */
#if (GKI_NUM_TIMERS > 0)
    INT32   OSTaskTmr0  [GKI_MAX_TASKS];
    INT32   OSTaskTmr0R [GKI_MAX_TASKS];
#endif

#if (GKI_NUM_TIMERS > 1)
    INT32   OSTaskTmr1  [GKI_MAX_TASKS];
    INT32   OSTaskTmr1R [GKI_MAX_TASKS];
#endif

#if (GKI_NUM_TIMERS > 2)
    INT32   OSTaskTmr2  [GKI_MAX_TASKS];
    INT32   OSTaskTmr2R [GKI_MAX_TASKS];
#endif

#if (GKI_NUM_TIMERS > 3)
    INT32   OSTaskTmr3  [GKI_MAX_TASKS];
    INT32   OSTaskTmr3R [GKI_MAX_TASKS];
#endif



    /* Buffer related variables
    */
    BUFFER_HDR_T    *OSTaskQFirst[GKI_MAX_TASKS][NUM_TASK_MBOX]; /* array of pointers to the first event in the task mailbox */
    BUFFER_HDR_T    *OSTaskQLast [GKI_MAX_TASKS][NUM_TASK_MBOX]; /* array of pointers to the last event in the task mailbox */

    /* Define the buffer pool management variables
    */
    FREE_QUEUE_T    freeq[GKI_NUM_TOTAL_BUF_POOLS];

    UINT16   pool_buf_size[GKI_NUM_TOTAL_BUF_POOLS];
    UINT16   pool_max_count[GKI_NUM_TOTAL_BUF_POOLS];
    UINT16   pool_additions[GKI_NUM_TOTAL_BUF_POOLS];

    /* Define the buffer pool start addresses
    */
    UINT8   *pool_start[GKI_NUM_TOTAL_BUF_POOLS];   /* array of pointers to the start of each buffer pool */
    UINT8   *pool_end[GKI_NUM_TOTAL_BUF_POOLS];     /* array of pointers to the end of each buffer pool */
    UINT16   pool_size[GKI_NUM_TOTAL_BUF_POOLS];    /* actual size of the buffers in a pool */

    /* Define the buffer pool access control variables */
    void        *p_user_mempool;                    /* User O/S memory pool */
    UINT16      pool_access_mask;                   /* Bits are set if the corresponding buffer pool is a restricted pool */
    UINT8       pool_list[GKI_NUM_TOTAL_BUF_POOLS]; /* buffer pools arranged in the order of size */
    UINT8       curr_total_no_of_pools;             /* number of fixed buf pools + current number of dynamic pools */

    BOOLEAN     timer_nesting;                      /* flag to prevent timer interrupt nesting */

    /* Time queue arrays */
    TIMER_LIST_Q *timer_queues[GKI_MAX_TIMER_QUEUES];
    /* System tick callback */
    SYSTEM_TICK_CBACK *p_tick_cb;
    BOOLEAN     system_tick_running;                /* TRUE if system tick is running. Valid only if p_tick_cb is not NULL */

#if (GKI_DEBUG == TRUE)
    UINT16      ExceptionCnt;                       /* number of GKI exceptions that have happened */
    EXCEPTION_T Exception[GKI_MAX_EXCEPTION];
#endif

} tGKI_COM_CB;


#ifdef __cplusplus
extern "C" {
#endif

/* Internal GKI function prototypes
*/
GKI_API extern BOOLEAN   gki_chk_buf_damage(void *);
extern BOOLEAN   gki_chk_buf_owner(void *);
extern void      gki_buffer_init (void);
extern void      gki_timers_init(void);
extern void      gki_adjust_timer_count (INT32);

extern void    OSStartRdy(void);
extern void    OSCtxSw(void);
extern void    OSIntCtxSw(void);
extern void    OSSched(void);
extern void    OSIntEnter(void);
extern void    OSIntExit(void);


/* Debug aids
*/
typedef void  (*FP_PRINT)(char *, ...);

#if (GKI_DEBUG == TRUE)

typedef void  (*PKT_PRINT)(UINT8 *, UINT16);

extern void gki_print_task(FP_PRINT);
extern void gki_print_exception(FP_PRINT);
extern void gki_print_timer(FP_PRINT);
extern void gki_print_stack(FP_PRINT);
extern void gki_print_buffer(FP_PRINT);
extern void gki_print_buffer_statistics(FP_PRINT, INT16);
GKI_API extern void gki_print_used_bufs (FP_PRINT, UINT8);
extern void gki_dump(UINT8 *, UINT16, FP_PRINT);
extern void gki_dump2(UINT16 *, UINT16, FP_PRINT);
extern void gki_dump4(UINT32 *, UINT16, FP_PRINT);

#endif
#ifdef __cplusplus
}
#endif

#endif
