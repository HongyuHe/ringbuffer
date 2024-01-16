#include "common.hpp"


#define mem_relaxed std::memory_order_relaxed
#define SIZE_MASK (RING_SIZE - 1)


bool
TailInsertToMessageBuffer(
       RingBuffer* Ring,
       const BufferT CopyFrom,
       MessageSizeT MessageSize
) {
       MessageSizeT messageBytes = sizeof(MessageSizeT) + MessageSize;
       while (messageBytes % CACHE_LINE != 0) {
              messageBytes++;
       }

       int forwardTail;
       int head;
       RingSizeT distance = 0;

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

       while (Ring->Tail != forwardTail) {}
       
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

#ifdef ARM
       std::atomic_thread_fence(std::memory_order_release);
#endif
       Ring->Tail = (forwardTail + messageBytes) & SIZE_MASK;

       return true;
}
 