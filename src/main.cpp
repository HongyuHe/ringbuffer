#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

#include "RingBuffer.hpp"

#define MESSAGE_SIZE 8
#define NUM_PRODUCERS 2
#define TOTAL_MESSAGES NUM_PRODUCERS * 10000000


// 8-byte message
char const *message = "ABCDEFG";

// Producer function
void producer(RingBuffer* ringBuffer, uint id) {
    size_t i = 0;
    for (i = 0; i < TOTAL_MESSAGES/NUM_PRODUCERS; ++i) {
        while(!InsertToMessageBuffer(ringBuffer, (BufferT)message, sizeof(message))) {}
        // std::cout << "Produced size [" << i << "]: " << sizeof(message) << std::endl;
        // std::cout << "Produced message [" << i << "]: " << message << std::endl;
    }
    // std::cout << "(Producer " << id << ") Total messages produced: " << i << std::endl;
}

// Consumer function
void consumer(RingBuffer* ringBuffer, bool verify) {
    char *payloadBuf = new char[RING_SIZE];
    // memset(payloadBuf, 0, sizeof(payloadBuf));
    MessageSizeT payloadSize;
    size_t receivedMsg = 0;

    //TODO: Start after warm up
    auto startTime = std::chrono::high_resolution_clock::now();
    // Sleep for 1 second to allow producer to produce messages
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    while (receivedMsg < TOTAL_MESSAGES) {

        if (FetchFromMessageBuffer(ringBuffer, (BufferT)payloadBuf, &payloadSize)) {
            // std::cout << "Consume payload bytes:\t " << payloadSize << std::endl;
        } else {
            // std::cout << "No messages to consume." << std::endl;
            continue;
        }

        MessageSizeT messageSize = 0;
        MessageSizeT remainingSize = payloadSize;
        char *messagePtr = payloadBuf;
        char *startOfNext = payloadBuf;
        do {
            // Parse the message and determine the next message start and remaining size
            ParseNextMessage(payloadBuf, payloadSize, &messagePtr, 
                &messageSize, &startOfNext, &remainingSize);
            
            // std::cout << "Message content [" << receivedMsg << "]:\t " << std::string(messagePtr, messageSize) << std::endl;
            // std::cout << "Message size [" << receivedMsg << "]:\t " << messageSize << std::endl;
            // std::cout << "Remaining bytes [" << receivedMsg << "]:\t " << remainingSize << std::endl;

            //* Compare the message with the original message
            if (verify && (messageSize != 60 || memcmp(messagePtr, message, MESSAGE_SIZE))) {
                std::cout << "Message content [" << receivedMsg << "]:\t " << std::string(messagePtr, messageSize) << std::endl;
                std::cout << "Message size [" << receivedMsg << "]:\t " << messageSize << std::endl;
                std::cout << "Remaining bytes [" << receivedMsg << "]:\t " << remainingSize << std::endl;
                std::cout << "Message corrupted." << std::endl;
                exit(1);
            }

            messagePtr = startOfNext;
            payloadSize = remainingSize;
            receivedMsg++;
        } while (remainingSize > 0);
        // std::cout << "Total messages consumed: " << receivedMsg << "/" << TOTAL_MESSAGES << std::endl;
    }

    // Calculate throughput 
    //TODO: Warm up
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    std::cout << "Duration: " << duration.count() << " milliseconds" << std::endl;
    double throughput = (double)TOTAL_MESSAGES / (duration.count() / 1000.0);

    std::cout << "Throughput: " << throughput << " messages per second" << std::endl;
}

int main(int argc, char *argv[]) {
    //* Get command line argument for verifying messages or not
    bool verify = false;
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <verify>" << std::endl;
        exit(1);
    } else {
        verify = atoi(argv[1]);
        std::cout << "Verify: " << verify << std::endl;
    }

    // Allocate the ring buffer
    BufferT buffer = new char[sizeof(RingBuffer) + CACHE_LINE];
    // std::cout << "Size of ring buffer in bytes: " << sizeof(RingBuffer) << std::endl;
    RingBuffer* ringBuffer = AllocateMessageBuffer(buffer);


    std::vector <std::thread> producerThreads;
    for (int id = 0; id < NUM_PRODUCERS; ++id) {
        producerThreads.push_back(std::thread(producer, ringBuffer, id+1));
    }

    // Create producer and consumer threads
    std::thread consumerThread(consumer, ringBuffer, verify);

    // Join threads
    for (auto &thread : producerThreads) {
        thread.join();
    }
    consumerThread.join();

    // Deallocate the ring buffer
    DeallocateMessageBuffer(ringBuffer);
    delete[] buffer;

    return 0;
}
