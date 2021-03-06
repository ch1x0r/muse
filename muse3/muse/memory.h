//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: memory.h,v 1.4.2.3 2009/12/15 22:08:50 spamatica Exp $
//
//  (C) Copyright 2003-2004 Werner Schweer (ws@seh.de)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdio.h>
#include <stdlib.h>

// NOTE: Keep this code in case we need a dimensioned pool!
#if 0
// most of the following code is based on examples
// from Bjarne Stroustrup: "Die C++ Programmiersprache"

//---------------------------------------------------------
//   Pool
//---------------------------------------------------------

class Pool {
      struct Verweis {
            Verweis* next;
            };
      struct Chunk {
            enum { size = 4 * 2048 };
            Chunk* next;
            char mem[size];
            };
      // Gives about 300 bytes maximum request @ 8 bytes item size.
      enum { dimension = 40 };
      Chunk* chunks[dimension];
      Verweis* head[dimension];
      Pool(Pool&);
      void operator=(Pool&);
      void grow(int idx);

   public:
      Pool();
      ~Pool();
      void* alloc(size_t n);
      void free(void* b, size_t n);
      };

//---------------------------------------------------------
//   alloc
//---------------------------------------------------------

inline void* Pool::alloc(size_t n)
      {
      if (n == 0)
            return 0;
      int idx = ((n + sizeof(unsigned long) - 1) / sizeof(unsigned long)) - 1;
      if (idx >= dimension) {
            printf("panic: alloc %zd %d %d\n", n, idx, dimension);
            exit(-1);
            }
      if (head[idx] == 0)
            grow(idx);
      Verweis* p = head[idx];
      head[idx] = p->next;
      return p;
      }

//---------------------------------------------------------
//   free
//---------------------------------------------------------

inline void Pool::free(void* b, size_t n)
      {
      if (b == 0 || n == 0)
            return;
      int idx = ((n + sizeof(unsigned long) - 1) / sizeof(unsigned long)) - 1;
      if (idx >= dimension) {
            printf("panic: free %zd %d %d\n", n, idx, dimension);
            exit(-1);
            }
      Verweis* p = static_cast<Verweis*>(b);
      p->next = head[idx];
      head[idx] = p;
      }

extern Pool audioRTmemoryPool;
extern Pool midiRTmemoryPool;

#endif


//---------------------------------------------------------
//   TypedMemoryPool
//   Most of the following code is based on examples
//    from Bjarne Stroustrup: "Die C++ Programmiersprache"
//---------------------------------------------------------

template <typename T, int itemsPerChunk> class TypedMemoryPool
{
      struct Verweis {
            Verweis* next;
            };
      struct Chunk {
            enum { size = itemsPerChunk * sizeof(T) };
            Chunk* next;
            char mem[size];
            };
      Chunk* chunks;
      Verweis* head;
      TypedMemoryPool(TypedMemoryPool&);
      void operator=(TypedMemoryPool&);
      
      void grow()
      {
        const int esize = sizeof(T);
        Chunk* n    = new Chunk;
        n->next     = chunks;
        chunks = n;
        const int nelem = Chunk::size / esize;
        char* start     = n->mem;
        char* last      = &start[(nelem-1) * esize];
        for(char* p = start; p < last; p += esize)
          reinterpret_cast<Verweis*>(p)->next =
            reinterpret_cast<Verweis*>(p + esize);
        reinterpret_cast<Verweis*>(last)->next = 0;
        head = reinterpret_cast<Verweis*>(start);
      }

   public:
      TypedMemoryPool()
      {
        head   = 0;
        chunks = 0;
        grow();  // preallocate
      }
      
      ~TypedMemoryPool()
      {
        Chunk* n = chunks;
        while (n)
        {
          Chunk* p = n;
          n = n->next;
          delete p;
        }
      }
      
      void* alloc(size_t items)
      {
        if(items == 0)
          return 0;
        if(items != 1)
        {
          printf("panic: TypedMemoryPool::alloc items requested:%u != 1\n", 
                (unsigned int)items);
          exit(-1);
        }
        if(head == 0)
          grow();
        Verweis* p = head;
        head = p->next;
        return p;
      }
      
      void free(void* b, size_t items)
      {
        if(b == 0 || items == 0)
          return;
        if(items != 1)
        {
          printf("panic: TypedMemoryPool::free items requested:%u != 1\n", 
                (unsigned int)items);
          exit(-1);
        }
        Verweis* p = static_cast<Verweis*>(b);
        p->next = head;
        head = p;
      }
};

//---------------------------------------------------------
//   MemoryQueue
//   An efficient queue which grows by fixed chunk sizes,
//    for single threads only.
//---------------------------------------------------------

class MemoryQueue {
      struct Chunk
      {
        enum { ChunkSize = 8 * 1024 };
        Chunk* _next;
        char _mem[ChunkSize];
      };
      Chunk* _startChunk;
      Chunk* _endChunk;
      Chunk* _curWriteChunk;
      size_t _curSize;
      size_t _curOffest;
      
      MemoryQueue(MemoryQueue&);
      void operator=(MemoryQueue&);
      void grow();

   public:
      MemoryQueue();
      ~MemoryQueue();

      // Static. Returns whether the given length in bytes needs to be chunked.
      static bool chunkable(size_t len) { return len > Chunk::ChunkSize; }
      
      // Returns current size in bytes.
      size_t curSize() const { return _curSize; }
      // Deletes all chunks except the first (to avoid a preallocation), and calls reset.
      void clear();
      // Resets the size and current write position, but does not clear.
      // Existing chunks will be used, and new ones will be created (allocated) if required.
      // This saves having to clear (which deletes) before every use, at the expense
      //  of keeping what could be an ever increasing memory block alive. 
      void reset();
      // Return true if successful.
      bool add(const unsigned char* src, size_t len);
      // Copies the queue to a character buffer.
      // Returns number of bytes copied.
      size_t copy(unsigned char* dst, size_t len) const;
      };

#endif

