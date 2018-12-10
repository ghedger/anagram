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

#ifndef _ANAGRAM_LOG_H_
#define _ANAGRAM_LOG_H_

enum _LOG_LEVEL {
  LOG_NONE = 0,
  LOG_NORMAL,
  LOG_INFO,
  LOG_DEBUG
};

typedef _LOG_LEVEL LOG_LEVEL;

LOG_LEVEL getVerbosity();
void setVerbosity( LOG_LEVEL );

// TODO: Make these inline?  Trouble is, "a" can be
//  "Reading " << lineTot << " words." << endl
// Circle back to this later...
#define VERBOSE_LOG(lev,a) if(getVerbosity() >= lev) {std::cout << a;std::flush(std::cout);}
#define SET_VERBOSITY_LEVEL(a) setVerbosity(a)
#define GET_LOG_VERBOSITY() (getVerbosity())

#endif // #ifdef _ANAGRAM_LOG_H
