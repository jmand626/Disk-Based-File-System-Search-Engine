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

#include "./WriteIndex.h"

#include <cstdio>    // for (FILE *).
#include <cstring>   // for strlen(), etc.

// We need to peek inside the implementation of a HashTable so
// that we can iterate through its buckets and their chain elements.
extern "C" {
  #include "libhw1/CSE333.h"
  #include "libhw1/HashTable_priv.h"
}
#include "./LayoutStructs.h"
#include "./Utils.h"

namespace hw3 {
//////////////////////////////////////////////////////////////////////////////
// Helper function declarations and constants.

static constexpr int kFailedWrite = -1;

// Helper function to write the docid->filename mapping from the
// DocTable "dt" into file "f", starting at byte offset "offset".
// Returns the size of the written DocTable or a negative value on error.
static int WriteDocTable(FILE* f, DocTable* dt, IndexFileOffset_t offset);

// Helper function to write the MemIndex "mi" into file "f", starting
// at byte offset "offset".  Returns the size of the written MemIndex
// or a negative value on error.
static int WriteMemIndex(FILE* f, MemIndex* mi, IndexFileOffset_t offset);

// Helper function to write the index file's header into file "f".
// Will atomically write the kMagicNumber as its very last operation;
// as a result, if we crash part way through writing an index file,
// it won't contain a valid kMagicNumber and the rest of HW3 will
// know to report an error.  On success, returns the number of header
// bytes written; on failure, a negative value.
static int WriteHeader(FILE* f, int doctable_bytes, int memidx_bytes);

// Function pointer used by WriteHashTable() to write a HashTable's
// HTKeyValue_t element into the index file at a specified byte offset.
//
// Arguments:
//   - f: the file to write into.
//   - offset: the byte offset into "f", at which we should
//             write the element.
//   - kv: a pointer to the key value pair to be interpreted and written.
//
// Returns:
//   - the number of bytes written, or a negative value on error.
typedef int (*WriteElementFn)(FILE* f, IndexFileOffset_t offset,
                              HTKeyValue_t* kv);

// Writes a HashTable into the index file at a specified byte offset.
//
// Writes a header (BucketListHeader), a list of bucket records (BucketRecord),
// then the bucket contents themselves (using a content-specific instance of
// WriteElementFn).
//
// Since this function can write any HashTable, regardless of its contents,
// it is the core functionality of the file.
//
// Arguments:
//   - f: the file to write into.
//   - offset: the byte offset into "f", at which we should write the
//             hashtable.
//   - ht: the hashtable to write.
//   - fn: a function that serializes a single HTKeyValue_t.  Needs to be
//         specific to the hashtable's contents.
//
// Returns:
//   - the number of bytes written, or a negative value on error.
static int WriteHashTable(FILE* f, IndexFileOffset_t offset, HashTable* ht,
                          WriteElementFn fn);

// Helper function used by WriteHashTable() to write a BucketRecord (ie, a
// "bucket_rec" within the hw3 diagrams).
//
// Arguments:
//   - f: the file to write into.
//   - offset: the byte offset into "f", at which we should
//             write the BucketRecord.
//   - numElts: the number of elements in the bucket.  Used to initialize
//              the BucketRecord's contents.
//   - bucketOffset: the offset at which the bucket (not the BucketRecord) is
//                   located in "f".
//
// Returns:
//   - the number of bytes written, or a negative value on error.
static int WriteHTBucketRecord(FILE* f, IndexFileOffset_t offset,
                               int32_t num_elts,
                               IndexFileOffset_t bucket_offset);

// Helper function used by WriteHashTable() to write out a bucket.
//
// Remember that a bucket consists of a LinkedList of elements.  Thus, this
// function writes out a list of ElementPositionRecords, describing the
// location (as a byte offset) of each element, followed by a series of
// elements thesmelves (serialized using an element-specific WriteElementFn).
//
// Arguments:
//   - f: the file to write into.
//   - offset: the byte offset into "f", at which we should write the bucket.
//   - li: the bucket's contents.
//   - fn: a function that serializes a single HTKeyValue_t -- stored as
//         the list's LLPayload_t -- within the LinkedList.
//
// Returns:
//   - the number of bytes written, or a negative value on error.
static int WriteHTBucket(FILE* f, IndexFileOffset_t offset,
                         LinkedList* li, WriteElementFn fn);


//////////////////////////////////////////////////////////////////////////////
// "Writer" functions
//
// Functions that comply with the WriteElementFn signature, to be used when
// writing hashtable elements to disk.

// Writes an element of the IdToName table from a DocTable into
// a specified file at a specified offset.
static int WriteDocidToDocnameFn(FILE* f, IndexFileOffset_t offset,
                                 HTKeyValue_t* kv);


// Writes an element of the MemIndex into a
// a specified file at a specified offset.
static int WriteWordToPostingsFn(FILE* f, IndexFileOffset_t offset,
                                 HTKeyValue_t* kv);

// Writes an element of an inner postings table into
// a specified file at a specified offset.
static int WriteDocIDToPositionListFn(FILE* f, IndexFileOffset_t offset,
                                      HTKeyValue_t* kv);


//////////////////////////////////////////////////////////////////////////////
// WriteIndex

int WriteIndex(MemIndex* mi, DocTable* dt, const char* file_name) {
  // Do some sanity checking on the arguments we were given.
  Verify333(mi != nullptr);
  Verify333(dt != nullptr);
  Verify333(file_name != nullptr);

  // fopen() the file for writing; use mode "wb+" to indicate binary,
  // write mode, and to create/truncate the file.
  FILE* f = fopen(file_name, "wb+");
  if (f == nullptr) {
    return kFailedWrite;
  }

  // Remember that the format of the index file is a header, followed by a
  // doctable, and then lastly a memindex.
  //
  // We write out the doctable and memindex first, since we need to know
  // their sizes before we can calculate the header.  So we'll skip over
  // the header for now.
  IndexFileOffset_t cur_pos = sizeof(IndexFileHeader);

  // Write the document table.
  int dt_bytes = WriteDocTable(f, dt, cur_pos);
  if (dt_bytes == kFailedWrite) {
    fclose(f);
    unlink(file_name);  // delete the file
    return kFailedWrite;
  }
  cur_pos += dt_bytes;

  // STEP 1.
  // Write the memindex.
  // This is a little confusing since memindex is defined after this in this
  // file. This code is similar to the above code writing to a doc table
  int mi_bytes = WriteMemIndex(f, mi, cur_pos);
  if (mi_bytes == kFailedWrite) {
    fclose(f);
    unlink(file_name);  // delete the file
    return kFailedWrite;
  }
  cur_pos += mi_bytes;


  // STEP 2.
  // Finally, backtrack to write the index header and write it.
  // We DO NOT need to write cur_pos += head_bytes here as cur_pos is
  // intialized to a point equal to the sizeof bytes
  int head_bytes = WriteHeader(f, dt_bytes, mi_bytes);
  // Slightly different parameters for this function, from defintion below
  if (head_bytes == kFailedWrite) {
    fclose(f);
    unlink(file_name);  // delete the file
    return kFailedWrite;
  }

  // Clean up and return the total amount written.
  fclose(f);
  return cur_pos;
}


//////////////////////////////////////////////////////////////////////////////
// Helper function definitions

static int WriteDocTable(FILE* f, DocTable* dt, IndexFileOffset_t offset) {
  // Break the DocTable abstraction in order to grab the docid->filename
  // hash table, then serialize it to disk.
  return WriteHashTable(f, offset, DT_GetIDToNameTable(dt),
                        &WriteDocidToDocnameFn);
}

static int WriteMemIndex(FILE* f, MemIndex* mi, IndexFileOffset_t offset) {
  // Use WriteHashTable() to write the MemIndex into the file.  You'll
  // need to pass in the WriteWordToPostingsFn helper function as the
  // final argument.
  return WriteHashTable(f, offset, mi, &WriteWordToPostingsFn);
}

static int WriteHeader(FILE* f, int doctable_bytes, int memidx_bytes) {
  // STEP 3.
  // We need to calculate the checksum over the doctable and index
  // table.  (Note that the checksum does not include the index file
  // header, just these two tables.)
  //
  // Use fseek() to seek to the right location, and use a CRC32 object
  // to do the CRC checksum calculation, feeding it characters that you
  // read from the index file using fread().
  // Seek to the start of the doctable.
  CRC32 crc;

  if (fseek(f, sizeof(IndexFileHeader), SEEK_SET) != 0) {
    // fseek takes in sizeof the header from SEEK_SET, where SEEK_SET is macro
    // for fseek that means start at the begining of the file. We sent in that
    // size of the header since we need the doctable and index to calculate
    // checksum and they come in after the header in our file

    return kFailedWrite;
  }

  size_t read;
  // Size_t is a general type we use here to guruantee there are no issues with
  // how we store the values we get from memory
  char buff[1024];
  size_t i;

  while (1) {
    read = fread(buff, 1, 1024, f);
    // Since 1024 is the max we will accept, and we are reading chars to CRC
    // each of them individually
    if (read == 0) break;

    for (i = 0; i < read; i++) {
      crc.FoldByteIntoCRC(buff[i]);
    }
  }


  // Note that in the next piece of code written by staff
  // we call crc.GetFinalCRC directly into constructor for IndexFile Header
  // , as this is the last piece of calculating a checksum

  // Write the header fields.  Be sure to convert the fields to
  // network order before writing them!
  IndexFileHeader header(kMagicNumber, crc.GetFinalCRC(),
                         doctable_bytes, memidx_bytes);
  header.ToDiskFormat();

  if (fseek(f, 0, SEEK_SET) != 0) {
    return kFailedWrite;
  }
  if (fwrite(&header, sizeof(IndexFileHeader), 1, f) != 1) {
    return kFailedWrite;
  }

  // Use fsync to flush the header field to disk.
  Verify333(fsync(fileno(f)) == 0);

  // We're done!  Return the number of header bytes written.
  return sizeof(IndexFileHeader);
}

static int WriteHashTable(FILE* f, IndexFileOffset_t offset, HashTable* ht,
                          WriteElementFn fn) {
  // Write the HashTable's header, which consists simply of the number of
  // buckets.
  BucketListHeader header(ht->num_buckets);
  header.ToDiskFormat();
  if (fseek(f, offset, SEEK_SET) != 0) {
    return kFailedWrite;
  }
  if (fwrite(&header, sizeof(BucketListHeader), 1, f) != 1) {
    return kFailedWrite;
  }

  // The byte offset of the next BucketRecord we want to write.  Remember that
  // the bucket records are located after the bucket header.
  IndexFileOffset_t record_pos = offset + sizeof(BucketListHeader);

  // The byte offset of the next bucket we want to write.  Reember that
  // the buckets are placed after the bucket header and the entire list
  // of BucketRecords.
  IndexFileOffset_t bucket_pos = offset + sizeof(BucketListHeader)
    + (ht->num_buckets) * sizeof(BucketRecord);

  // Iterate through the hashtable contents, first writing each bucket record
  // (ie, the BucketRecord) and then jumping forward in the file to write
  // the bucket contents itself.
  //
  // Be sure to handle the corner case where the bucket's chain is
  // empty.  For that case, you still have to write a record for the
  // bucket, but you won't write a bucket.
  for (int i = 0; i < ht->num_buckets; i++) {
  // STEP 4.
    LinkedList* list;
    // Hash table contents

    int num_elements;
    int bucket = 0;
    // Bytes for the bucket, not the bucket record

    int HTBt_bytes = 0;
    // Bytes for the bucket record, not the bucket itself


    num_elements = 0;
    list = ht->buckets[i];
    if (list != nullptr) {
      num_elements = LinkedList_NumElements(list);
    }

    // notice that there is no check for if the chain is empty here, which we
    // don't need because we want to write records even if the chain has no
    // elements
    HTBt_bytes = WriteHTBucketRecord(f, record_pos, num_elements, bucket_pos);
    if (HTBt_bytes == kFailedWrite) return kFailedWrite;

    record_pos += HTBt_bytes;
    // since the method to write bucket records returns the num of bytes which
    // means we can either move our pointer correctly, or -1, which is
    // kfailedwrite

    if (list != nullptr && num_elements > 0) {
      // unlike bucketrecords, we actually need values to write buckets
      bucket = WriteHTBucket(f, bucket_pos, list, fn);
      if (bucket == kFailedWrite) {
        return kFailedWrite;
      }

      bucket_pos += bucket;
    }
  }

  // Calculate and return the total number of bytes written.
  return bucket_pos - offset;
}

static int WriteHTBucketRecord(FILE* f,
                               IndexFileOffset_t offset,
                               int32_t num_elts,
                               IndexFileOffset_t bucket_offset) {
  // STEP 5.
  // Initialize a BucketRecord in network byte order.
  // If we look at the declaration for this func, we see that num elts and
  // bucket_offset are what we need to intialize bucket_offset
  BucketRecord record(num_elts, bucket_offset);
  record.ToDiskFormat();
  // "The on-disk format is big-endian, sometimes called "network order""


  // fseek() to where we want to write this record.
  if (fseek(f, offset, SEEK_SET) != 0) {
    return kFailedWrite;
  }

  // STEP 6.
  // Write the BucketRecord.
  int retVal = fwrite(&record, sizeof(BucketRecord), 1, f);
  if (retVal != 1) {
    return kFailedWrite;
  }


  // Calculate and return how many bytes we wrote.
  return sizeof(BucketRecord);
}

static int WriteHTBucket(FILE* f, IndexFileOffset_t offset, LinkedList* li,
                         WriteElementFn fn) {
  int num_elts = LinkedList_NumElements(li);
  if (num_elts == 0) {
    // Not an error; nothing to write
    return 0;
  }

  // The byte offset of the next ElementPositionRecord we want to write.
  IndexFileOffset_t record_pos = offset;

  // The byte offset of the next element we want to write.  Remember that
  // the elements are placed after the entire list of ElementPositionRecords.
  IndexFileOffset_t element_pos = offset
    + sizeof(ElementPositionRecord) * num_elts;

  // Iterate through the list contents, first writing each entry's (ie, each
  // payload's) ElementPositionRecord and then jumping forward in the file
  // to write the element itself.
  //
  // Be sure to write in network order, and use the "fn" argument to write
  // the element (ie, the list payload) itself.
  LLIterator* it = LLIterator_Allocate(li);
  Verify333(it != nullptr);
  for (int i = 0; i < num_elts; i++) {
    // STEP 7.
    // fseek() to the where the ElementPositionRecord should be written,
    // then fwrite() it in network order.
    int fcheck = 0;
    ElementPositionRecord record(element_pos);
    record.ToDiskFormat();

    // Once again, SEEK_SET starts our search from the start, while record_pos
    // is already at the correct place
    fcheck = fseek(f, record_pos, SEEK_SET);
    if (fcheck != 0) return kFailedWrite;

    // in the syntax of fwrite, the first param is a pointer to where we get
    // our data, and we allocate a new ElementPosistionRecord for this
    // 1 is the third param (num of elements), since we call this method once
    // per each call to fwrite
    fcheck = fwrite(&record, sizeof(ElementPositionRecord), 1, f);


    // STEP 8.
    // Write the element itself, using fn.
    HTKeyValue_t* kv;
    int element_bytes;
    LLIterator_Get(it, reinterpret_cast<LLPayload_t*>(&kv));
    // So what on earth is going on here? Because we are intersection C and C++
    // code here, we need to cast to account for strange differences, and kv
    // needs to be truned into a LLPayload when getting from the iterator to
    // make sure the types match

    element_bytes = fn(f, element_pos, kv);
    if (element_bytes == kFailedWrite) return kFailedWrite;
    element_pos += element_bytes;


    // Advance to the next element in the chain, updating our offsets.
    record_pos += sizeof(ElementPositionRecord);

    // element_pos += 1;  you may need to change this logic
    // This one line broke my code many times :(

    LLIterator_Next(it);
  }
  LLIterator_Free(it);

  // Return the total amount of data written.
  return element_pos - offset;
}


//////////////////////////////////////////////////////////////////////////////
// "Writer" functions

// This write_element_fn is used to write a doc_id->doc_name mapping
// element, i.e., an element of the "doctable" table.
static int WriteDocidToDocnameFn(FILE* f, IndexFileOffset_t offset,
                                 HTKeyValue_t* kv) {
  // STEP 9.
  // Determine the file name length
  char* f_name = static_cast<char*>(kv->value);
  // This docctable has doc_names as values. Those are obviously char*s...
  int16_t f_name_length = strlen(f_name);


  // fwrite() the docid from "kv".  Remember to convert to
  // disk format before writing.
  DoctableElementHeader header(kv->key, f_name_length);
  header.ToDiskFormat();

  // fseek() to the provided offset and then write the header.
  if (fseek(f, offset, SEEK_SET) != 0) {
    return kFailedWrite;
  }
  if (fwrite(&header, sizeof(DoctableElementHeader), 1, f) != 1) {
    return kFailedWrite;
  }

  // STEP 10.
  // fwrite() the file name.  We don't write the null-terminator from the
  // string, just the characters, since we've already written a length
  // field for the string.
  int fcheck = fwrite(f_name, f_name_length, 1, f);
  // There is of course 1 file name so thats why there is a 1 there
  if (fcheck != 1) return kFailedWrite;


  // STEP 11.
  // Calculate and return the total amount written.
  // In between step 9 and 10, the code actually writes the doctableelemheader,
  // which means we want to skip it here
  return sizeof(DoctableElementHeader) + f_name_length;
}

// This write_element_fn is used to write a DocID + position list
// element (i.e., an element of a nested docID table) into the file at
// offset 'offset'.
static int WriteDocIDToPositionListFn(FILE* f,
                                      IndexFileOffset_t offset,
                                      HTKeyValue_t* kv) {
  // Extract the docID from the HTKeyValue_t.
  DocID_t doc_id = static_cast<DocID_t>(kv->key);

  // Extract the positions LinkedList from the HTKeyValue_t and
  // determine its size.
  LinkedList* positions = static_cast<LinkedList*>(kv->value);
  int num_positions = LinkedList_NumElements(positions);

  // STEP 12.
  // Write the header, in disk format.
  // You'll need to fseek() to the right location in the file.
  DocIDElementHeader header(doc_id, num_positions);
  // Remember, constructors like header and recorder are defined in
  // LayoutSturcts.h
  int fcheck;
  header.ToDiskFormat();

  // Seek to 'offset'
  fcheck = fseek(f, offset, SEEK_SET);
  if (fcheck != 0) return kFailedWrite;

  // write header at offset
  fcheck = fwrite(&header, sizeof(DocIDElementHeader), 1, f);
  if (fcheck != 1) return kFailedWrite;
  // The return values for errors for these two functions are different because
  // the creators of C++ hate the sun, kittens, and chocolate


  // Loop through the positions list, writing each position out.
  DocIDElementPosition position;
  LLIterator* it = LLIterator_Allocate(positions);
  Verify333(it != nullptr);
  for (int i = 0; i < num_positions; i++) {
    // STEP 13.
    // Get the next position from the list.
    LLPayload_t nextPos;
    LLIterator_Get(it, &nextPos);

    // Need nextPos as its a return param
    position.position = static_cast<DocPositionOffset_t>(
    reinterpret_cast<uint64_t>(nextPos));
    // First, posistions need to be integers, and then posistion is a DocID
    // ElementPosistion.

    // STEP 14.
    // Truncate to 32 bits, then convert it to network order and write it out.

    position.ToDiskFormat();
    if (fwrite(&position, sizeof(DocIDElementPosition), 1, f) != 1) {
      return kFailedWrite;
    }

    // Advance to the next position.
    LLIterator_Next(it);
  }
  LLIterator_Free(it);

  // STEP 15.
  // Calculate and return the total amount of data written.
  return sizeof(DocIDElementHeader) + (num_positions *
  sizeof(DocIDElementPosition));
  // wrote header in step 12, and then wrote out a LIST of posistions in
  // steps 13-14
}


// This write_element_fn is used to write a WordPostings
// element into the file at position 'offset'.
static int WriteWordToPostingsFn(FILE* f,
                                 IndexFileOffset_t offset,
                                 HTKeyValue_t* kv) {
  // Extract the WordPostings from the HTKeyValue_t.
  WordPostings* wp = static_cast<WordPostings*>(kv->value);
  Verify333(wp != nullptr);

  // STEP 16.
  // Prepare the wordlen field.
  int16_t word_bytes = strlen(wp->word);  // you may need to change this logic

  // Write the nested DocID->positions hashtable (i.e., the "docID
  // table" element in the diagrams).  Use WriteHashTable() to do it,
  // passing it the wp->postings table and using the
  // WriteDocIDToPositionListFn helper function as the final parameter.
  int ht_bytes = WriteHashTable(
    f,
    offset + sizeof(WordPostingsHeader) + word_bytes,
    wp->postings,
    &WriteDocIDToPositionListFn);

  if (ht_bytes == kFailedWrite) {
    return kFailedWrite;
  }

  // STEP 17.
  // Write the header, in network order, in the right place in the file.
  WordPostingsHeader header(word_bytes, ht_bytes);
  header.ToDiskFormat();
  int fcheck;

  fcheck = fseek(f, offset, SEEK_SET);
  if (fcheck != 0) return kFailedWrite;

  fcheck = fwrite(&header, sizeof(WordPostingsHeader), 1, f);
  if (fcheck != 1) return kFailedWrite;

  // STEP 18.
  // Write the word itself, excluding the null terminator, in the right
  // place in the file.
  char* word = wp -> word;
  if (fwrite(word, word_bytes, 1, f) != 1) {
    return kFailedWrite;
  }


  // STEP 19.
  // Calculate and return the total amount of data written.
  return ht_bytes + sizeof(header) + word_bytes;
}
}  // namespace hw3
