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
#include <set>
#include <map>
#include <vector>
#include <stack>
#include <queue>
#include <deque>
#include <algorithm>
#include "templ_node.h"
#include "ternary_tree.h"
#include "anagram_log.h"

int GetWordCount(std::ifstream *file)
{
  int lineTot = std::count(std::istreambuf_iterator<char>(*file),
      std::istreambuf_iterator<char>(), '\n');

  file->seekg(0);
  return lineTot;
}

// ReadDictionaryFile
// Read dictionary file into our data structure.
// TODO (RFE): Would be nice to have the tree self-balance
// instead of reading the sorted file in two halves...
//
// @In:     -
// @Out:    -
void ReadDictionaryFile(const char *path, TernaryTree *pTree, TNode *& root_node)
{
  try
  {
    std::ifstream file(path);
    std::string line;
    int lineTot = GetWordCount(&file);
    int start = lineTot >> 1;
    int idx = 0;

    VERBOSE_LOG(LOG_INFO, "Reading " << lineTot << " words." << std::endl);

    // Begin at middle of file
    while (idx < start)
    {
      getline(file, line);
      idx++;
    }

    // Insert second half
    VERBOSE_LOG(LOG_INFO, "Reading phase 1..." << std::endl);
    while (getline(file, line) )
    {
      VERBOSE_LOG(LOG_DEBUG, "|" << line.c_str() << "|" << std::endl);
      pTree->Insert(line.c_str(), &root_node);
    }

    // Insert first half
    VERBOSE_LOG(LOG_INFO, "Reading phase 2..." << std::endl);
    idx = 0;
    file.clear();
    file.seekg(0, std::ios::beg);
    while (idx < start)
    {
      getline(file, line);
      VERBOSE_LOG(LOG_DEBUG, "|" << line.c_str() << "|" << std::endl);
      pTree->Insert(line.c_str(), &root_node);
      idx++;
    }
  }
  catch(...)
  {
    VERBOSE_LOG(0, "Error reading file." );
  }
}

// OutputPreamble
//
// @In:     -
// @Out:    -
void OutputPreamble()
{
  std::cout <<  "Dicto" << std::endl;
  std::cout <<  "Copyright (C) 2015 Gregory P. Hedger" << std::endl;
  std::cout << "This program comes with ABSOLUTELY NO WARRANTY." << std::endl;
  std::cout << "This is free software, and you are welcome to redistribute it" << std::endl;
  std::cout << "under certain conditions covered by GNU Public License v3.0." << std::endl;
}


// PrintPrompt
// output prompt
//
// @In:     -
// @Out:    -
void PrintPrompt()
{
  std::cout << std::endl << ">" ;
}

// PrintUsage
// Print basic program usage/invocation
//
// @In:     -
// @Out:    -
void PrintUsage()
{
  OutputPreamble();
  std::cout << "Usage:" << std::endl;
  std::cout << "dicto [flags]:" << std::endl;
  std::cout << "Flags:" << std::endl;
  std::cout << "\t-v set verbosity: -v0 none -v1 info -v2 debug" << std::endl;
  std::cout << "\t-d set maximum Levenshtein distance, example -d18" << std::endl;
}

// LEG is used for PrintTraversal to tell what leg the current node is on.
enum LEG
{
  LEG_L   = 0,
  LEG_C,
  LEG_R,
};

// We're going to render into a buffer
const int screenWidth = 80;
const int screenHeight = 8;
static char _renderBuf [ screenWidth * screenHeight ];

// PrintTraversal
// Render and prints a rudimentary treelike representation to console.
// Note this is rather rough, added at the end of this demo; it could be
//  improved.
//
// @In:     root_node pointer to root node
//          leg left, right, or center
//          htab starting htab
// @Out:    -
void PrintTraversal(TNode *pNode, LEG leg, int htab, int depth = 0)
{
  int vtab = depth;

  // Initialize render buf if at top
  if (!depth) {
    printf("\n");
    memset(_renderBuf, ' ', sizeof(_renderBuf) );
    htab = screenWidth >> 1;        // set initial htab to center of rendering buffer
  }

  if (depth > 3)
    return;


  // Caculate screen layout
  switch (leg) {
    case LEG_L:
      htab -= 22 - (depth << 1);
      _renderBuf[ htab - 1 + (vtab * screenWidth) ] = '>';
      break;
    case LEG_R:
      htab += 22 - (depth << 1);
      _renderBuf[ htab - 1 + (vtab * screenWidth) ] = '<';
      break;
    case LEG_C:
      _renderBuf[ htab - 1 + (vtab * screenWidth) ] = '^';
      break;
    default:
      break;
  }

  // printf("htab: %d depth: %d value: %c\n", htab, depth, pNode->GetKey());

  sprintf(&_renderBuf[ htab + (vtab * screenWidth) ], "%c",  pNode->GetKey());
  if (pNode->GetLeft() )
  {
    PrintTraversal(pNode->GetLeft(), LEG_L, htab, depth + 1);
  }
  if (pNode->GetCenter() )
  {
    PrintTraversal(pNode->GetCenter(), LEG_C, htab, depth + 1);
  }
  if (pNode->GetRight() )
  {
    PrintTraversal(pNode->GetRight(), LEG_R, htab, depth + 1);
  }

  // If we're done, print out the rendered tree
  if (!depth)
  {
    int i, j;
    for (j = 0; j < screenHeight; j++ )
    {
      for (i = 0; i < screenWidth; i++ )
      {
        char c = _renderBuf[ i +  (j * screenWidth)];
        // Replace terminator with space
        c = c ? c : ' ';
        putchar(c);
      }
      printf("\n" );         // Print LF every screenWidth columns
    }
  }
}


// GetCharCountMap
// Returns a map of the # of letters.
// Example: "fussy" will return
//  'f' -> 1
//  's' -> 2
//  'u' -> 1
//  'y' -> 1
// Entry: reference to map<UCHAR, size_t>
//        const pointer to word
void GetCharCountMap(std::map<UCHAR, size_t>& char_count, const UCHAR *word)
{
  const UCHAR *current_char = word;
  for (auto i = 0; *current_char; ++i, ++current_char) {
    // Dictionary messiness: if there was no previous count, add it.
    // Otherwise, increase the count of this character in the word.
    if (!char_count.count(*current_char)) {
      char_count[*current_char] = 1;
    } else {
      char_count[*current_char] = char_count[*current_char] + 1;
    }
  }
}

// MatchCharCounts
// Get the per-word character counts and determine if they match.
// Example: counts for 'live', 'levi', 'veil', 'vile', and 'evil' match.
// Entry: first word
//        second word
// Exit: true == match
bool MatchCharCounts(const UCHAR *word_a, const UCHAR *word_b)
{
  bool result = true;  // assume success
  std::map< UCHAR, size_t > char_count_a, char_count_b;
  GetCharCountMap(char_count_a, word_a);
  GetCharCountMap(char_count_b, word_b);
  for (auto i : char_count_a) {
    if (char_count_b.count(i.first) != i.second) {
      result = false;
      break;
    }
  }
  return result;
}

// GetAnagrams
// Entry: pointer to ternary_tree
//        word to check for anagrams
void GetAnagrams(
  TernaryTree& t,
  TNode *root_node,
  std::map< int, std::string >& anagrams,
  const UCHAR *word
)
{
  using namespace std;
  // We are going to try to find words containing ALL of the letters.
  // We will generate starting with each letter and filter out the ones that
  // have too many.
  size_t word_len = strlen((const char *)word);

  map< string, int > output;
  map< int, string > extrapolation;
  for (size_t i = 0; i <= word_len; i++) {
    UCHAR c[2] = {0,0};
    *c = word[i];
    extrapolation.clear();
    t.FuzzyFind((const char *)c, root_node, &extrapolation);

    // Now we'll check the lengths of each word and compare it to our input
    // For the few that match, we'll check and see if they have the same
    // characters.
    // TODO: This is horribly inefficient brute force approach; optimize!
    size_t candidate_len = 0;
    for (auto it : extrapolation) {
      const UCHAR *candidate = (const UCHAR *)it.second.c_str();
      // Does length match?
      if ((candidate_len = strlen((const char *)candidate)) == word_len) {
        // Does word match against character count?
        if (MatchCharCounts(candidate, word)) {
          if (!output.count((const char *)candidate)) {
            output[(const char *)candidate] = 1;
          }
        }
      } else {
        // TODO: Add partial word matches here
      }
    }
  }

  // Iterates through all the findings and spit them out
  for (auto i : output) {
    cout << i.first << endl;
  }
}

// main
// This is the main entry point and testbed for ternary tree.
//
// @In:     -
// @Out:    0 == success
int main(int argc, const char *argv[])
{
  const int kMaxIn = 128;
  TNode *root_node = NULL;
  TernaryTree t;

  // parseargs
  if (1 < argc) {
    int i = 1;
    while (i < argc) {
      if ('-' == argv[i][0]) {
        switch(argv[i][1]) {
          case 'v':
            {
              int verbosity;
              if (isdigit(verbosity = argv[i][2])) {
                LOG_LEVEL ll = (LOG_LEVEL) (verbosity - (int) '0');
                SET_VERBOSITY_LEVEL(ll);
              }
            }
            break;
          case 'd':
            {
              unsigned int diff;
              sscanf(&argv[i][2], "%d", &diff);
              if (diff > 0 && diff < (unsigned int) ~0) {
                std::cout << diff << std::endl;
                t.SetMaxDifference( (int) diff);
              }
            }
            break;

          default:
            PrintUsage();
            return 1;
        }
      }
      else
      {
        PrintUsage();
        return 1;
      }

      i++;
    }
  }

  OutputPreamble();
  ReadDictionaryFile("dict.txt", &t, root_node);

  // Print out the top portion of the tree
  if (LOG_DEBUG <= GET_LOG_VERBOSITY())
    PrintTraversal(root_node, LEG_C, 0, 0);

  UCHAR in[ kMaxIn ];
  while (1) {
    PrintPrompt();
    std::cin >> in;

    // Extrapolate words from a prefix
    const UCHAR *prefix = in;

    std::map< int, std::string > anagrams;
    t.SetMaxDifference(0);
    GetAnagrams(t, root_node, anagrams, prefix);
  }
  return 0;
}
