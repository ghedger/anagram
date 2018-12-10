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

#ifndef _ANAGRAM_LOCK_H_
#define _ANAGRAM_LOCK_H_

#include <sched.h>

namespace anagram {

// Lock
// Simple lock class for acquisition and release of lock
class Lock {
 public:
  Lock() { lock_ = 0; }
  ~Lock() {}
  // Acquire
  inline void Acquire() {
    #if 1
    while (lock_) {
      // Occupado, sister; Force a context switch and wait our turn
      // (TODO: Is this even necessary?)
      sched_yield();
    }
    __sync_lock_test_and_set(&lock_, 1);
    #else
    // Low-level lock on Intel processors
    #endif
  }
  // Release
  inline void Release() {
    // Low-level release on Intel chips
    #if 1
    lock_ = 0;
    #else
    __sync_lock_release(&lock_);
    #endif
  }
 private:
  volatile int lock_;
};
} // namespace anagram
#endif // #ifndef _ANAGRAM_LOCK_H_

