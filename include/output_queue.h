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

#ifndef _OUTPUT_QUEUE_H
#define _OUTPUT_QUEUE_H

#include "anagram_lock.h"

namespace anagram {

const int kOutputQueueSize = 256;
const int kOutputQueueItemSize = 256;

// OutputQueue
// This class buffers the console output to a single thread.  This helps prevent
// unwanted output artifacts such as missed carriage returns from text sent to
// stdout from mulitple threads.  Simply disabling buffering or setting line mode
// with setbuf(stdout, NULL) or setlinebuf(stdout) is insufficient.
//
// It implements a fixed circular buffer of fixed item size for storing text
// data designated for stdout.
//
// OutputQueue starts its own worker thread to service the queue at regular
// polled intervals.
//
// It is designed for a pure producer-consumer model in that threads adding
// strings for output should not also remove them; they are to be removed only
// by the worker thread.
// It is scope-coincident, with the thread being terminated
// when the function goes out of scope.
class OutputQueue {
 public:
  OutputQueue();
  ~OutputQueue();
  void Push(const char *);
  const char *Pop();
  void AcquireLock();
  void ReleaseLock();
 private:
  void StartThread();
  void Terminate();
  static void *Worker(void *);
  size_t GetItemTot();
  // Queue variables
  char *        queue_;
  Lock          queue_lock_;
  size_t        queue_start_index_; // start index (last in line, index to add)
  size_t        queue_end_index_;   // end index (first in line)
  size_t        queue_size_;        // size in items
  size_t        queue_item_size_;
  // Thread variables
  static volatile bool  terminate_;
  static volatile bool  terminated_;
  static pthread_t *    pthread_;
};
} // namespace anagram

#endif // #ifndef _OUTPUT_QUEUE_H
