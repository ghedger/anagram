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

#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_map>
#include <algorithm>

#include "anagram_common.h"
#include "templ_node.h"
#include "ternary_tree.h"
#include "anagram_log.h"
#include "occupancy_hash.h"

// CleanString
// Removes extraneous characters from string
// Entry: string (in/out)
// Exit: -
std::string& CleanString(std::string& s) {
    s.erase(remove_if(s.begin(), s.end(), [](const char& c) {
        return c == ' ';
    }), s.end());
    return s;
}

// GetWordCount
// Count the number of words in a file.  Assume one word per line.
// Entry: ifstream
// Exit: # of words in files
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
// Entry: path to file
//        pointer to TernaryTree
//        pointer to tree root node
void ReadDictionaryFile(
  const char *path,
  TernaryTree *pTree,
  TNode *& root_node
)
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
void OutputPreamble()
{
  using namespace std;
  cout << "Anagram" << endl;
  cout << "Copyright (C) 2019 Gregory P. Hedger" << endl;
  cout << "This program comes with ABSOLUTELY NO WARRANTY." << endl;
  cout << "This is free software, and you are welcome to redistribute it" << endl;
  cout << "under certain conditions covered by GNU Public License v3.0." << endl;
}

// PrintUsage
// Print basic program usage/invocation
void PrintUsage()
{
  using namespace std;
  OutputPreamble();
  cout << "Anagram" << endl;
  cout << "Copyright (C) 2018 Gregory Hedger" << endl;
  cout << "Usage:" << endl;
  cout << "\tanagram [flags] the phrase or word" << endl;
  cout << "Example:" << endl;
  cout << "\nanagram hello world" << endl << endl;
  cout << "Flags:" << endl;
  cout << "\t-v set verbosity:" << endl;
  cout << "\t\t-v0 terse: anagrams only, no formatting" << endl;
  cout << "\t\t-v1 normal" << endl;
  cout << "\t\t-v2 info" << endl;
  cout << "\t\t-v3 debug" << endl;
  cout << "\t-t use std::unordered_map tree structure instead of sparse hash array" << endl;
}
// GetCharCountMap
// Returns a unordered_map of the # of letters.
// Example: "fussy" will return
//  'f' -> 1
//  's' -> 2
//  'u' -> 1
//  'y' -> 1
// Entry: reference to unordered_map
//        const pointer to word
void GetCharCountMap(std::unordered_map< char, size_t>& char_count, const char *word)
{
  const char *current_char = word;
  // This counts the characters, ignoring spaces.
  for (auto i = 0; *current_char; ++i, ++current_char) {
    if (' ' == *current_char)
      continue;
    // std::unordered_map<> messiness: if there was no previous count, add it.
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
bool MatchCharCounts(const char *word_a, const char *word_b)
{
  bool result = true;  // assume success
  std::unordered_map< char, size_t > char_count_a, char_count_b;
  GetCharCountMap(char_count_a, word_a);
  GetCharCountMap(char_count_b, word_b);
  for (auto i : char_count_a) {
    if (!(char_count_b.count(i.first))
      || char_count_b[i.first] != i.second) {
      result = false;
      break;
    }
  }
  return result;
}

// IsSubset
// Determines if a candidate permutation is a subset of a master word; if all
// of the characters in the subset occur in the master, and no
// one uniquei candidate word character exceeds the count for that character
// in the master then the candidate is a subset of the master.
// Entry: master word
//        subset candidate word
// Exit:  true == candidate word is a subset
bool IsSubset(const char *master, const char *candidate)
{
  bool result = true; // assume success
  std::unordered_map< char, size_t> master_count, candidate_count;
  GetCharCountMap(master_count, master);
  GetCharCountMap(candidate_count, candidate);
  // Loop through the candidate and ensure that it contains
  // no characters that are not also part of the master,
  // and that of the common characters the candidate does not
  // exceed the count for the master (i.e. candidate has the same or
  // fewer letter "a"s than master)
  for (auto i : candidate_count) {
    if (!master_count.count(i.first)) { // checks if character is common
      result = false;                   // It isn't; we're done; exit
      break;
    }
    // Check master unique character count ensuring it meets/exceeds candidate
    if (master_count[i.first] < i.second) {
      result = false;
      break;
    }
  }

  if (result) {
    result = true;
  }

  return result;
}

// AddAndCompare
// Adds two candidates and compares them with the permutative lexical
// value of the master, returning a ternary output.
// Entry: master lexical count permutation
//        word a lexical count permutation
//        worb b lexical count permutation
// Exit: -1 candidate_a + candidate_b lexically less than master
//        0 candidate_a + candidate_b match master
//       +1 candidate_a + candidate_b lexically greater than master
int AddAndCompare(
  std::unordered_map< char, size_t>& char_count_master,
  std::unordered_map< char, size_t>& char_count_a,
  std::unordered_map< char, size_t>& char_count_b
)
{
  int result = 0;  // assume match

  // Add a to b, and store in b
  for (auto i : char_count_a) {   // place a counts in sum
    if (char_count_b.end() == char_count_b.find(i.first)) {
      char_count_b[i.first] = i.second;
    } else {
      char_count_b[i.first] += i.second;
    }
  }

  // Now that we have the lexical sum, we'll just check for subset
  for (auto i : char_count_b) {
    // This checks if there is a character in the candidate that
    // does not appear in the master; exit if so with +1 result.
    if (char_count_master.end() == char_count_master.find(i.first)) {
      result = 1;
      break;
    }
    // This checks if a given character count in the candidate exceeds
    // the count for the same character in the master; if so, exit with +1.
    if (char_count_master[i.first] < i.second) {
      result = 1;
      break;
    }

    // This checks if a given character count in the candidate is less than
    // the count for the same character in the master; if so, continue with -1.
    if (char_count_master[i.first] > i.second) {
      result = -1;
    }
  }

  // In case it looks like a match, need to make sure that no
  // master characters exist that don't also exist in the candidate...
  // If any do, then the candidate is lexically less than the master, -1.
  if (!result) {
    for (auto i : char_count_master) {
      if (char_count_b.end() == char_count_b.find(i.first)) {
        result = -1;
        break;
      }
    }
  }

  return result;
}

// CombineSubsetsRecurse
// Recurse into subsets, additively updating candidate count.
// We have a candidate count passed in, and we will compare
/// against the other words to get the second candidate count.
// until we reach a full combo.
// Entry: word
//        subset unordered_map
//        output unordered_map
//        candidate combo
void CombineSubsetsRecurse(
  const char *word,
  std::unordered_map< std::string, int >& subset,
  std::unordered_map< std::string, int >& output,
  std::unordered_map< char, size_t>& master_count,
  std::unordered_map< char, size_t>& candidate_count_a,
  std::unordered_map<std::string, int>::const_iterator& start_i
)
{
  using namespace std;
  unordered_map< char, size_t> candidate_count_b;
  candidate_count_b.reserve(26);
  for (std::unordered_map<std::string, int>::const_iterator i = start_i; i != subset.end(); ++i) {
    // Skip ourselves
    // Note: Expensive and unnecessary since we use a unordered_map<>
    // Overwrite is cheaper than this check each time
    //if (!strcmp(i->first.c_str(), word))
    //  continue;
    candidate_count_b.clear();
    GetCharCountMap(candidate_count_b, i->first.c_str());
    // This checks to for a complete anagram assembled from partials. This is
    // determined by a permutation equivalency.
    // The addition will be saved in candidate_count_b.
    int comparison_result = AddAndCompare(
      master_count,
      candidate_count_a,
      candidate_count_b
    );
    if (!comparison_result) {
      // This combination is a complete anagram; add it to the output,
      // separating the partials by spaces.
      string output_phrase = word;
      output_phrase += " ";
      output_phrase += i->first;
      output[output_phrase] = 1;
      VERBOSE_LOG(LOG_NORMAL, "\r" << "Anagrams found: " << output.size() << "\r");
    } else if (comparison_result < 0) {
      // The two candidates do not make a full anagram; Since the letter count
      // permutation is still less than that of master, the two candidates
      // combined still form a partial.  Below, we combine them in a space-
      // delimited phrase and recurse.
      string output_phrase = word;
      output_phrase += " ";
      output_phrase += i->first;

      unordered_map< char, size_t> new_candidate_count;
      GetCharCountMap(new_candidate_count, output_phrase.c_str());
      CombineSubsetsRecurse(
        output_phrase.c_str(),
        subset,
        output,
        master_count,
        new_candidate_count,
        i
      );
    } else {
      // The two candidates exceed the lexical permutative value of the
      // master; this combination will not work so continue on...
    }
  }
}

// CombineSubsets
// Given an input of a master word/phrase, find all combinations of partial words
// to create complete anagrams.  Spaces in master word are ignored.
// Entry: master word/phrase
//        subset unordered_map of partials
//        output unordered_map
void CombineSubsets(
  const char *word,
  std::unordered_map< std::string, int >& subset,
  std::unordered_map< std::string, int >& output
)
{
  std::unordered_map< char, size_t> master_count, candidate_count;
  GetCharCountMap(master_count, word);
  std::unordered_map<std::string, int>::const_iterator i = subset.begin();
  while (i != subset.end()) {
    //if (!strcmp(i->first.c_str(), word))
    //  continue;
    candidate_count.clear();
    GetCharCountMap(candidate_count, i->first.c_str());
    // Here we want to get a starting point for our character count
    // for the candidate, and compare it against the others.
    CombineSubsetsRecurse(
      i->first.c_str(),
      subset, output,
      master_count,
      candidate_count,
      i
    );
    ++i;
  }
}


// CombineSubsetsRecurseFast
// Recurse into subsets, additively updating candidate count.
// We have a candidate count passed in, and we will compare
/// against the other words to get the second candidate count.
// until we reach a full combo.
// Entry: word
//        subset unordered_map
//        output unordered_map
//        candidate combo
void CombineSubsetsRecurseFast(
  const char *word,
  std::unordered_map< std::string, int >& subset,
  std::unordered_map< std::string, int >& output,
  OccupancyHash& master_count,
  OccupancyHash& candidate_count_a,
  std::unordered_map<std::string, int>::const_iterator& start_i
)
{
  using namespace std;
  OccupancyHash candidate_count_b;
  for (std::unordered_map<std::string, int>::const_iterator i = start_i; i != subset.end(); ++i) {
    // Skip ourselves
    if (!strcmp(i->first.c_str(), word))
      continue;

    candidate_count_b.clear();
    candidate_count_b.GetCharCountMap(i->first.c_str());

    // This checks to for a complete anagram assembled from partials. This is
    // determined by a permutation equivalency.
    // The addition will be saved in candidate_count_b.
    candidate_count_b += candidate_count_a;
    int comparison_result = candidate_count_b.Compare(master_count);

    if (!comparison_result) {
      // This combination is a complete anagram; add it to the output,
      // separating the partials by spaces.
      string output_phrase = word;
      output_phrase += " ";
      output_phrase += i->first;
      output[output_phrase] = 1;

      VERBOSE_LOG(LOG_NORMAL, "\r" << "Anagrams found: " << output.size() << "\r");
    } else if (comparison_result < 0) {
      // The two candidates do not make a full anagram; Since the letter count
      // permutation is still less than that of master, the two candidates
      // combined still form a partial.  Below, we combine them in a space-
      // delimited phrase and recurse.
      string output_phrase = word;
      output_phrase += " ";
      output_phrase += i->first;

      OccupancyHash new_candidate_count;
      new_candidate_count.GetCharCountMap(output_phrase.c_str());
      CombineSubsetsRecurseFast(
        output_phrase.c_str(),
        subset,
        output,
        master_count,
        new_candidate_count,
        i
      );
    } else {
      // The two candidates exceed the lexical permutative value of the
      // master; this combination will not work so continue on...
    }
  }
}

// CombineSubsetsFast
// Given an input of a master word/phrase, find all combinations of partial words
// to create complete anagrams.  Spaces in master word are ignored.
// Entry: master word/phrase
//        subset unordered_map of partials
//        output unordered_map
void CombineSubsetsFast(
  const char *word,
  std::unordered_map< std::string, int >& subset,
  std::unordered_map< std::string, int >& output
)
{
  OccupancyHash master_count, candidate_count;
  master_count.GetCharCountMap(word);
  std::unordered_map<std::string, int>::const_iterator i = subset.begin();
  while (i != subset.end()) {
    if (!strcmp(i->first.c_str(), word))
      continue;
    candidate_count.clear();
    candidate_count.GetCharCountMap(i->first.c_str());

    // Here we want to get a starting point for our character count
    // for the candidate, and compare it against the others.
    CombineSubsetsRecurseFast(
      i->first.c_str(),
      subset,
      output,
      master_count,
      candidate_count,
      i
    );
    ++i;
  }
}
// GetAnagrams
// Entry: pointer to ternary_tree
//        word to check for anagrams
void GetAnagrams(
  TernaryTree& t,
  TNode *root_node,
  const char *word,
  std::unordered_map< std::string, int >& anagrams,
  bool use_tree_engine
)
{
  using namespace std;
  // We are going to try to find words containing ALL of the letters.
  // We will generate starting with each letter and filter out the ones that
  // have too many.
  size_t word_len = strlen((const char *)word);

  unordered_map< string, int > subset;
  map< int, string > extrapolation;

  // Step 1: At the end of this process we will have a list of
  // A) complete set of one-word complete anagrams, for example:
  //  "live" -> "evil", "levi", "veil", "vile";
  // B) full words representing potential parts of anagrams.
  VERBOSE_LOG(LOG_DEBUG, "Step 1: Garner full-word anagrams and partials..." << endl);
  for (size_t i = 0; i <= word_len; i++) {
    char c[2] = {0,0};
    *c = word[i];
    extrapolation.clear();
    t.FuzzyFind((const char *)c, root_node, &extrapolation);

    // Now we'll check the lengths of each word and compare it to our input
    // For the few that match, we'll check and see if they have the same
    // characters.
    // TODO: It's likely possible to optimize what follows.
    // Probably should look at using a binary heap or other data structure.
    size_t candidate_len = 0;
    for (auto it : extrapolation) {
      const char *candidate = it.second.c_str();
      // This checkes for a length and character count match; if found
      // then the word is an anagram so we add it to our output collection.
      if ((candidate_len = strlen(candidate)) == word_len) {
        // Does word match against character count?
        if (MatchCharCounts(candidate, word)) {
          if (!anagrams.count(candidate)) {
            anagrams[candidate] = 1;
          }
        }
      } else {
        // TODO: Add partial word matches here:
        // If the word is smaller than the input AND all of the smaller word's
        // characters appear in the input (i.e. an unordered subset), then we
        // then we have a candidate for a partial match provided other word(s)
        // can fill in the missing characters exactly.
        if (IsSubset(word, candidate)) {
          subset[candidate] = 1;    //
        }
      }
    }
  }

  // Iterates through all the findings and spit them out
  VERBOSE_LOG(LOG_DEBUG, "Step 2: Combine partials..." << endl);

  // Step 2: Now we have a complete set of subsets; we must now combine them to
  // obtain combinations matching the input word character count permutation.
  if (use_tree_engine) {
    CombineSubsets(word, subset, anagrams);
  } else {
    CombineSubsetsFast(word, subset, anagrams);
  }
}

// SigtermHandler
// This is needed so that, if the user hits Ctrl-C, the curser
// can be set back to normal.  It exits the program.
// Entry: signal type
void SigtermHandler(int signal)
{
  std::cout << COUT_SHOWCURSOR << COUT_NORMAL_WHITE << std::endl;
  exit(1);
}

// main
// This is the main entry point and testbed for ternary tree.
//
// @In:     -
// @Out:    0 == success
int main(int argc, const char *argv[])
{
  using namespace std;
  TNode *root_node = NULL;
  TernaryTree t;

  // This hides the cursor and sets up a signal handler to re-show it in
  // case user hits CTRL-C
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = SigtermHandler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Sets a sane level that allows UI but not debug messages
  SET_VERBOSITY_LEVEL(LOG_NORMAL);

  // This parses the arguments and takes subsequent non-dashed arguments
  // as the input (no quotes required)
  bool use_old_engine = false;
  string word;
  if (1 < argc) {
    int i = 1;
    while (i < argc) {
      if ('-' == argv[i][0]) {
        if (word.length()) {
          PrintUsage();
          return -1;
        }
        switch(argv[i][1]) {
          case 'v': {
              int verbosity;
              if (isdigit(verbosity = argv[i][2])) {
                LOG_LEVEL log_level = (LOG_LEVEL) (verbosity - (int) '0');
                SET_VERBOSITY_LEVEL(log_level);
              }
            }
            break;
          case 'd': {
              unsigned int diff;
              sscanf(&argv[i][2], "%d", &diff);
              if (diff > 0 && diff < (unsigned int) ~0) {
                cout << diff << endl;
                t.SetMaxDifference( (int) diff);
              }
            }
            break;
          case 't': {
            use_old_engine = true;
          }
            break;

          default:
            PrintUsage();
            return -1;
        }
      }
      else
      {
        word += argv[i];
        word += " ";
      }
      i++;
    }
  }

  // This rtrims and lowercases the word
  word.erase(std::find_if(word.rbegin(), word.rend(), [](int c) {
        return !std::isspace(c);
    }).base(), word.end());
  std::transform(word.begin(), word.end(), word.begin(), ::tolower);

  if (!word.length()) {
    PrintUsage();
    return -1;
  }

  // This will hide the cursor and set the color
  ////VERBOSE_LOG(LOG_NORMAL, COUT_HIDECURSOR << COUT_BOLD_YELLOW << endl);

  // This reads the dictionary file and gathers all the anagrams
  // from our source word.
  ReadDictionaryFile("dict_no_abbreviations.txt", &t, root_node);
  unordered_map< string, int > anagrams;
  t.SetMaxDifference(0);
  GetAnagrams(t, root_node, word.c_str(), anagrams, use_old_engine);

  // Iterates through all the findings and spit them out to stdout.
  VERBOSE_LOG(LOG_NORMAL, "\r                         \r"
    << COUT_BOLD_WHITE << word.c_str()
    << COUT_BOLD_YELLOW << endl);
  int count = 0;
  for (auto i : anagrams) {
    cout << i.first << endl;
    ++count;
  }
  VERBOSE_LOG(LOG_NORMAL,  COUT_BOLD_WHITE << count << " ANAGRAMS FOUND."
    << COUT_SHOWCURSOR << endl);

  return 0;
}
