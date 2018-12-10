#ifndef _ANAGRAM_FLAGS_H_
#define _ANAGRAM_FLAGS_H_

struct AnagramFlags {
  unsigned int tree_engine : 1;
  unsigned int allow_dupes : 1;
  unsigned int output_directly : 1;
  unsigned int big_dictionary: 1;
};

#endif // #ifndef _ANAGRAM_FLAGS_H_
