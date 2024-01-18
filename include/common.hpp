#pragma once

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <future>
#include <numeric>
#include <string.h>

#define TOTAL_CORES 32
#define MESSAGE_SIZE 8
#define PAYLOAD_SIZE CACHE_LINE - sizeof(MessageSizeT)
#define NUM_PRODUCERS 2
#define NUM_MESSAGES 10000000
#define TOTAL_MESSAGES NUM_PRODUCERS * NUM_MESSAGES
#define WARMUP_MESSAGES TOTAL_MESSAGES * 0.05
#define REPEATS 3


#ifdef MEM_RELAXED
       #define mem_barrier std::memory_order_relaxed // for optimized
#else
       #define mem_barrier std::memory_order_seq_cst // default
#endif

std::mutex mtx;
std::condition_variable cond;

double gThroughput;
int gNumProducers = -1;
//* 8-byte message
char const *MESSAGE = "ABCDEFG";

void writeCSV(const std::string& filename, const std::vector<std::vector<std::string>>& data) {
    std::ofstream outputFile(filename);

    if (!outputFile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    for (const auto& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            outputFile << row[i];

            if (i < row.size() - 1) {
                outputFile << ",";
            }
        }

        outputFile << "\n";
    }

    outputFile.close();
}


//
// Copyright (c) Far Data Lab (FDL).
// All rights reserved.
//
//

#pragma once

#include <atomic>

#define RING_SIZE           16777216
#define FORWARD_DEGREE      1048576
#define CACHE_LINE          64
#define INT_ALIGNED         16
 
template <class C>
using Atomic = std::atomic<C>;
typedef char*        BufferT;
typedef unsigned int MessageSizeT;
typedef unsigned int RingSizeT;
 
struct RingBuffer {
       Atomic<int> ForwardTail[INT_ALIGNED];
       Atomic<int> SafeTail[INT_ALIGNED];
       int Tail;
       int Head[INT_ALIGNED];
       char Buffer[RING_SIZE];
};

RingBuffer*
AllocateMessageBuffer(
       BufferT BufferAddress
) {
       RingBuffer* ringBuffer = (RingBuffer*)BufferAddress;
 
       size_t ringBufferAddress = (size_t)ringBuffer;
       while (ringBufferAddress % CACHE_LINE != 0) {
              ringBufferAddress++;
       }
       ringBuffer = (RingBuffer*)ringBufferAddress;
 
       memset(ringBuffer, 0, sizeof(RingBuffer));
 
       return ringBuffer;
}

void
DeallocateMessageBuffer(
       RingBuffer* Ring
) {
       memset(Ring, 0, sizeof(RingBuffer));
}

bool
FetchFromMessageBuffer(
       RingBuffer* Ring,
       BufferT CopyTo,
       MessageSizeT* MessageSize
) {
       int safeTail = (Ring->Tail < 0)? Ring->SafeTail[0].load(mem_barrier) : Ring->Tail;
       int forwardTail = Ring->ForwardTail[0].load(mem_barrier);
       int head = Ring->Head[0];
 
       if (forwardTail == head) {
              return false;
       }
 
       if (forwardTail != safeTail) {
              return false;
       }
 
       RingSizeT availBytes = 0;
       char* sourceBuffer1 = nullptr;
       char* sourceBuffer2 = nullptr;
 
       if (safeTail > head) {
              availBytes = safeTail - head;
              *MessageSize = availBytes;
              sourceBuffer1 = &Ring->Buffer[head];
       }
       else {
              availBytes = RING_SIZE - head;
              *MessageSize = availBytes + safeTail;
              sourceBuffer1 = &Ring->Buffer[head];
              sourceBuffer2 = &Ring->Buffer[0];
       }
 
       memcpy(CopyTo, sourceBuffer1, availBytes);
       memset(sourceBuffer1, 0, availBytes);
 
       if (sourceBuffer2) {
              memcpy((char*)CopyTo + availBytes, sourceBuffer2, safeTail);
              memset(sourceBuffer2, 0, safeTail);
       }
 
       Ring->Head[0] = safeTail;
 
       return true;
}
 
void
ParseNextMessage(
       BufferT CopyTo,
       MessageSizeT TotalSize,
       BufferT* MessagePointer,
       MessageSizeT* MessageSize,
       BufferT* StartOfNext,
       MessageSizeT* RemainingSize
) {
       char* bufferAddress = (char*)CopyTo;
       MessageSizeT totalBytes = *(MessageSizeT*)bufferAddress;
 
       *MessagePointer = (BufferT)(bufferAddress + sizeof(MessageSizeT));
       *MessageSize = totalBytes - sizeof(MessageSizeT);
       *RemainingSize = TotalSize - totalBytes;
 
       if (*RemainingSize > 0) {
              *StartOfNext = (BufferT)(bufferAddress + totalBytes);
       }
       else {
              *StartOfNext = nullptr;
       }
}
