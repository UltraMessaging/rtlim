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

You can use the rtlim API to control the rate at which your
application performs some operation.
The original motivation for this module was to add rate control
functionality to
[Ultra Messaging Smart Sources](https://ultramessaging.github.io/currdoc/doc/Design/advancedoptimizations.html#smartsources_),
but the code is not specific to messaging and can be used to control
the rate of any desired operation.

The reason for limiting messaging publishing rates is to avoid
[packet loss](https://ultramessaging.github.io/currdoc/doc/Design/packetloss.html).
It is primarily for UDP-based messaging where sending too fast can
fill a buffer somewhere on its way to the subscriber,
resulting in drops.


## General Approach

There are many ways to limit the publishing rate;
see [Alternative Algorithms](#alternative-algorithms).
The method used here is a
[Token Bucket algorithm](https://en.wikipedia.org/wiki/Token_bucket),
similar to the one used by Ultra Messaging.
One important difference:
instead of using an asynchronous timer to refill the tokens,
it is done opportunistically by the sender.

The rtlim C API consists of an object that is created,
and then used to control the rate of sending messages
(or any arbitrary operation).

To use an rtlim object, you "take" one or more tokens from it before
the operation you want to be controlled.
If you are within the allowed rate,
the "take" function returns right away.
If you are exceeding the rate limit and blocking mode is selected,
the "take" function delays for enough time to bring you into
compliance.
When the "take" function returns, you may perform the operation.


## Reference

````
rtlim_t *
rtlim_create(unsigned long long refill_interval_ns, int refill_token_amount);
````
Where:
* refill_interval_ns - time resolution for rate limiter.
* refill_token_amount - number of tokens available in each interval.

Returns pointer to rtlim object.

rtlim_create() creates a rate limiter object.

---
````
void
rtlim_delete(rtlim_t *rtlim);
````
Where:
* rtlim - rate limiter object (previously returned by rtlim_create()).

rtlim_delete() deletes a rate limiter object.

---
````
int
rtlim_take(rtlim_t *rtlim, int take_token_amount, int block);
````
Where:
* rtlim - rate limiter object (previously returned by rtlim_create()).
* take_token_amount - number of tokens needed.
* block - one of RTLIM_NON_BLOCK, RTLIM_BLOCK_SPIN, or RTLIM_BLOCK_SLEEP.

Returns status code where:
* 0 = Success.
* -1 = Failed due to insufficient tokens available and RTLIM_NON_BLOCK
was specified.
* -2 = Failed due to requesting more tokens than refill_token_amount and RTLIM_NON_BLOCK was specified.
There can never be more than refill_token_amount tokens,
so trying to take more than that with non-blocking can never succeed.

If the number of tokens needed can be satisfied,
rtlim_take() decrements its internal token counter and returns
immediately.

If the number of tokens needed cannot be satisfied,
rtlim_take()'s behavior depends on the "block" parameter:
* RTLIM_NON_BLOCK - rtlim_take() returns immediately with a
negative status to indicate insufficient tokens available.
* RTLIM_BLOCK_SPIN - rtlim_take() delays by busy looping
until enough tokens are earned to satisfy the need.
* RTLIM_BLOCK_SLEEP - rtlim_take() delays by calling
select() to sleep until enough tokens are earned to satisfy the need.
This has the advantage of allowing other threads to use the CPU
during its sleep time, but the disadvantage of being much less
deterministic as to when it wakes up.
The select() function produces highly variable sleep times
for small time values (FYI the same is true for usleep()).
It might sleep significantly longer than the minimum time required,
resulting in lower throughput and higher latencies than necessary.


## Example

Create a rate limiter.
````
  rtlim_t *rtlim;
  rtlim = rtlim_create(1000000, 50);
````

The interval is set to 1 millisecond (1000000 nanoseconds).
The number of tokens is 50.

For example, you can send in a tight loop at the desired rate of
50 messages every millisecond:
````
  while (1) {
    rtlim_take(rtlim, 1, RTLIM_BLOCK_SPIN);
    lbm_ssrc_send_ex(...);
  }
````
Note that it specified RTLIM_BLOCK_SPIN.
If a delay is needed, rtlim will busy-loop for the amount of time required
to add more tokens to satisfy the rtlim_take().

Let's say that each lbm_ssrc_send_ex() takes 2 microseconds.
The first 50 times around the loop will execute at full speed,
taking a total of 100 microseconds.
The 51st call to rtlim_take() will delay 900 microseconds.
The 52nd through 100th loops are full speed. The 101st call
delays 900 microseconds.
Thus, the message send rate is 50 messages per millisecond,
or 50,000 mssages per second.

Compare this to an interval of 10 milliseconds and a refill token
amount of 500:
````
  rtlim = rtlim_create(10000000, 500);
````

The same loop above will run at full speed for 500 messages,
taking a total of 1 millisecond,
and the 501st message will delay 9 milliseconds,
for an average of 500 messages per 10 milliseconds,
or 50,000 messages per second.

Over a full second, the two averages are the same.
But the second one allows much more intense short-term bursts,
and can impose a much longer delay.

The goal of using a longer interval is to avoid latency when bursts are
needed,
but care must be taken not to allow a burst so intense that it causes loss.
That is, only delay the sender when the alternative (loss) is worse.

To see more example usages that demonstrate its features and behavior,
see the "main()" function inside "rtlim.c".
(Note: the "main()" function is conditionally compiled only if
"-DSELFTEST" is specified.)


## Taking More than One Token

The previous example limited the number of messages sent per millisecond,
regardless of the message size.
The assumption is that the sizes of the messages will be close to the
same (or at least will require about the same number of packets).

For applications that send messages with widely varying numbers of
packets each,
you might want a large message to consume more tokens than a small message.

````
  /* Assume 1300 bytes of user data par packet. */
  approx_num_packets = (message_size / 1300) + 1;
  rtlim_take(rtlim, approx_num_packets, RTLIM_BLOCK_SPIN);
````
Note that calculating the number of packets required by Ultra
Messaging for a given message size can be difficult and depends
on configuration.


## Limitations

The rtlim code is not thread-safe.
If multiple threads will be taking tokens from a single rtlim object,
a mutex lock will have to be added.

But note that the original motivation for this rate limiter was for
use with Smart Sources, which are also not thread-safe.
So I would expect the user to solve both external to the
rtlim module.


## Building and Testing

The rtlim.c file is straightforward and has no external dependencies
except for the provided "rtlim.h" header file.
The intent is that the source code be incorporated directly into
your application.

Alternatively, "rtlim.c" can be compiled to object and linked into
your application.
Application source files should include "rtlim.h".

Finally, the module includes a "main()" which performs a self-test.
To enable the self-test "main()", compile with the
"-DSELFTEST" directive.
See "tst.sh" for a script that compiles and runs the test.


## Porting to Windows

The module makes use of Unix's "clock_gettime()" function to get
a nanosecond-precision, monotonically increasing time.
For suggestions porting it to windows, see
[porting-clock-gettime-to-windows](https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows).

Also, RTLIM_BLOCK_SLEEP mode relies on usleep(), which has microsecond precision.
Windows' Sleep() function has millisecond precision.


## Alternative Algorithms

One problem with this approach is selecting the sending rate limit.
You want to set the rate limit as high as possible to avoid
unnecessary latency,
but not so high that you risk dropping data due to overload.
It typically requires experimental testing to determine the best
rate limit.
And even then, a change in receiver timing could mess things up.

It would be nice if the system itself could determine the correct
automatically and dynamically.


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

