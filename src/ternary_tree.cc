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
#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <fstream>
#include <string>
#include <map>
#include <set>
#include <stack>
#include <queue>
#include <deque>
#include "templ_node.h"
#include "ternary_tree.h"
#include "anagram_log.h"

typedef unsigned char UCHAR;

//#define DEBUG
#define INFO

// This class is the tree itself. It manages a ternary search tree of TNodes
// This is a dictionary and allows us quick word lookup
// with a yes/no response.

// InsertNode
// Insert a node into the tree
//
// @In: word pointer to null-terminated string
// ppParent pointer to parent pointer
// @Out: Node *
TNode * TernaryTree::Insert(const char *word, TNode **ppNode)
{
  TNode * pChild = NULL;
  //VERBOSE_LOG(LOG_DEBUG, "Insert >>>>>" << std::endl);
  if (!(*ppNode)) {
    *ppNode = AllocNode(*word);
    //VERBOSE_LOG(LOG_DEBUG, "ALLOC" << std::endl);
  }
  if (tolower(*word) < ((*ppNode)->GetKey())) {
    //VERBOSE_LOG(LOG_DEBUG,  "L: " << word);
    Insert(word, &((*ppNode)->l_));
    (*ppNode)->GetLeft()->SetParent((*ppNode)->GetParent());
  }
  else if (tolower(*word) > (*ppNode)->GetKey()) {
    //VERBOSE_LOG(LOG_DEBUG,  "R: " << word << std::endl);
    // Add a peer on the right
    Insert(word, &((*ppNode)->r_));
    (*ppNode)->GetRight()->SetParent((*ppNode)->GetParent());
  } else {
    // Is this the last letter (is there a char in the second position?)
    if (word[ 1 ])
    {
      //VERBOSE_LOG(LOG_DEBUG,  "C: " << word << std::endl);
      pChild = Insert(word + 1, &((*ppNode)->c_));
      pChild->SetParent(*ppNode);
    }
    else
    {
      // Yep, last letter, so we will set the terminator flag...
      //VERBOSE_LOG(LOG_DEBUG,  "T: " << word << std::endl);
      (*ppNode)->SetTerminator();
    }
  }


  //VERBOSE_LOG(LOG_DEBUG,  "Insert <<<<<" << std::endl);
  /*
  VERBOSE_LOG(LOG_DEBUG,  "" << std::endl);
  VERBOSE_LOG(LOG_DEBUG,  " K: " << (*ppNode)->GetKey());
  VERBOSE_LOG(LOG_DEBUG,  " L: " << (*ppNode)->GetLeft());
  VERBOSE_LOG(LOG_DEBUG,  " C: " << (*ppNode)->GetCenter());
  VERBOSE_LOG(LOG_DEBUG,  " R: " << (*ppNode)->GetRight());
  VERBOSE_LOG(LOG_DEBUG,  " P: " << (*ppNode)->GetParent());
  VERBOSE_LOG(LOG_DEBUG,  std::endl);
  */

  return *ppNode;
};

// Find
// Find a word
//
// @In:     @word pointer to null-terminated string
//          @pParent pointer to current parent node
//          @ppTerminal pointer to terminal node pointer
// @Out:    true == match found
bool TernaryTree::Find(const char *word, TNode *pParent, TNode ** ppTerminal)
{
  bool ret = false;
  if (pParent)
  {
    if ((*word) < pParent->GetKey())
      ret = Find(word, pParent->GetLeft(), ppTerminal);
    else if ((*word) > pParent->GetKey())
      ret = Find(word, pParent->GetRight(), ppTerminal);
    else
    {
      if ('\0' == word[ 1 ])
      {
        ret = pParent->GetTerminator();
        if (ppTerminal) // Mark pointer to node
          *ppTerminal = pParent;
      }
      else
        ret = Find(word + 1, pParent->GetCenter(), ppTerminal);
    }
  }
  return ret;
}

// Perform an inexact, "fuzzy" lookup of a word
//
// @In:     @word pointer to null-terminated string
//          @pParent pointer to current parent node
// @Out:    true == match found
//          @map key/value pair map with tiebroken score and word
void TernaryTree::FuzzyFind(
    const char *word,
    TNode *pParent,
    std::map< int, std::string > *words)
{
  ClearMaxTies();
  std::string search_word = word;
  TNode *node = NULL;
  while (search_word.length() > 0)
  {
    VERBOSE_LOG(LOG_INFO,  "SEARCHING " << search_word.c_str() << "(" << word << ")" << std::endl);
    if (Find(search_word.c_str(), pParent, &node)) {
      (*words)[0] = search_word.c_str();
      break;
    }
    if (node)
      break;

    VERBOSE_LOG(LOG_INFO,  "NO \"" << search_word.c_str() << "\" found; ");

    search_word = search_word.substr(0, search_word.length() - 1);
    VERBOSE_LOG(LOG_INFO,  "TRYING: " << search_word.c_str() << "(" << word << ")" << std::endl);
  }

  // now Extrapolate and score possibilities from stem
  if (node)
  {
    std::deque< UCHAR > accum;
    if (word != search_word) {
      VERBOSE_LOG(LOG_NONE,  "NO EXACT MATCH; NEAREST STEM: " << search_word.c_str() << "(ORIGINAL: " << word << ")" << std::endl);
    }
    VERBOSE_LOG(LOG_INFO,  "TRYING " << search_word.c_str() << "(" << word << ")" << std::endl);
    ExtrapolateAll(node, words, &accum, search_word.c_str(), word);
  }
}

// ExtrapolateAll
// Extrapolate all possibilities from an input string.
//
// @In:   node pointer to starting node
//        words vector of words
//        associative map of words
//        accumulator
// @Out:  at least one match found
//        words filled with words from starting node
bool TernaryTree::ExtrapolateAll(
    TNode *node,
    std::map< int, std::string > *words,
    std::deque< UCHAR > *accum,
    const char *stem,
    const char *word)
{
  std::map<int,int> tie_breaker_lookup;
  if (node) {
    Extrapolate(node, node->GetCenter(), words, accum, stem, word, &tie_breaker_lookup, max_diff_);
    return true;
  }
  else
    return false;
}

// Extrapolate
// Extrapolate from a word stem.
//
// I selected a deque as my accumulator data structure. A stack
// requires a second stack or some other means of word reversal,
// while a deque gives me both FIFO representation and FILO
// functionality.
//
// @In:     node pointer to starting node
//          words map of words, keyed by score
// @Out:    true == match found
//          pVect filled with words from starting node
bool TernaryTree::Extrapolate(
    TNode *root,
    TNode *node,
    std::map< int, std::string > *words,
    std::deque< UCHAR > *accum,
    const char *stem,
    const char *word,
    std::map< int, int > *tie_breaker_lookup,
    const int max_diff,
    int depth
    )
{
  if (!node)
    return false;

  TNode *pChild = NULL;
  bool ret = false;

  // Is this the end of a full word, ergo "o" in "piano"?
  if (node->GetTerminator()) {
    VERBOSE_LOG(LOG_DEBUG,  "TERMINATOR: " << node << std::endl);
    std::string search_word;
    TNode *pCur = node;
    while (pCur != root && pCur) {
      // Push this node's key onto our candidate accumulator
      accum->push_front(pCur->GetKey());
      pCur = pCur->GetParent();
    }
    // Reverse the search_word
    auto commit = *accum;
    while (!commit.empty()) {
      VERBOSE_LOG(LOG_DEBUG,  commit.front());
      search_word.push_back(commit.front());
      commit.pop_front();
    }
    VERBOSE_LOG(LOG_DEBUG,  std::endl);

    std::string compound;
    compound = stem;
    compound = compound.substr(0, compound.length());
    compound += search_word;
    VERBOSE_LOG(LOG_DEBUG,  "ADDING " << compound.c_str() << std::endl);

    int score = CalcLevenshtein(word, compound.c_str());

    // If the Levenshtein distance exceeds our variance threshold,
    // discontinue and unwind.
    if (max_diff && score > max_diff) {
      accum->clear();
      return false;
    }

    // Is this the first search_word with this levenshtein distance from the stem?
    // If so populate the key.  If not, use the current tie count.
    // We will keep a lookup table keyed by score containing the total #
    // of items with this score.
    // This operation is faster than the previous O ( (n^2) / 2 + n/2 ) of
    // iterating through words->count(tie_breaker + (score << shift)) until
    // we find an empty spot.
    // Note: limit of 4096 ties!
    int tie_breaker = 0;
    if (!((*tie_breaker_lookup).count(score))) {
      // Set this score-keyed lookup entry to the next distance to use
      (*tie_breaker_lookup)[ score ] = 0;
    } else {
      (*tie_breaker_lookup)[ score ] = (*tie_breaker_lookup)[ score ] + 1;
      tie_breaker = (*tie_breaker_lookup)[ score ];
      if( tie_breaker > tie_hwm_ ) {   // update tie high-watermark
        tie_hwm_ = tie_breaker;
      }
    }

    //while ((words)->count(tie_breaker + (score << 12)) != 0) {
    //  tie_breaker++;
    //}
    (*words)[ tie_breaker + (score << 12)] = compound;
    VERBOSE_LOG(LOG_DEBUG,  "SCORING " << word << " =|= " << compound.c_str() << " SCORE: " << score << std::endl);
    accum->clear();
  }

  // Recurse
  if ((pChild = node->GetLeft())) {
    if (Extrapolate(root, pChild, words, accum, stem, word, tie_breaker_lookup, max_diff, depth + 1)) {
      ret |= true;
      if (!accum->empty() &&
          !pChild->GetLeft() && !pChild->GetCenter() && !pChild->GetRight())
      {
      }
    }
  }

  if ((pChild = node->GetCenter())) {
    if (Extrapolate(root, pChild, words, accum, stem, word, tie_breaker_lookup, max_diff, depth + 1)) {
      ret |= true;
      if ((!accum->empty() &&
            !pChild->GetLeft() && !pChild->GetCenter() && !pChild->GetRight())) {
      }
    }
  }

  if ((pChild = node->GetRight())) {
    if (Extrapolate(root, pChild, words, accum, stem, word, tie_breaker_lookup, max_diff, depth + 1)) {
      ret |= true;
      if ((!accum->empty() &&
            !pChild->GetLeft() && !pChild->GetCenter() && !pChild->GetRight())) {
      }
    }
  }

  return ret;
}

// AllocNode
// Insert a node into the tree
//
// @In: key key of node to add
// @Out: Pointer to node
TNode *TernaryTree::AllocNode(char key)
{
  TNode *node = new TNode(key);
  assert(node);
  node->SetKey(key);
  return node;
}

// Utility preprocessor macro for Levenshtein
#if !defined(MIN3)
#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))
#else
#error MIN3 Already defined!
#endif

// CalcLevenshtein
//
// This is an optimized Levenshtein string distance calculation.
// It returns how "different" two strings are, effectively performing
// a commutative subtraction operation.
//
// @In: s1 string #1
// s2 string #2
// @Out: Levenshtein difference
int TernaryTree::CalcLevenshtein(const char *s1, const char *s2)
{
  int score = -1;
  if (s1 && s2) {
    unsigned int len1, len2, x, y, lastdiag, olddiag;
    len1 = strlen(s1);
    len2 = strlen(s2);
    unsigned int column[ len1 + 1 ];
    for (y = 1; y <= len1; y++)
      column[ y ] = y;
    for (x = 1; x <= len2; x++) {
      column[ 0 ] = x;
      for (y = 1, lastdiag = x - 1; y <= len1; y++) {
        olddiag = column[ y ];
        column[y] = MIN3(column[ y ] + 1,
          column[ y - 1 ] + 1,
          lastdiag +
          (
           s1[ y - 1 ] == s2[ x - 1 ] ? 0 : 1
          )
        );
        lastdiag = olddiag;
      }
    }
    score = column[ len1 ];
  }
  return score;
}
