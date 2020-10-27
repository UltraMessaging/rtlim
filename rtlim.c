/* rtlim.c */
/*
 * Copyright (c) 2020 Informatica Corporation. All Rights Reserved.
 * Permission is granted to licensees to use
 * or alter this software for any purpose, including commercial applications,
 * according to the terms laid out in the Software License Agreement.
 *
 * This source code example is provided by Informatica for educational
 * and evaluation purposes only.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 * NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR
 * PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE
 * UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES,
 * BE LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
 * INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
 * TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF
 * THE LIKELIHOOD OF SUCH DAMAGES.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>


/* Primitive error handling. */
#define NULLCHK(ptr_) do { \
  if ((ptr_) == NULL) { \
    fprintf(stderr, "Null pointer error at %s:%d '%s'\n", \
      __FILE__, __LINE__, #ptr_); \
    fflush(stderr); \
    exit(1); \
  } \
} while (0);
#define FAILCHK(status_) do { \
  if ((status_) == -1) { \
    perror("Failed status"); \
    fprintf(stderr, "Failure at %s:%d '%s'\n", \
      __FILE__, __LINE__, #status_); \
    fflush(stderr); \
    exit(1); \
  } \
} while (0);


typedef struct rtlim_s {
  unsigned long long refill_interval_ns;
  unsigned long long last_refill_ns;
  unsigned long long cur_ns;
  unsigned long long refill_token_amount;
  int current_tokens;
} rtlim_t;

#define BLOCKING 1
#define NON_BLOCKING 2


unsigned long long current_time_ns()
{
  struct timespec cur_timespec;
  int status;
  unsigned long long rtn_time;

  status = clock_gettime(CLOCK_MONOTONIC, &cur_timespec);
  FAILCHK(status);

  rtn_time = (unsigned long long)cur_timespec.tv_sec * 1000000000ll +
    (unsigned long long)cur_timespec.tv_nsec;

  return rtn_time;
}  /* current_time_ns */


rtlim_t *rtlim_create(unsigned long long refill_interval_ns, int refill_token_amount)
{
  rtlim_t *rtlim;

  rtlim = (rtlim_t *)malloc(sizeof(rtlim_t));
  NULLCHK(rtlim);

  rtlim->refill_interval_ns = refill_interval_ns;
  rtlim->refill_token_amount = refill_token_amount;
  rtlim->current_tokens = refill_token_amount;  /* Fill rate limiter. */
  rtlim->last_refill_ns = current_time_ns();

  return rtlim;
}  /* rtlim_create */


void rtlim_delete(rtlim_t *rtlim)
{
  free(rtlim);
}  /* rtlim_delete */


/* Returns:
 *    0 for success,
 *   -1 for tokens not availale, -2.
 *   -2 for non-blocking request for more tokens than refill amount.
 */
int rtlim_take(rtlim_t *rtlim, int take_token_amount, int block)
{
  if ((block == NON_BLOCKING) && take_token_amount > rtlim->refill_token_amount) {
    return -2;
  }

  do {
    rtlim->cur_ns = current_time_ns();

    /* Has an interval of time passed since the last refill? */
    if (rtlim->cur_ns >= rtlim->last_refill_ns + rtlim->refill_interval_ns) {
      rtlim->current_tokens = rtlim->refill_token_amount;  /* refill */
      rtlim->last_refill_ns = rtlim->cur_ns;
    }

    /* Does rate limiter have enough tokens for request? */
    if (take_token_amount <= rtlim->current_tokens) {
      rtlim->current_tokens -= take_token_amount;  /* Take tokens. */
      take_token_amount = 0;
    }
    else {  /* Not enoough available tokens. */
      if (block == BLOCKING) {
        /* for blocking behavior, take all available tokens and wait for more. */
        take_token_amount -= rtlim->current_tokens;
        rtlim->current_tokens = 0;
      }
      else {
        /* Non-blocking, not enough tokens. */
        return -1;
      }
    }
  } while (take_token_amount > 0);

  return 0;
}  /* rtlim_take */


/************************ Test code *************************/
#ifdef SELFTEST

#define EQUALCHK(val_,chk_) do { \
  unsigned long long inval_ = (unsigned long long)(val_); \
  unsigned long long inchk_ = (unsigned long long)(chk_); \
  if (inval_ != inchk_) { \
    fprintf(stderr, "Equal check failed at %s:%d, %s=%llu, %s=%llu\n", \
      __FILE__, __LINE__, #val_, inval_, #chk_, inchk_); \
    fflush(stderr); \
    exit(1); \
  } \
} while (0)

#define APPROXCHK(val_,chk_) do { \
  unsigned long long inval_ = (unsigned long long)(val_); \
  unsigned long long inchk_ = (unsigned long long)(chk_); \
  unsigned long long chkhi_ = inchk_ + (inchk_ / 50); /* 2% higher. */ \
  unsigned long long chklo_ = inchk_ - (inchk_ / 50); /* 2% lower. */ \
  if ((inval_ > chkhi_) || (inval_ < chklo_)) { \
    fprintf(stderr, "Approx check failed at %s:%d, %s=%llu, %s=%llu (%llu..%llu)\n", \
      __FILE__, __LINE__, #val_, inval_, #chk_, inchk_, chklo_, chkhi_); \
    fflush(stderr); \
    exit(1); \
  } \
} while (0)

int main(int argc, char **argv)
{
  rtlim_t *rl;
  int status;
  unsigned long long start_time;

  rl = rtlim_create(500000000, 100);  /* Half second */

  /* Error: taking more tokens than it refills to with a non-blocking take. */
  start_time = current_time_ns();
  status = rtlim_take(rl, 200, NON_BLOCKING);
  EQUALCHK(status, -2);  /* make sure it failed. */
  APPROXCHK(current_time_ns(), start_time);  /* no time delay. */

  /* Success: take 2 intervals worth (takes a full second). */
  start_time = current_time_ns();
  status = rtlim_take(rl, 200, BLOCKING);
  EQUALCHK(status, 0);
  APPROXCHK(current_time_ns(), start_time + 1000000000);  /* 1 sec. */
  EQUALCHK(rl->current_tokens, 0);

  /* Sleep less than the refill interval and nonblock for 100. Should fail. */
  usleep(400000);  /* .4 sec */
  start_time = current_time_ns();
  status = rtlim_take(rl, 100, NON_BLOCKING);
  EQUALCHK(status, -1);  /* make sure it failed. */
  APPROXCHK(current_time_ns(), start_time);  /* no time delay. */
  EQUALCHK(rl->current_tokens, 0);

  /* Sleep past than the refill interval and nonblock for 100. Should be OK. */
  usleep(200000);  /* .2 sec */
  start_time = current_time_ns();
  status = rtlim_take(rl, 100, NON_BLOCKING);
  EQUALCHK(status, 0);  /* Success. */
  APPROXCHK(current_time_ns(), start_time);  /* no time delay. */
  EQUALCHK(rl->current_tokens, 0);

  /* Sleep past than the refill interval and nonblock for 80. Should be OK. */
  usleep(600000);  /* .6 sec */
  start_time = current_time_ns();
  status = rtlim_take(rl, 80, BLOCKING);
  EQUALCHK(status, 0);  /* Success. */
  APPROXCHK(current_time_ns(), start_time);  /* no time delay. */
  EQUALCHK(rl->current_tokens, 20);

  /* Block for 80 more. */
  start_time = current_time_ns();
  status = rtlim_take(rl, 80, BLOCKING);
  EQUALCHK(status, 0);  /* Success. */
  APPROXCHK(current_time_ns(), start_time + 500000000);  /* .5 sec. */
  EQUALCHK(rl->current_tokens, 40);

  /* Block for 400 more. */
  start_time = current_time_ns();
  status = rtlim_take(rl, 400, BLOCKING);
  EQUALCHK(status, 0);  /* Success. */
  APPROXCHK(current_time_ns(), start_time + 2000000000);  /* 2 seconds. */
  EQUALCHK(rl->current_tokens, 40);

  rtlim_delete(rl);

  printf("OK\n");

  return 0;
}  /* main */

#endif
