/* MIT License
 *
 * Copyright (c) 2018 Greg Hedger
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <thread>    // TODO: Move to boost threads for OS independence
#include <cstring>
#include <sched.h>

#include "anagram_lock.h"
#include "output_queue.h"
#include "anagram_log.h"

namespace anagram {

volatile bool OutputQueue::terminate_;
volatile bool OutputQueue::terminated_;
pthread_t OutputQueue::pthread_;

// Constructor
// This will start a service thread that runs on a timer and processes the
// output queue at regular intervals.
OutputQueue::OutputQueue()
{
  // Allocate the queue memory itself.  For simplicity, items are fixed size.
  queue_size_ = kOutputQueueSize;
  queue_item_size_ = kOutputQueueItemSize;
  queue_start_index_ = queue_end_index_ = 0;
  queue_ = new char[queue_size_ * queue_item_size_];

  // Start the service thread
  StartThread();
}

// Destructor
// OutputQueue is responsible for shutting down its service thread and
// cleanup up any allocated resources
OutputQueue::~OutputQueue()
{
  TerminateThread();
}

// StartThread
// This initializes and starts the service thread.
void OutputQueue::StartThread()
{
  terminate_ = false;
  terminated_ = false;

  int error = 0;
  error = pthread_create(
    &pthread_,
    nullptr,
    &OutputQueue::Worker,
    (void *)(this)    // <- This is the thread's link to the outside world.
  );
  if (error) {
    // TODO: This is highly problematic if some threads were created!
    // Want to look at pthread_cancel with cleanup handlers.
    VERBOSE_LOG(LOG_NONE, "Thread creation error" << std::endl);
  }
  return;
}

// TerminateThread
void OutputQueue::TerminateThread()
{
  terminate_ = true;
  void *result;
  pthread_join(OutputQueue::pthread_,&result);
}

// AcquireLock
// Acquires lock on the queue
void OutputQueue::AcquireLock()
{
  queue_lock_.Acquire();
}

// ReleaseLock
// Releases lock on the queue
void OutputQueue::ReleaseLock()
{
  queue_lock_.Release();
}

// Pop
// Remove the next item in FIFO order.
// Exit: pointer to string, or nullptr if none.
// NOTE: Designed for pure producer-consumer implmenetation; locks
// assumed to be applied from consuming caller, so not used here.
const char *OutputQueue::Pop()
{
  const char * item = nullptr;
  if (GetItemTot() ) {    // items?
    item = queue_ + queue_end_index_ * queue_item_size_;
    if (++queue_end_index_ >= queue_size_) {
      queue_end_index_ = 0;
    }
  }
  return item;
}

// Push
// Remove the next item in FIFO order.
// Exit: pointer to string, or nullptr if none.
// NOTE: string length must be 256 or less.
void OutputQueue::Push(const char *text)
{
  // We will BLOCK if necessary until we can write to the queue.
  while (GetItemTot() == queue_size_ - 1) {
    sched_yield();
  }
  queue_lock_.Acquire();  // Atomic operation - one at a time, so lock.

  // Need to copy the text because the source may go out of scope
  // by the time the worker thread processes it.
  if (text) {
    char *loc = queue_ + queue_start_index_ * kOutputQueueItemSize;
    std::strncpy(loc, text, kOutputQueueItemSize);
  }

  // This will bump the queue index accounting for wrapping.
  if (++queue_start_index_ >= queue_size_) {
    queue_start_index_ = 0;
  }

  queue_lock_.Release();
}

// GetItemTot
// Returns the # of items currently awaiting processing in the queue.
// Exit: # of items in queue
size_t OutputQueue::GetItemTot()
{
  size_t tot = queue_start_index_ - queue_end_index_;
  // tot is unsigned; overflow will result in a huge #, hence >
  while (tot > queue_size_) {
    tot += queue_size_;
  }
  return tot;
}

// Worker
// This is the service thread.  It sits in a loop and polls for any queue
// items, spitting them out to the output stream in order.
void *OutputQueue::Worker(void *params)
{
  OutputQueue *out_queue = (OutputQueue *) params;

  VERBOSE_LOG(LOG_INFO, "QUEUE THREAD STARTING..." << std::endl);
  // This waits and processes the output queue at regular intervals.
  while (!OutputQueue::terminate_) {
    // Go to sleep for a time (2ms)
    struct timespec ts = {0, 2000000L };
    nanosleep(&ts, NULL);

    // Up and at 'em... process any queue items...
    const char *output_string;
    out_queue->AcquireLock();   // Disallow operations on queue
    while (nullptr != (output_string = out_queue->Pop())) {
      std::cout << output_string;   // note, no endl or other decoration
    }
    out_queue->ReleaseLock();   // Re-allow queue ops
  }
  VERBOSE_LOG(LOG_INFO, "QUEUE THREAD EXITING..." << std::endl);
  return nullptr;
}
} // namespace anagram
