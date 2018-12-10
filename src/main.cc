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
#include <pthread.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <map>
#include <algorithm>

#include "anagram_common.h"
#include "anagram_flags.h"
#include "templ_node.h"
#include "ternary_tree.h"
#include "anagram_log.h"
#include "occupancy_hash.h"
#include "anagram_lock.h"

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
  TernaryTree *trie,
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
    while (idx < start) {
      getline(file, line);
      idx++;
    }

    // Insert second half
    VERBOSE_LOG(LOG_INFO, "Reading phase 1..." << std::endl);
    std::locale loc;
    std::string lowerline;
    while (getline(file, line) )
    {
      lowerline = "";
      VERBOSE_LOG(LOG_DEBUG, "|" << line.c_str() << "|" << std::endl);
      for(auto elem : line)    // convert to lowercase; trie stores thus
         lowerline += std::tolower(elem,loc);
      if (!trie->Find(lowerline.c_str(), root_node)) {
        trie->Insert(line.c_str(), &root_node);
      }
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
      trie->Insert(line.c_str(), &root_node);
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
  cout << "Copyright (C) 2018 Gregory Hedger" << endl;
  cout << "" << endl;
  cout << "Usage:" << endl;
  cout << "\tanagram [flags] the phrase or word" << endl;
  cout << "Example:" << endl;
  cout << "\nanagram hello world" << endl << endl;
  cout << "Flags:" << endl;
  cout << "\t-b Use big dictionary (~423,000 words)" << endl;
  cout << "\t-d Allow duplicates of same work to appear" << endl;
  cout << "\t\tmultiple times in same anagram" << endl;
  cout << "\t-o Output directly. This is useful for performance for" << endl;
  cout << "\t\tinputs that produce a very large # of anagrams as" << endl;
  cout << "\t\tthe system is not limited by available memory and" << endl;
  cout << "\t\tcan stream directly to disk." << endl;
  cout << "\t-t use std::map tree structure instead of sparse hash array" << endl;
  cout << "\t-v set verbosity:" << endl;
  cout << "\t\t-v0 terse: anagrams only, no formatting or updates" << endl;
  cout << "\t\t-v1 normal [default]" << endl;
  cout << "\t\t-v2 info" << endl;
  cout << "\t\t-v3 debug" << endl;
}

// PrintAnagram
// Common entry point to print an anagram to stdout
// Entry: anagram string
void PrintAnagram(const char *anagram)
{
  VERBOSE_LOG(LOG_NONE, anagram << std::endl);
}

// GetCharCountMap
// Returns a map of the # of letters.
// Example: "fussy" will return
//  'f' -> 1
//  's' -> 2
//  'u' -> 1
//  'y' -> 1
// Entry: reference to map
//        const pointer to word
void GetCharCountMap(std::map< char, size_t>& char_count, const char *word)
{
  const char *current_char = word;
  // This counts the characters, ignoring spaces.
  for (auto i = 0; *current_char; ++i, ++current_char) {
    if (' ' == *current_char)
      continue;
    // std::map<> messiness: if there was no previous count, add it.
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
  std::map< char, size_t > char_count_a, char_count_b;
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
  std::map< char, size_t> master_count, candidate_count;
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
  std::map< char, size_t>& char_count_master,
  std::map< char, size_t>& char_count_a,
  std::map< char, size_t>& char_count_b
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


//
// Threading data structures
//
static char chars_completed[256];
static size_t cpu_tot;
static anagram::Lock chars_completed_lock;
static anagram::Lock subset_lock;
static anagram::Lock gather_lock;

// CombineSubsetsRecurse
// Recurse into subsets, additively updating candidate count.
// We have a candidate count passed in, and we will compare
/// against the other words to get the second candidate count.
// until we reach a full combo.
// Entry: word
//        subset map
//        output map
//        candidate combo
void CombineSubsetsRecurse(
  const char *word,
  std::map< std::string, int >& subset,
  std::map< std::string, int >& output,
  std::map< char, size_t>& master_count,
  std::map< char, size_t>& candidate_count_a,
  std::map<std::string, int>::const_iterator& start_i,
  AnagramFlags flags
)
{
  using namespace std;
  map< char, size_t> candidate_count_b;
  for (std::map<std::string, int>::const_iterator i = start_i; i != subset.end(); ++i) {
    // Skip ourselves
    // Note: Expensive and unnecessary since we use a map<>
    // Overwrite is cheaper than this check each time
    if (!flags.allow_dupes && !strcmp(i->first.c_str(), word))
      continue;
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
      if (flags.output_directly)
        PrintAnagram(output_phrase.c_str());
      else {
        output[output_phrase] = 1;
        VERBOSE_LOG(LOG_NORMAL, "\r" << "Anagrams found: " << output.size() << "\r");
      }
    } else if (comparison_result < 0) {
      // The two candidates do not make a full anagram; Since the letter count
      // permutation is still less than that of master, the two candidates
      // combined still form a partial.  Below, we combine them in a space-
      // delimited phrase and recurse.
      string output_phrase = word;
      output_phrase += " ";
      output_phrase += i->first;

      map< char, size_t> new_candidate_count;
      GetCharCountMap(new_candidate_count, output_phrase.c_str());
      CombineSubsetsRecurse(
        output_phrase.c_str(),
        subset,
        output,
        master_count,
        new_candidate_count,
        i,
        flags
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
//        subset map of partials
//        output map
void CombineSubsets(
  const char *word,
  std::map< std::string, int >& subset,
  std::map< std::string, int >& output,
  AnagramFlags flags
)
{
  std::map< char, size_t> master_count, candidate_count;
  GetCharCountMap(master_count, word);
  std::map<std::string, int>::const_iterator i = subset.begin();
  while (i != subset.end()) {
    if (!flags.allow_dupes && !strcmp(i->first.c_str(), word))
      continue;
    candidate_count.clear();
    GetCharCountMap(candidate_count, i->first.c_str());
    // Here we want to get a starting point for our character count
    // for the candidate, and compare it against the others.
    CombineSubsetsRecurse(
      i->first.c_str(),
      subset, output,
      master_count,
      candidate_count,
      i,
      flags
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
//        subset map
//        output map
//        candidate combo
void CombineSubsetsRecurseFast(
  const char *word,
  std::map< std::string, int >& subset,
  std::map< std::string, int >& output,
  OccupancyHash& master_count,
  OccupancyHash& candidate_count_a,
  std::map<std::string, int>::const_iterator& start_i,
  AnagramFlags flags
)
{
  using namespace std;
  OccupancyHash candidate_count_b;
  for (std::map<std::string, int>::const_iterator i = start_i; i != subset.end(); ++i) {
    // Disallow candidacy of already-processed word if dupes are disallowed.
    if (!flags.allow_dupes && !strcmp(i->first.c_str(), word))
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
      if (flags.output_directly)
        PrintAnagram(output_phrase.c_str());
      else {
        chars_completed_lock.Acquire();
        output[output_phrase] = 1;
        VERBOSE_LOG(LOG_NORMAL, "\r" << "Anagrams found: " << output.size() << "\r");
        chars_completed_lock.Release();
      }
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
        i,
        flags
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
//        subset map of partials
//        output map
void CombineSubsetsFast(
  const char *word,
  std::map< std::string, int >& subset,
  std::map< std::string, int >& output,
  AnagramFlags flags,
  int thread_index
)
{
  OccupancyHash master_count, candidate_count;
  subset_lock.Acquire();
  master_count.GetCharCountMap(word);
  std::map<std::string, int>::const_iterator i = subset.begin();
  subset_lock.Release();
  for (auto inc = 0; inc < thread_index; inc++) {
    ++i;
  }
  // We want to interleave the processing, so
  // we will start at a staggered position depending on our
  // thread index.
  while (i != subset.end()) {
    if (!flags.allow_dupes && !strcmp(i->first.c_str(), word))
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
        i,
        flags
        );
    // Increment by cpu count to preserve the task interleaving
    for (size_t inc = 0; inc < cpu_tot; inc++) {
      if( i != subset.end())
        ++i;
    }
  }
}


// GetAnagrams
// Entry: pointer to ternary_tree
//        word to check for anagrams
void GetAnagrams(
  TernaryTree& t,
  TNode *root_node,
  const char *word,
  std::map< std::string, int >& anagrams,
  std::map< std::string, int >& subset,
  AnagramFlags flags,
  int thread_index
)
{
  using namespace std;
  // Match lexical permutations:
  // We are going to try to find words containing ALL of the letters.
  // We will generate starting with each letter and filter out the ones that
  // have too many.
  size_t word_len = strlen((const char *)word);
  map< int, string > extrapolation;

  // Step 1: At the end of this process we will have a list of
  // A) complete set of one-word complete anagrams, for example:
  //  "live" -> "evil", "levi", "veil", "vile";
  // B) full words representing potential parts of anagrams.
  // Note that this first porion is single-threaded.
  VERBOSE_LOG(LOG_DEBUG, "Step 1: Garner full-word anagrams and partials..." << endl);
  if (thread_index == 0) {
    gather_lock.Acquire();  // block other threads until this first bit is done
    for (size_t i = 0; i <= word_len; ++i) {
      char c[2] = {0,0};
      *c = word[i];

      chars_completed_lock.Acquire();
      if(chars_completed[(size_t)c[0]]) {  // don't repeat letters already done
        chars_completed_lock.Release();
        continue;
      }
      chars_completed[(size_t)c[0]] = 1;

      extrapolation.clear();
      t.FuzzyFind((const char *)c, root_node, &extrapolation);

      chars_completed_lock.Release();

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
              if (flags.output_directly)
                PrintAnagram(candidate);
              else {
                chars_completed_lock.Acquire();
                anagrams[candidate] = 1;
                chars_completed_lock.Release();
                VERBOSE_LOG(LOG_NORMAL, "\r" << "Anagrams found: " << anagrams.size() << "\r");
              }
            }
          }
        } else {
          // Add partial word matches here:
          // If the word is smaller than the input AND all of the smaller word's
          // characters appear in the input (i.e. an unordered subset), then we
          // then we have a candidate for a partial match provided other word(s)
          // can fill in the missing characters exactly.
          if (IsSubset(word, candidate)) {
            subset_lock.Acquire();
            subset[candidate] = 1;    // mark word as a partial
            subset_lock.Release();
          }
        }
      }
    }
    gather_lock.Release();
  } else {
    // Non-first threads will block on this until the gathering
    // process is complete
    gather_lock.Acquire();
    gather_lock.Release();
  }

  // Iterates through all the findings and spit them out
  VERBOSE_LOG(LOG_DEBUG, "Step 2: Combine partials..." << endl);

  // Step 2: Now we have a complete set of subsets; we must now combine them to
  // obtain combinations matching the input word character count permutation.
  if (flags.tree_engine) {
    CombineSubsets(word, subset, anagrams, flags);
  } else {
    CombineSubsetsFast(word, subset, anagrams, flags, thread_index);
  }
}

//
// Threading
//

static volatile int thread_total = 0;

struct AnagramWorkerParams {
  TernaryTree *trie;
  TNode *root_node;
  const char *word;
  std::map< std::string, int > *anagrams;
  std::map< std::string, int > *subset;
  AnagramFlags flags;
  int thread_index;
};

// Worker
// This Worker thread process simply acquires and releases the lock repeatedly
// with the expectation other homogenous threads are doing likewise, for
// lock testing purposes.
// Entry: params
// Exit: (ignored)
void *Worker(void *worker_params)
{
  AnagramWorkerParams *params = (AnagramWorkerParams *) worker_params;

  GetAnagrams(
    *params->trie,
    params->root_node,
    params->word,
    *params->anagrams,
    *params->subset,
    params->flags,
    params->thread_index
  );

  --thread_total;
  return nullptr;
}

// initThreads
//
void StartThreads(const int thread_tot, AnagramWorkerParams *params)
{
  // Clean the common chars_completed array shared by
  // all the threads.
  memset(chars_completed, 0, sizeof(chars_completed));

  cpu_tot = thread_tot;

  // Allocate thread pointers
  AnagramWorkerParams *thread_params =
    (AnagramWorkerParams *) malloc(sizeof(AnagramWorkerParams) * thread_tot);
  if (thread_params) {
  } else {
    VERBOSE_LOG(LOG_NONE, "Allocation error(1)" << std::endl);
    return;
  }

  pthread_t *pthread = (pthread_t *) malloc(thread_tot * sizeof(pthread_t));
  if (!pthread) {
    free(thread_params);
    VERBOSE_LOG(LOG_NONE, "Allocation error(2)" << std::endl);
    return;
  }


  // This needs to be common to all the threads but does not
  // need to be visible to the client, so we will assume owneship
  // here.
  std::map< std::string, int > subset;
  // This adds all the threads
  int error = 0;
  for (auto i = 0; i < thread_tot; ++i) {
    memcpy(thread_params + i, params, sizeof(AnagramWorkerParams));
    thread_params[i].thread_index = i;     // set cpu index
    thread_params[i].subset = &subset;  // set the common working set
    error = pthread_create(
      pthread + i,
      NULL,
      &Worker,
      (void *)(thread_params + i));
    if (error) {
      // TODO: This is highly problematic is some threads were created!
      // Want to look at pthread_cancel with cleanup handlers.
      VERBOSE_LOG(LOG_NONE, "Thread creation error" << std::endl);
      free(thread_params);
      return;
    }
    // Wait a few milliseconds to allow first thread to get in
    struct timespec ts = {0, 10000000L };
    nanosleep(&ts, NULL);

    ++thread_total;
  }

  // Twiddle our thumbs while threads do their thing
  while (thread_total) {
    struct timespec ts = {0, 100000000L };
    ts.tv_nsec = (rand() & 10000000) | 1;
    nanosleep(&ts, NULL);
  }

  // Clean up and get out
  if (pthread)
    free(pthread);
  if (thread_params)
    free(thread_params);

  return;
}

//
// Exception handling
//

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
  TernaryTree trie;
  trie.SetRoot(&root_node);

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
  AnagramFlags flags;
  flags.tree_engine = flags.allow_dupes = flags.output_directly
    = flags.big_dictionary = 0;
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
          case 'b': {
              flags.big_dictionary = 1;
            }
            break;
          case 'd': {
              flags.allow_dupes = 1;
            }
            break;
          case 't': {
             flags.tree_engine = 1;
            }
            break;
          case 'o': {
             flags.output_directly = 1;
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
  if (!flags.output_directly) {
    VERBOSE_LOG(LOG_NORMAL, COUT_HIDECURSOR << COUT_BOLD_YELLOW << endl);
  }

  // This reads the dictionary file and gathers all the anagrams
  // from our source word.
  ReadDictionaryFile(
    "anagram_dict_no_abbreviations.txt",
    &trie,
    root_node
  );
  if (flags.big_dictionary) {
    ReadDictionaryFile(
      "anagram_bigdict.txt",
      &trie,
      root_node
    );
  }

  // Sets up our structure to hold the anagrams.  Note that, if
  // the -o "output_directly" flag is set, this will not be used and
  // the output will instead go directly to std::out, making
  // huge anagram files possible (> available physical memory).
  map< string, int > anagrams;  // container for anagram strings
  trie.SetMaxDifference(0);  // Do not clamp by Levenshtein distance

  AnagramWorkerParams params;
  params.trie = &trie;
  params.root_node = root_node;
  params.word = word.c_str();
  params.anagrams = &anagrams;
  params.flags = flags;
  params.thread_index = 0;   // Round-robined in StartThreads

  StartThreads(6, &params);

  // Iterates through all the findings and spit them out to stdout.
  // Only does so if we are not outputting directly; otherwise the
  // output collection will be empty.
  if (!flags.output_directly) {
    VERBOSE_LOG(LOG_NORMAL, "\r                         \r"
      << COUT_BOLD_WHITE << word.c_str()
      << COUT_BOLD_YELLOW << endl);
    int count = 0;
    for (auto i : anagrams) {
      cout << i.first << endl;
      ++count;
    }
    VERBOSE_LOG(LOG_NORMAL,  COUT_BOLD_WHITE << count << " ANAGRAMS FOUND.");
  }
  VERBOSE_LOG(LOG_NONE, COUT_NORMAL_WHITE << COUT_SHOWCURSOR << endl);

  return 0;
}
