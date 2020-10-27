# rtlim
Example code for a rate limiter.

## License

Copyright (c) 2020 Informatica Corporation.
Permission is granted to licensees to use or alter this software for any
purpose, including commercial applications,
according to the terms laid out in the Software License Agreement.

This source code example is provided by Informatica for educational
and evaluation purposes only.

THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR
PURPOSE. INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE
UNINTERRUPTED OR ERROR-FREE. INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE
LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF
THE LIKELIHOOD OF SUCH DAMAGES.


## Introduction

The primary intended application of this example code is to have a messaging
publisher limit the rate at which it publishes.
However, the code was written to be general to allow other applications.

The purpose of limiting messaging publishing rates is to avoid loss.
It is primarily for UDP-based messaging where sending too fast can
fill a buffer somewhere on its way to the subscriber,
resulting in drops.

There are many ways to limit the publishing rate;
see [Alternatives](#alternatives).
The method used here is a
[Token Bucket algorithm](https://en.wikipedia.org/wiki/Token_bucket),
similar to the one used by Ultra Messaging.
One important difference: instead of using an asynchronous timer to
refill the "bucket",
it is done opportunistically by the sender.


## General Approach

The rtlim C API consists of an object that is created,
and then used to control the rate of any arbitrary operation.
The motivation is to control the rate of sending messages,
but it could be anything.

To use an rtlim object, you "take" one or more tokens from it before
the operation you want to be controlled.
If you are within the allowed rate,
the "take" function returns right away.
If you are exceeding the rate limit and blocking mode is selected,
the "take" function busy loops for enough time to bring you into
compliance.
When the "take" function returns, you may perform the operation.

There's also a non-blocking mode which returns an error code instead
of busy looping when you exceed the limit.


## Details

When the rtlim object is created, the application passes two parameters:
* refill_interval_ns - time resolution for rate limiter.
* refill_token_amount - number of tokens available in each interval.

For example:

````
  rtlim_t *rtlim;
  rtlim = rtlim_create(1000000, 50);
````

The interval is set to 1 millisecond (1000000 nanoseconds).
The number of tokens is 50.

````
  while (1) {
    rtlim_take(rtlim, 1, 1);
    send_message(...);
  }
````

Let's say that each send_message() takes 2 microseconds.
The first 50 times around the loop will execute at full speed,
taking 100 microseconds.
The 51st call to rtlim_take() will delay 900 microseconds.
The 52nd through 100th loops are full speed. The 101st call
delays 900 microseconds.
Thus, the message send rate is 50 messages per millisecond when
averaged over the entire millisecond.


## Taking more than one token

The previous example limited the number of messages sent per millisecond,
regardless of the message size.
The assumption is that the sizes of the messages will be close to the
same (or at least will require about the same number of packets).

For applications that send messages with widely varying numbers of
packets each,
you might want a large message to consume more tokens than a small message.

````
  approx_num_packets = (message_size / 1200) + 1;
  rtlim_take(rtlim, approx_num_packets, 1);
````


## Alternatives

One problem with this approach is selecting the sending rate limit.
You want to set the rate limit as high as possible to avoid
unnecessary latency,
but not so high that you risk dropping data due to overload.
It typically requires experimental testing to determine the best
rate limit.
It would be nice if the system itself could determine.


### Sliding Windows

The TCP protocol has had decades of research and development into
loss avoidance algorithms
(sliding window, slow start, congestion window, etc.).
The idea is that you limit the number of in-flight packets using
acknowledgments from the receiver(s).
This makes the connection receiver-paced.

But for low-latency, high-fanout applications using multicast,
getting ACKs back from receivers doesn't scale well and can introduce
latency outliers as the processing of acknowledgments contend with
the processing of newly-sent messages,
and that contention leads to latency outliers.
This is why low-latency multicast protocols are NAK-based,
and don't have receiver pacing.

Also, receiver pacing is not always the desired behavior.
For many applications, receivers are expected to keep up with an
agreed-upon rate.
Receivers that can't keep up *should* get loss.
The rate limit is used to ensure the publisher doesn't exceed the
agreed-upon rate.


### Adaptive Algorithms

An adaptive algorithm might slowly increase the sending rate until
congestion or loss happens, and then back off.
It can produce a sawtooth-shaped graph of sending rate,
oscillating around the maximum sustainable send rate.

However, ACKs from receivers is generally required to detect congestion,
which is usually bad for multicast protocols,
and loss introduces significant latency outlier for NAK/retransmission.
