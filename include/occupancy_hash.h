#ifndef _OCCUPANCY_HASH_H_
#define  _OCCUPANCY_HASH_H_

#include <memory.h>
#include <iostream>

#include "anagram_log.h"

namespace anagram {

// OccupancyHash
// This class maintains a sparse lookup table of counts of unique characters
// in a string or phrase, and provides limited algebraic operations on them.
// TODO: This isn't really a hash and should probably be renamed.
class OccupancyHash
{
 public:
  OccupancyHash() {
    OccupancyHash::constructor_count_++;
    memset((void *) occupancy_index_, 0, sizeof(occupancy_index_) * sizeof(size_t));
    memset((void *) index_index_, 0, sizeof(index_index_));
    memset((void *) char_count_, 0, sizeof(char_count_));
    index_ptr_ = 0;
  };
  OccupancyHash(const char *word) {
    OccupancyHash();
    GetCharCountMap(word);
  };
  ~OccupancyHash() {};

  static void PrintConstructorCalls();
  inline void clear() {
    //VERBOSE_LOG(LOG_INFO,"Clear called" << std::endl);
    while(index_ptr_) {
      --index_ptr_;
      char_count_[(size_t)occupancy_index_[index_ptr_]] = 0;
      index_index_[(size_t)occupancy_index_[index_ptr_]] = 0;
      occupancy_index_[index_ptr_] = 0;
    }
  }

  inline void AddChar(char char_index) {
    size_t index = (size_t) char_index;
    ++char_count_[index];
    size_t index_index = (size_t) index_index_[index];
    if (index_index) {
      ++occupancy_index_[index_index];
    } else {
      index_index_[index] = index_ptr_;
      occupancy_index_[index_ptr_] = index;
      ++index_ptr_;
    }
  }

  // GetCharCount
  // Entry: char_index
  inline size_t GetCharCount(char char_index) {
    return (size_t) char_count_[ (size_t) char_index];
  }
  // Add one hash to the other.
  // Entry: OccupancyHash against which to compare
  inline void operator+=(const OccupancyHash& b) {
    size_t char_index;
    size_t b_index = b.index_ptr_;
    while (b_index) {
      --b_index;
      char_index = (size_t) b.occupancy_index_[b_index];
      // Does "a" have this character occupied? If not, add to index...
      if (!char_count_[char_index]) {
        index_index_[char_index] = index_ptr_;
        char_count_[char_index] = b.char_count_[char_index];  // will add below...
        occupancy_index_[index_ptr_] = char_index;
        ++index_ptr_;
      } else {
        char_count_[char_index] += b.char_count_[char_index];
      }
    }
  }

  // DebugOut
  // Spit out relevant debugging info.
  void DebugOut()
  {
    size_t debug_index_ptr = index_ptr_;
    while (debug_index_ptr) {
      --debug_index_ptr;
      VERBOSE_LOG(LOG_INFO,
        occupancy_index_[debug_index_ptr] << "("
        << (size_t) char_count_[(size_t)occupancy_index_[debug_index_ptr]]
        << "," << (size_t) index_index_[(size_t)occupancy_index_[debug_index_ptr]]
        << ")"
      );
    }
    VERBOSE_LOG(LOG_INFO, "  index_ptr:" << index_ptr_);
    VERBOSE_LOG(LOG_INFO, std::endl);
  }

  size_t GetIndexPtr() {
    return index_ptr_;
  }

  // Compare
  // Returns a modified lexical comparison of two OccupancyHashes.
  // Entry: b hash to compare
  //  - if a has more unique letters than b OR one with higher count, a > b
  //  - if a has fewer unique letters than b, a < b
  // - if all of a's unique letters are in b but b has one not in a, a < b
  // - if the unique counts are equal and all letters common, a == b
  // Note: I opted for explicit compare rather than operator override.
  // Exit: 1 == has characters not in b OR higher count of char in b
  //       0 == same count of same characters as b (complete anagram)
  //      -1 == has some of the characters in b, all less or equal count
  //      -2 == has all the characters in b but some fewer count
  int Compare(const OccupancyHash& b)
  {
    int result = 0;

    // First, just check characters; if b has more character, we are less than;
    // if b as fewer unique characters, we are greater.
    if (b.index_ptr_ > index_ptr_)
      result = -1;  // we have fewer unique characters than b; we are less than
                    // but if we have more of one specific character, supercede
    if (b.index_ptr_ < index_ptr_)
      return 1; // we have more unique characters than b; no need to continue.

    // Okay, same # of unique characters, so we need to iterate through them;
    // We will determine if they are the same characters, and if so,
    // how the counts match up.
    size_t indexindex = index_ptr_;
    while (indexindex) {
      --indexindex;
      if (char_count_[(size_t)occupancy_index_[indexindex]] >
        b.char_count_[(size_t)occupancy_index_[indexindex]]) {
        result = 1; // we have greater count on at least one unique char
        break;      // No need to coninue... this greaterness supercedes
                    // any previously-calculated lessnesss.
      }
      if (char_count_[(size_t)occupancy_index_[indexindex]] <
        b.char_count_[(size_t)occupancy_index_[indexindex]]) {
        result = -1; // we have lesser count on at least one unique char
                     // However, we must continue, since greater trumps less
      }
    }

    // If we've gotten this far, it means "a" is a complete subset of "b";
    // now we'll check whether "b" and "a" are congruent or if are some
    // letters in "b" not present in "a".
    if (!result) {
      indexindex = b.index_ptr_;
      while (indexindex) {
        --indexindex;
        if (char_count_[(size_t)b.occupancy_index_[indexindex]] <
          b.char_count_[(size_t)b.occupancy_index_[indexindex]])
          return -1; // we have lower count on at least one unique char
      }
    }

    return result;
  }

  // IsSubset
  // Determines if the candidate is a complete subset of b
  // Entry: b to compare
  // Exit: true == is complete subset
  bool IsSubset(const OccupancyHash& b)
  {
    bool result = true; // assume success
    // Loop through the candidate and ensure that it contains
    // no characters that are not also part of the master,
    // and that of the common characters the candidate does not
    // exceed the count for the master (i.e. candidate has the same or
    // fewer letter "a"s than master)
    size_t indexindex = index_ptr_;
    while (indexindex) {
      --indexindex;
      if (b.char_count_[(size_t)occupancy_index_[indexindex]] <
          char_count_[(size_t)occupancy_index_[indexindex]] ) {
        result = false;
        break;
      }
    }

    return result;
  }

  // GetCharCountMap
  // Count the # of unique characters in a string.
  // Entry: pointer to string
  void GetCharCountMap(const char *word)
  {
    // This counts the characters, ignoring spaces.
    size_t char_index;
    for (size_t i = 0; (char_index = (size_t) word[i]); ++i) {
      if (0x20 == char_index)   // skip spaces
        continue;
      // If this character is new to this occupancy matrix, add it
      // to the indexes and increase the # of unique characters.
      if (!char_count_[char_index]) {
        char_count_[char_index] = 1;
        index_index_[char_index] = index_ptr_;
        occupancy_index_[index_ptr_] = char_index;
        ++index_ptr_;   // # of unique characters
      } else {
        char_count_[char_index]++;
      }
    }
  }

 private:
  static const int kMaxCharsPerWord = 40;
  char    occupancy_index_[kMaxCharsPerWord]; // indexes into char_count_
  char    index_index_[256]; // maps char to occupancy_index_
  char    char_count_[256]; // Size of domain (7/8 bit only)
  size_t  index_ptr_;       // # of unique chars/occupancy_index_ tot
  static int constructor_count_;
};
} // namespace anagram
#endif // #ifndef _OCCUPANCY_HASH_H_
