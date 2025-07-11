/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Autumn Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include "./HashTableReader.h"

#include <stdint.h>  // for uint32_t, etc.
#include <cstdio>    // for (FILE *).
#include <list>      // for std::list.

#include "./LayoutStructs.h"

extern "C" {
  #include "libhw1/CSE333.h"
}
#include "./Utils.h"  // for FileDup().


using std::list;

namespace hw3 {

HashTableReader::HashTableReader(FILE* f, IndexFileOffset_t offset)
  : file_(f), offset_(offset) {
  // STEP 1.
  // fread() the bucket list header in this hashtable from its
  // "num_buckets" field, and convert to host byte order.

  // Because we are given an offset, we have to fseek to the correct place
  // first before we read
  Verify333(fseek(f, offset, SEEK_SET) == 0);

  // Now we can actually read the header in and conver it
  Verify333(fread(&header_.num_buckets, sizeof(header_.num_buckets), 1, f)
  == 1);
  header_.num_buckets = ntohl(header_.num_buckets);
}

HashTableReader::~HashTableReader() {
  fclose(file_);
  file_ = nullptr;
}

list<IndexFileOffset_t>
HashTableReader::LookupElementPositions(HTKey_t hash_key) const {
  // Figure out which bucket the hash value is in.  We assume
  // hash values are mapped to buckets using the modulo (%) operator.
  int bucket_num = hash_key % header_.num_buckets;

  // Figure out the offset of the "bucket_rec" field for this bucket.
  IndexFileOffset_t bucket_rec_offset =
      offset_ + sizeof(BucketListHeader) + sizeof(BucketRecord) * bucket_num;

  // STEP 2.
  // Read the "chain len" and "bucket position" fields from the
  // bucket record, and convert from network to host order.
  BucketRecord bucket_rec;
  Verify333(fseek(file_, bucket_rec_offset, SEEK_SET) == 0);
  // Once again, the fact that we need an offset clearly means that we are not
  // starting at the begining of file and therefore we need to seek

  Verify333(fread(&bucket_rec, sizeof(bucket_rec), 1, file_) == 1);
  // there have been alot of these reads so far, and they are practically the
  // same

  // I learned the actual name of these ones from layout structs.h, go there
  // for more info on them
  bucket_rec.chain_num_elements = ntohl(bucket_rec.chain_num_elements);
  bucket_rec.position = ntohl(bucket_rec.position);



  // This will be our returned list of element positions.
  list<IndexFileOffset_t> ret_val;

  // STEP 3.
  // Read the "element positions" fields from the "bucket" header into
  // the returned list.  Be sure to insert into the list in the
  // correct order (i.e., append to the end of the list).
  Verify333(fseek(file_, bucket_rec.position, SEEK_SET) == 0);
  // We seek to the bucket pos since each bucket rec 'maps' to a bucket that
  // contains a list of elements

  int size = bucket_rec.chain_num_elements;
  for (int i = 0; i < size; i++) {
    ElementPositionRecord element_pos;
    Verify333(fread(&element_pos, sizeof(element_pos), 1, file_) == 1);

    // We have to keep remembering to do this since we just read it from a file
    element_pos.position = ntohl(element_pos.position);
    // Push back goes to the end
    ret_val.push_back(element_pos.position);
  }

  // Return the list.
  return ret_val;
}
}  // namespace hw3
