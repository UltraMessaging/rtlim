/* rtlim.h */
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

#ifndef RTLIM_H
#define RTLIM_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


typedef struct rtlim_s {
  unsigned long long refill_interval_ns;
  unsigned long long last_refill_ns;
  unsigned long long cur_ns;
  unsigned long long refill_token_amount;
  int current_tokens;
} rtlim_t;

#define RTLIM_BLOCKING 1
#define RTLIM_NON_BLOCKING 2


unsigned long long current_time_ns();
rtlim_t *rtlim_create(unsigned long long refill_interval_ns, int refill_token_amount);
void rtlim_delete(rtlim_t *rtlim);
int rtlim_take(rtlim_t *rtlim, int take_token_amount, int block);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif  /* RTLIM_H */
