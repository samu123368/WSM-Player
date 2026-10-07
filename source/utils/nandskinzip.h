#pragma once
#include <cstddef>
#include <vector>

// Decode one bounded member in memory. Never extracts files to SD.
bool ReadNandSkinZip(const unsigned char *zip, size_t size, const char *name,
                     std::vector<unsigned char> &out);
