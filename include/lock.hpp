#include "common.hpp"


bool
LockInsertToMessageBuffer(
       RingBuffer* Ring,
       const BufferT CopyFrom,
       MessageSizeT MessageSize
) {
       //* Lock everything.
       std::lock_guard<std::mutex> lock(mtx);

       MessageSizeT messageBytes = sizeof(MessageSizeT) + MessageSize;
       while (messageBytes % CACHE_LINE != 0) {
              messageBytes++;
       }

       int forwardTail;
       int head;
       RingSizeT distance = 0;

       //* Simplified to do-while loop.
       do {
              forwardTail = Ring->ForwardTail[0].load(mem_barrier);
              head = Ring->Head[0];
              
              if (forwardTail < head) {
                     distance = forwardTail + RING_SIZE - head;
              }
              else {
                     distance = forwardTail - head;
              }

              if (distance >= FORWARD_DEGREE) {
                     return false;
              }

              if (messageBytes > RING_SIZE - distance) {
                     return false;
              }
       } while (Ring->ForwardTail[0].compare_exchange_weak(
              forwardTail, (forwardTail + messageBytes) % RING_SIZE, mem_barrier, mem_barrier) == false);
       
       if (forwardTail + messageBytes <= RING_SIZE) {
              char* messageAddress = &Ring->Buffer[forwardTail];
 
              *((MessageSizeT*)messageAddress) = messageBytes;
 
              memcpy(messageAddress + sizeof(MessageSizeT), CopyFrom, MessageSize);
       }
       else {
              RingSizeT remainingBytes = RING_SIZE - forwardTail - sizeof(MessageSizeT);
              char* messageAddress1 = &Ring->Buffer[forwardTail];
              *((MessageSizeT*)messageAddress1) = messageBytes;

              if (MessageSize <= remainingBytes) {
                     memcpy(messageAddress1 + sizeof(MessageSizeT), CopyFrom, MessageSize);
              } else {
                     char* messageAddress2 = &Ring->Buffer[0];
                     if (remainingBytes) {
                            memcpy(messageAddress1 + sizeof(MessageSizeT), CopyFrom, remainingBytes);
                     }
                     memcpy(messageAddress2, (const char*)CopyFrom + remainingBytes, MessageSize - remainingBytes);
              }
       }

       //* Extracted the following part out.
       int safeTail = Ring->SafeTail[0];
       while (Ring->SafeTail[0].compare_exchange_weak(
              safeTail, (safeTail + messageBytes) % RING_SIZE, mem_barrier, mem_barrier) == false) 
       {
              safeTail = Ring->SafeTail[0].load(mem_barrier);
       }
       
       return true;
}
 