#ifndef _OCCUPANCY_HASH_H_
#define  _OCCUPANCY_HASH_H_

#include <memory.h>

class OccupancyHash
{
 public:
  OccupancyHash() {
    memset((void *) occupancy_index_, 0, sizeof(occupancy_index_) * sizeof(size_t));
    memset((void *) index_index_, 0, sizeof(index_index_));
    memset((void *) char_count_, 0, sizeof(char_count_));
    index_ptr_ = 0;
  };
  ~OccupancyHash() {};

  inline void clear() {
    //VERBOSE_LOG(LOG_INFO,"clear called" << std::endl);
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
  inline size_t GetCharCount(char char_index) {
    return (size_t) char_count_[ (size_t) char_index];
  }

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

  void DebugOut()
  {
    size_t debug_index_ptr = index_ptr_;
    while (debug_index_ptr) {
      --debug_index_ptr;
      VERBOSE_LOG(LOG_INFO,
        occupancy_index_[debug_index_ptr] << "("
        << (size_t) char_count_[occupancy_index_[debug_index_ptr]]
        << "," << (size_t) index_index_[occupancy_index_[debug_index_ptr]]
        << ")"
      );
    }
    VERBOSE_LOG(LOG_INFO, "  index_ptr:" << index_ptr_);
    VERBOSE_LOG(LOG_INFO, std::endl);
  }

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

    if (!result) {
      indexindex = b.index_ptr_;
      while (indexindex) {
        --indexindex;
        if (char_count_[(size_t)b.occupancy_index_[indexindex]] <
          b.char_count_[(size_t)b.occupancy_index_[indexindex]])
          return -1; // we have lesser count on at least one unique char
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
      // to the indexes.
      if (!char_count_[char_index]) {
        char_count_[char_index] = 1;
        index_index_[char_index] = index_ptr_;
        occupancy_index_[index_ptr_] = char_index;
        ++index_ptr_;
      } else {
        char_count_[char_index]++;
      }
    }
  }

 private:
  unsigned char    occupancy_index_[64]; // indexes into char_count_
  unsigned char    index_index_[256]; // maps char to occupancy_index_
  unsigned char    char_count_[256]; // Size of domain (7/8 bit only)
  size_t           index_ptr_;       // # of unique chars/occupancy_index_ tot
};
#endif // #ifndef _OCCUPANCY_HASH_H_
