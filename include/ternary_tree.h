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

#ifndef _TERNARY_TREE_H
#define _TERNARY_TREE_H

#include <map>
#include <string>
#include <queue>
#include <deque>
#include <stack>
#include "templ_node.h"

typedef unsigned char UCHAR;

//#define DEBUG
#define INFO

// TNode
// This is the instantiable class from my TemplNode template
class TNode : public TemplNode <UCHAR, TNode> {
 public:
  TNode() {};
  TNode(UCHAR key) : TemplNode <UCHAR, TNode> (key) { };
  ~TNode() {};
  void SetKey(UCHAR key)
  {
    if(isupper(key) )
    {
      SetUpper();
    }
    key_ = (UCHAR) tolower(key);
  }
};

// TernaryTree
// This class is the tree itself. It manages a ternary search tree of TNodes
// This is a dictionary and allows us quick word lookup
// with a yes/no response.
class TernaryTree {
 public:
  TernaryTree() {
    tie_hwm_ = 0;
    max_diff_ = 10;
  };
  ~TernaryTree() {};
  TNode * Insert(const char *pWord, TNode **ppNode = NULL);
  bool Find(const char *pWord, TNode *pParent, TNode ** ppTerminal = NULL);
  void FuzzyFind(
    const char *pWord,
    TNode *pParent,
    std::map< int, std::string > *pWords);
  bool ExtrapolateAll(
    TNode *pNode,
    std::map< int, std::string > *pWords,
    std::deque< UCHAR > *accum,
    const char *pStem,
    const char *pWord
    );
  bool Extrapolate(
    TNode *pRoot,
    TNode *pNode,
    std::map< int, std::string > *pWords,
    std::deque< UCHAR > *accum,
    const char *pStem,
    const char *pWord,
    std::map< int, int > *tie_breaker_lookup,
    const int max_diff = 0,
    int depth = 0);

 int GetMaxTies() { return tie_hwm_; }
 void ClearMaxTies() { tie_hwm_ = 0; }
 void SetMaxDifference(int max) { max_diff_ = max; }
 int GetMaxDifference() { return max_diff_; }
 protected:
  TNode *AllocNode(char key);
  int CalcLevenshtein(const char *s1, const char *s2);

  // member variables
  int tie_hwm_;
  int max_diff_;
};

#endif // #ifndef _TERNARY_TREE_H

