#ifndef _ANAGRAM_FLAGS_H_
#define _ANAGRAM_FLAGS_H_

// AnagramFlags
// This structure defines a 32-bit word of flags
struct AnagramFlags {
  unsigned int tree_engine : 1;
  unsigned int allow_dupes : 1;
  unsigned int output_directly : 1;
  unsigned int big_dictionary: 1;
  unsigned int print_subset : 1;
};

#endif // #ifndef _ANAGRAM_FLAGS_H_
