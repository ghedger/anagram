#pragma clang diagnostic push
#pragma ide diagnostic ignored "EmptyDeclOrStmt"
/* MIT License
 *
 * Copyright (c) 2020 Greg Hedger
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

#include <cstdio>
#include <memory.h>
#include <unistd.h>
#include <csignal>
#include <pthread.h>
#include <ctime>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <algorithm>
#include <thread>

#include "anagram_common.h"
#include "anagram_flags.h"
#include "templ_node.h"
#include "ternary_tree.h"
#include "anagram_log.h"
#include "occupancy_hash.h"
#include "anagram_lock.h"
#include "output_queue.h"

namespace anagram {
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
  int lineTot;
  lineTot = std::count(std::istreambuf_iterator<char>(*file),
                       std::istreambuf_iterator<char>(), '\n');
  file->seekg(0);
  return lineTot;
}

// ReadDictionaryFile
// Read dictionary file into our trie data structure.
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

    // Skip to middle of file
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
      //VERBOSE_LOG(LOG_DEBUG, "|" << line.c_str() << "|" << std::endl);
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
  cout << "Copyright (c) 2018 Greg Hedger" << endl;
  cout << "MIT License" << endl;
  cout << endl;
  cout << "Permission is hereby granted, free of charge, to any person obtaining a copy" << endl;
  cout << "of this software and associated documentation files (the \"Software\"), to deal" << endl;
  cout << endl;
  cout << "in the Software without restriction, including without limitation the rights" << endl;
  cout << "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell" << endl;
  cout << "copies of the Software, and to permit persons to whom the Software is" << endl;
  cout << "furnished to do so, subject to the following conditions:" << endl;
  cout << endl;
  cout << "The above copyright notice and this permission notice shall be included in all" << endl;
  cout << "copies or substantial portions of the Software." << endl;
  cout << "Copyright (C) 2019 Gregory P. Hedger" << endl;
  cout << endl;
}

// PrintUsage
// Print basic program usage/invocation
void PrintUsage()
{
  using namespace std;
  OutputPreamble();
  cout << "" << endl;
  cout << "Usage:" << endl;
  cout << "\tanagram [flags] the phrase or word" << endl;
  cout << "Example:" << endl;
  cout << "\nanagram hello world" << endl << endl;
  cout << "Flags:" << endl;
  cout << "\t-b Use big dictionary (~423,000 words)" << endl;
  cout << "\t-d Allow duplicates of same work to appear" << endl;
  cout << "\t\tmultiple times in same anagram" << endl;
  cout << "\t-e exclude (example -ealb,hello,exclude" << endl;
  cout << "\t-o Output directly. This is useful for performance for" << endl;
  cout << "\t\tinputs that produce a very large # of anagrams as" << endl;
  cout << "\t\tthe system is not limited by available memory and" << endl;
  cout << "\t\tcan stream directly to disk." << endl;
  cout << "\t-s print subset dictionary of partial candidate words" << endl;
  cout << "\t-t use std::map tree structure instead of sparse hash array" << endl;
  cout << "\t-v set verbosity:" << endl;
  cout << "\t\t-v0 terse: anagrams only, no formatting or updates" << endl;
  cout << "\t\t-v1 normal [default]" << endl;
  cout << "\t\t-v2 info" << endl;
  cout << "\t\t-v3 debug" << endl;
}

//
// Threading data structures
//
static char chars_completed[256];
static size_t cpu_tot;
static anagram::Lock output_lock;
static anagram::Lock subset_lock;
static anagram::Lock gather_lock;

// PrintAnagram
// Prints anagram followed by an endline
// Entry: anagram
void PrintAnagram(const char *anagram)
{
  output_lock.Acquire();
  std::cout << anagram << std::endl;
  output_lock.Release();
}

// PrintSubset
// Prints the complete subset dictionary of candidate words for the input.
// Entry: subset
void PrintSubset(std::map< std::string, int >& subset, OutputQueue *queue)
{
  const int kColCount = 8;
  if (queue) {
    auto column_count = kColCount;
    std::string out;
    for (const auto& i : subset) {
      out += i.first;
      if (!--column_count) {
        column_count = kColCount;
        out += "\n";
        queue->Push(out.c_str());
        out = "";
      } else {
        out += ",";
      }
    }
    out += "\n\n";
    queue->Push(out.c_str());
  }
}

// This is to ensure the output queue is not overloaded
const int kOutputQueueThrottleFrequency = 100;
static int output_queue_throttle = kOutputQueueThrottleFrequency;

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
  OccupancyHash& candidate_count_b,
  OccupancyHash *candidate_count_arr[],
  std::map<std::string, int>::const_iterator& start_i,
  AnagramFlags flags,
  OutputQueue *queue,
  int depth
)
{
  using namespace std;
  for (auto i = start_i; i != subset.end(); ++i) {
    // Disallow candidacy of already-processed word if dupes are disallowed.
    if (!flags.allow_dupes && !strcmp(i->first.c_str(), word))
      continue;

    candidate_count_b.clear();
    candidate_count_b.GetCharCountMap(i->first.c_str());

    // This checks to for a complete anagram assembled from partials. This is
    // determined by a lexically-equivalent permutation (same character
    // counts in different permutation).  Sum stored in candidate_count_b.
    candidate_count_b += candidate_count_a;
    int comparison_result = candidate_count_b.Compare(master_count);

    if (!comparison_result) {
      // This combination is a complete anagram; add it to the output,
      // separating the partials by spaces.
      string output_phrase = word;
      output_phrase += " ";
      output_phrase += i->first;
      if (flags.output_directly) {
        output_phrase.append("\n");
        queue->Push(output_phrase.c_str());
      } else {
        subset_lock.Acquire();
        output[output_phrase] = 1;
        subset_lock.Release();

        if (!--output_queue_throttle) {
          output_queue_throttle = kOutputQueueThrottleFrequency;
          output_lock.Acquire();
          static char out[256];
          sprintf(out, "\rAnagrams found: %ld    ", output.size());
          queue->Push(out);
          output_lock.Release();
        }
      }
    } else if (comparison_result < 0) {
      // The two candidates do not make a full anagram; Since the letter count
      // permutation is still less than that of master, the two candidates
      // combined still form a partial.  Below, we combine them in a space-
      // delimited phrase and recurse.
      string output_phrase = word;
      output_phrase += " ";
      output_phrase += i->first;
      //OccupancyHash new_candidate_count;
      //new_candidate_count.GetCharCountMap(output_phrase.c_str());
      candidate_count_arr[depth]->clear();
      candidate_count_arr[depth]->GetCharCountMap(output_phrase.c_str());
      CombineSubsetsRecurseFast(
        output_phrase.c_str(),
        subset,
        output,
        master_count,
        *candidate_count_arr[depth],
        candidate_count_b,
        candidate_count_arr,
        i,
        flags,
        queue,
        depth + 1
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
  int thread_index,
  OutputQueue *queue
)
{
  OccupancyHash master_count, candidate_count_a, candidate_count_b;
  OccupancyHash *candidate_count_arr[64];
  for (auto & i : candidate_count_arr) {
    i = new OccupancyHash();
  }

  master_count.GetCharCountMap(word);
  std::map<std::string, int>::const_iterator i = subset.begin();
  for (auto inc = 0; inc < thread_index; ++inc) {
    if( i != subset.end())
      ++i;
  }
  // We want to interleave the processing, so
  // we will start at a staggered position depending on our
  // thread index.
  while (i != subset.end()) {
    if (!flags.allow_dupes && !strcmp(i->first.c_str(), word))
      continue;
    candidate_count_a.clear();
    candidate_count_a.GetCharCountMap(i->first.c_str()); // count char occurrences

    // Here we want to get a starting point for our character count
    // for the candidate, and compare it against the others.
    CombineSubsetsRecurseFast(
        i->first.c_str(),
        subset,
        output,
        master_count,
        candidate_count_a,
        candidate_count_b,
        candidate_count_arr,
        i,
        flags,
        queue,
        0
    );
    // Increment by cpu count to preserve the task interleaving
    for (size_t inc = 0; inc < cpu_tot; ++inc) {
      if (i != subset.end()) { ++i; }
    }
  }

  // Clean up
  for (auto & di : candidate_count_arr) {
    delete di;
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
  std::map< std::string, int >& excludeset,
  AnagramFlags flags,
  int thread_index,
  OutputQueue *queue
)
{
  using namespace std;
  // Match lexical permutations:
  // We are going to try to find words containing ALL of the letters.
  // We will generate starting with each letter and filter out the ones that
  // have too many.

  if (thread_index == 0) {
    gather_lock.Acquire();  // block other threads until this first bit is done
    size_t word_len = strlen((const char *)word);
    map< int, string > extrapolation;

    // Step 1: At the end of this process we will have a list of
    // A) complete set of one-word complete anagrams, for example:
    //  "live" -> "evil", "levi", "veil", "vile";
    // B) full words representing potential parts of anagrams.
    // Note that this first porion is single-threaded.
    VERBOSE_LOG(LOG_DEBUG, "Step 1: Garner full-word anagrams and partials..." << endl);

    // Get unique character counts for the master word/phrase
    OccupancyHash master_count(word);
    OccupancyHash candidate_count;  // reused for each candidate word

    // Step through all the letters in the source word/phrase, avoiding repetitions.
    // For example, if the phrase is "pussy cat":
    // - find all words beginning with "p" and matching the character count,
    //    and mark "p" as done.
    //      o this will return phrases like
    for (size_t i = 0; i <= word_len; ++i) {
      char c[2] = {0,0};
      *c = word[i];

      if (chars_completed[(size_t)(unsigned char)c[0]]) {  // don't repeat letters already done
        continue;
      }

      chars_completed[(size_t)(unsigned char)c[0]] = 1;

      extrapolation.clear();
      t.FuzzyFind((const char *)c, root_node, &extrapolation);

      // Now we'll check the lengths of each word and compare it to our input
      // For the few that match, we'll check and see if they have the same
      // characters.
      for (const auto& it : extrapolation) {
        // This checks if the word is in the exclude set; if so, ignore and continue.
        if (excludeset.end() != excludeset.find(it.second))
          continue;

        const char *candidate = it.second.c_str();

        candidate_count.clear();
        candidate_count.GetCharCountMap(candidate);

        int comparison_result = candidate_count.Compare(master_count);
        if (!comparison_result) { // candidate unique char counts == master?
          // If we got here, it's a FULL anagram; add it.
          if (!anagrams.count(candidate)) {
            if (flags.output_directly) {
              string out = candidate;
              out += "\n";
              queue->Push(out.c_str());
            } else {
              anagrams[candidate] = 1;
              static char out[256];
              sprintf(out, "\rAnagrams found: %ld    ", anagrams.size());
              queue->Push(out);
            }
          }
        }
        else if (candidate_count.IsSubset(master_count)) { // candidate char counts < master?
          subset[candidate] = 1;    // mark word as a partial
        }
      }
    }

    // If we are to print subsets, this does that now.
    if (flags.print_subset) {
      PrintSubset(subset,queue);
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
  CombineSubsetsFast(word, subset, anagrams, flags, thread_index, queue);

  OccupancyHash::PrintConstructorCalls();
}

//
// Threading
//

// Global count of active threads.  Each thread decrements this on completion.
static volatile int thread_total = 0;

// AnagramWorkerParams
// Structure for paramters to be passed to worker threads.
// TODO: Move this to an appropriate header.
struct AnagramWorkerParams {
  TernaryTree *trie;
  TNode *root_node;
  const char *word;
  std::map< std::string, int > *anagrams;
  std::map< std::string, int > *subset;
  std::map< std::string, int > *excludeset;
  AnagramFlags flags;
  int thread_index;
  OutputQueue *queue;
};

// Worker
// This is the top-level entry for job concurrent job processing.
// Entry: AnagramWorkParams
// Exit: (ignored)
void *Worker(void *worker_params)
{
  auto *params = (AnagramWorkerParams *) worker_params;

  GetAnagrams(
    *params->trie,
    params->root_node,
    params->word,
    *params->anagrams,
    *params->subset,
    *params->excludeset,
    params->flags,
    params->thread_index,
    params->queue
  );

  usleep(10000);

  if (thread_total)
    --thread_total;

  return nullptr;
}

// initThreads
//
void RunJob(const unsigned int thread_tot, AnagramWorkerParams *params)
{
  // Our one and only output queue runs on its own thread
  OutputQueue queue;

  // Clean the common chars_completed array shared by
  // all the threads.
  memset(chars_completed, 0, sizeof(chars_completed));

  cpu_tot = thread_tot; // assume 1-to-1 correlation between threads and cpus

  // Allocate thread parameter blocks
  auto *thread_params =
    (AnagramWorkerParams *) malloc(sizeof(AnagramWorkerParams) * thread_tot);
  if (thread_params) {
  } else {
    VERBOSE_LOG(LOG_NONE, "Allocation error(1)" << std::endl);
    return;
  }

  auto *pthread_struct = (pthread_t *) malloc(thread_tot * sizeof(pthread_t));
  if (!pthread_struct) {
    free(thread_params);
    VERBOSE_LOG(LOG_NONE, "Allocation error(2)" << std::endl);
    return;
  }

  // This needs to be common to all the threads but does not
  // need to be visible to the client, so we will assume owneship
  // here.
  std::map< std::string, int > subset;
  // This adds all the threads
  int error;
  for (auto i = 0; i < thread_tot; ++i) {
    memcpy(thread_params + i, params, sizeof(AnagramWorkerParams));
    thread_params[i].thread_index = i;  // set cpu index
    thread_params[i].subset = &subset;  // set the common working set
    thread_params[i].queue = &queue;  // set the common working set
    error = pthread_create(
      &pthread_struct[i],
      nullptr,
      &Worker,
      (void *)(thread_params + i));
    if (error) {
      // TODO: This is highly problematic if some threads were created!
      // Want to look at pthread_cancel with cleanup handlers.
      VERBOSE_LOG(LOG_NONE, "Thread creation error" << std::endl);
      free(thread_params);
      return;
    }
    // Wait a few milliseconds to allow first thread to get in
    if (!i) {
      struct timespec ts = {0, 10000000L };
      nanosleep(&ts, nullptr);
    }

    ++thread_total;
  }

  // Twiddle our thumbs while threads do their thing
  void *result;
  pthread_join(pthread_struct[0],&result);
  pthread_join(pthread_struct[1],&result);
  pthread_join(pthread_struct[2],&result);
  pthread_join(pthread_struct[3],&result);
  pthread_join(pthread_struct[4],&result);
  pthread_join(pthread_struct[5],&result);
  pthread_join(pthread_struct[6],&result);

  // Clean up and get out
  free(pthread_struct);
  free(thread_params);
}
} // namespace anagram

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
  setvbuf(stdout, nullptr, _IONBF, 1024);  // TODO: Way to restore actual orig.?
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
  using namespace anagram;

  // Here we set up our trie data structure, used to organize the dictionary
  // words for fast lookup. Will read it later after we've gathered our input
  // arguments since they can affect what is read.
  TNode *root_node = nullptr;
  TernaryTree trie;
  trie.SetRoot(&root_node);

  // This  sets up a signal handler to re-show the cursor if user ctrl-c's
  struct sigaction sigIntHandler{};
  sigIntHandler.sa_handler = SigtermHandler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, nullptr);

  // Sets a sane level that allows UI but not debug messages
  SET_VERBOSITY_LEVEL(LOG_NORMAL);

  // This parses the arguments and takes subsequent non-dashed arguments
  // as the input (no quotes required)
  AnagramFlags flags{};
  memset(&flags, 0, sizeof(flags));
  flags.tree_engine = flags.allow_dupes = flags.output_directly
    = flags.big_dictionary = 0;
  string word;
  map< string, int > excludeset;
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
              char verbosity;
              if (isdigit(verbosity = argv[i][2])) {
                auto log_level = (LOG_LEVEL) (verbosity - (int) '0');
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
           case 'e': {
              // Parses out comma-separated lists of words to exclude, form:
              // parse out -eword1,word2
              string parse;
              int j = 0;
              const char *nchar = &argv[i][j + 2 - 1];
              do {
                nchar++;
                if (',' == *nchar || !(*nchar)) {
                  std::transform(parse.begin(), parse.end(), parse.begin(),
                    ::tolower);
                  excludeset[parse] = 1;
                  parse = "";
                } else {
                    parse += *nchar;
                }
              } while (*nchar);
            }
            break;
          case 's': {
             flags.print_subset = 1;
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

  // Turns off stream buffering.  This prevents unwanted artifacts in output
  // such as missed carriage returns.
  //setvbuf(stdout, NULL, _IONBF, 0);

  // turns off buffering so text updates show up on console immediately.
  setbuf(stdout, nullptr);

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

  AnagramWorkerParams params{};
  params.trie = &trie;
  params.root_node = root_node;
  params.word = word.c_str();
  params.anagrams = &anagrams;
  params.flags = flags;
  params.thread_index = 0;   // Round-robined in RunJob
  params.excludeset = &excludeset;   // Round-robined in RunJob

  unsigned core_tot = std::thread::hardware_concurrency();
  if (core_tot > 1) {
    // Don't use ALL the cores; use cores - 1
    core_tot -= 1;
  } else {
    core_tot = 1;
  }
  RunJob(core_tot, &params);

  // Iterates through all the findings and spit them out to stdout.
  // Only does so if we are not outputting directly; otherwise the
  // output collection will be empty.
  if (!flags.output_directly) {
    VERBOSE_LOG(LOG_NORMAL, "\r                         \r"
      << COUT_BOLD_WHITE << word.c_str()
      << COUT_BOLD_YELLOW << endl);
    int count = 0;
    for (const auto& i : anagrams) {
      cout << i.first << endl;
      ++count;
    }
    VERBOSE_LOG(LOG_NORMAL,  COUT_BOLD_WHITE << count << " ANAGRAMS FOUND.");
  }
  VERBOSE_LOG(LOG_NONE, COUT_NORMAL_WHITE << COUT_SHOWCURSOR << endl);

  setvbuf(stdout, nullptr, _IONBF, 1024);  // TODO: Way to restore actual orig.?
  return 0;
}
#pragma clang diagnostic pop
