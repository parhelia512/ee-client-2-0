//-----------------------------------------------------------------------------
// Element Engine
// 
//-----------------------------------------------------------------------------

#include "LockFreeChunker.h"
#include <assert.h>
//----------------------------------------------------------------------------
namespace WL
{

DataChunker::DataChunker(S32 size)
{
   chunkSize          = size;
   curBlock           = new DataBlock(size);
   curBlock->next     = 0;
   curBlock->curIndex = 0;
}

DataChunker::~DataChunker()
{
   freeBlocks();
}

void *DataChunker::alloc(S32 size)
{
   assert(size <= chunkSize);
   if(!curBlock || size + curBlock->curIndex > chunkSize)
   {
      DataBlock *temp = new DataBlock(chunkSize);
      temp->next = curBlock;
      temp->curIndex = 0;
      curBlock = temp;
   }
   void *ret = curBlock->data + curBlock->curIndex;
   curBlock->curIndex += (size + 3) & ~3; // dword align
   return ret;
}

DataChunker::DataBlock::DataBlock(S32 size)
{
   data = new U8[size];
}

DataChunker::DataBlock::~DataBlock()
{
   delete[] data;
}

void DataChunker::freeBlocks()
{
   while(curBlock)
   {
      DataBlock *temp = curBlock->next;
      delete curBlock;
      curBlock = temp;
   }
}

}