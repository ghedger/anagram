#include "occupancy_hash.h"
namespace anagram {

int anagram::OccupancyHash::constructor_count_ = 0;
void OccupancyHash::PrintConstructorCalls()
{
  VERBOSE_LOG(LOG_INFO, "Occupancy constructor calls: " << OccupancyHash::constructor_count_ << std::endl);
}


} // namespace anagram
