#include "common.hpp"

#define SIZE_MASK (RING_SIZE - 1)


bool
FreeInsertToMessageBuffer(
       RingBuffer* Ring,
       const BufferT CopyFrom,
       MessageSizeT MessageSize
) {
       int forwardTail = Ring->ForwardTail[0];
       int head = Ring->Head[0];
       RingSizeT distance = 0;

       if (forwardTail < head) {
              distance = forwardTail + RING_SIZE - head;
       }
       else {
              distance = forwardTail - head;
       }

       if (distance >= FORWARD_DEGREE) {
              return false;
       }

       MessageSizeT messageBytes = sizeof(MessageSizeT) + MessageSize;
       while (messageBytes % CACHE_LINE != 0) {
              messageBytes++;
       }
 
       if (messageBytes > RING_SIZE - distance) {
              return false;
       }
 
       while (Ring->ForwardTail[0].compare_exchange_weak(forwardTail, (forwardTail + messageBytes) & SIZE_MASK) == false) {
              forwardTail = Ring->ForwardTail[0];
              head = Ring->Head[0];

              forwardTail = Ring->ForwardTail[0];
              head = Ring->Head[0];

              if (forwardTail <= head) {
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
       }
 
       if (forwardTail + messageBytes <= RING_SIZE) {
              char* messageAddress = &Ring->Buffer[forwardTail];
 
              *((MessageSizeT*)messageAddress) = messageBytes;
 
              memcpy(messageAddress + sizeof(MessageSizeT), CopyFrom, MessageSize);
 
              int safeTail = Ring->SafeTail[0];
              while (Ring->SafeTail[0].compare_exchange_weak(safeTail, (safeTail + messageBytes) & SIZE_MASK) == false) {
                     safeTail = Ring->SafeTail[0];
              }
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
 
              int safeTail = Ring->SafeTail[0];
              while (Ring->SafeTail[0].compare_exchange_weak(safeTail, (safeTail + messageBytes) & SIZE_MASK) == false) {
                     safeTail = Ring->SafeTail[0];
              }
       }
 
       return true;
}
 