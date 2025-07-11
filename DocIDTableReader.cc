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

#include "./DocIDTableReader.h"

#include <list>      // for std::list
#include <cstdio>    // for (FILE*)

#include "./LayoutStructs.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

using std::list;

namespace hw3 {

// The constructor for DocIDTableReader calls the constructor
// of HashTableReader(), its base class. The base class takes
// care of taking ownership of f and using it to extract and
// cache the number of buckets within the table.
DocIDTableReader::DocIDTableReader(FILE* f, IndexFileOffset_t offset)
  : HashTableReader(f, offset) { }

bool DocIDTableReader::LookupDocID(
     const DocID_t& doc_id, list<DocPositionOffset_t>* const ret_val) const {
  // Use the base class's `LookupElementPositions` function to
  // walk through the docIDtable and get back a list of offsets
  // to elements in the bucket for this docID.
  auto elements = LookupElementPositions(doc_id);

  // If the list of elements is empty, we're done.
  if (elements.empty())
    return false;

  // Iterate through all of elements, looking for our docID.
  for (IndexFileOffset_t& curr_element : elements) {
    // STEP 1.
    // Slurp the next docid out of the current element.
    DocIDElementHeader curr_header;
    Verify333(fseek(file_, curr_element, SEEK_SET) == 0);
    // Read the header from the current position
    Verify333(fread(&curr_header, sizeof(curr_header), 1, file_) == 1);
    // This is almost exactly the same as the other ones, espically from Doc
    // TableReader...
    curr_header.ToHostFormat();


    // Is it a match?
    if (curr_header.doc_id == doc_id) {
      // STEP 2.
      // Yes!  Extract the positions themselves, appending to
      // std::list<DocPositionOffset_t>.  Be sure to push in the right
      // order, adding to the end of the list as you extract
      // successive positions.
      for (int i = 0; i < curr_header.num_positions; i++) {
        // Read each position
        DocIDElementPosition pos;
        Verify333(fread(&pos, sizeof(DocIDElementPosition), 1, file_)
         == 1);
        pos.ToHostFormat();
        // Add to the end of the list, which is why we use push back
        ret_val->push_back(pos.position);
      }

      // STEP 3.
      // Return the positions list through the output parameter,
      // and return true.
      // Stranglely enough we dont need to add anything here, as the function
      // already returns and we built up our list in the previous section

      return true;
    }
  }

  // We failed to find a matching docID, so return false.
  return false;
}

list<DocIDElementHeader> DocIDTableReader::GetDocIDList() const {
  // This will be our returned list of docIDs within this table.
  list<DocIDElementHeader> doc_id_list;

  // Go through *all* of the buckets of this hashtable, extracting
  // out the docids in each element and the number of word positions
  // for the each docid.
  for (int i = 0; i < header_.num_buckets; i++) {
    // STEP 4.
    // Seek to the next BucketRecord.  The "offset_" member
    // variable stores the offset of this docid table within
    // the index file.
    IndexFileOffset_t bt_offset = offset_ + sizeof(BucketListHeader) +
    (i * sizeof(BucketRecord));
    // Another word for the header is num_buckets, as gleamed from the images
    // in the begining of the homework. We need to get past them, and however
    // many bucket recs we got before we get to the one we want, with typical
    // fseek params
    Verify333(fseek(file_, bt_offset, SEEK_SET) == 0);


    // STEP 5.
    // Read in the chain length and bucket position fields from
    // the bucket_rec.
    BucketRecord bucket_rec;
    Verify333(fread(&bucket_rec, sizeof(BucketRecord), 1, file_) == 1);
    // I think we are all starting to see why part a was considered the long
    // one, but it is a nice change of pace. We seeked in step 4 to our bt_rec,
    // and since its soley made out of the chain length and bucket pos fields
    // we read it all in at once
    bucket_rec.ToHostFormat();


    // Sweep through the next bucket, iterating through each
    // chain element in the bucket.
    off_t element_offset;
    for (int j = 0; j < bucket_rec.chain_num_elements; j++) {
      // Seek to chain element's position field in the bucket header.
      element_offset = bucket_rec.position + j * sizeof(ElementPositionRecord);
      Verify333(fseek(file_, element_offset, SEEK_SET) == 0);

      // STEP 6.
      // Read the next element position from the bucket header.
      // and seek to the element itself.
      ElementPositionRecord element_pos;
      // We go in the opposite order of whats expected because remember, this
      // inner loop goes through the chain of elements stored within each
      // element, and both the record and the elements are stored with the idea
      // of store records that point to where we want to go
      Verify333(fread(&element_pos, sizeof(ElementPositionRecord), 1, file_)
      == 1);
      element_pos.ToHostFormat();

      // Now that we got the record and reformatted it, we just seek to where
      // we need for the next piece of code
      Verify333(fseek(file_, element_pos.position, SEEK_SET) == 0);


      // STEP 7.
      // Read in the docid and number of positions from the element.
      DocIDElementHeader elem;
      Verify333(fread(&elem, sizeof(DocIDElementHeader), 1, file_) == 1);
      elem.ToHostFormat();

      // Append it to list, push back as the read starts at the logical begin-
      // ing and goes down
      doc_id_list.push_back(elem);
    }
  }

  // Done!  Return the result list.
  return doc_id_list;
}

}  // namespace hw3
