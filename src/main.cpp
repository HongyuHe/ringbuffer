#include "common.hpp"
#include "lock.hpp"
#include "spin.hpp"
#include "notify.hpp"
#include "tail.hpp"
#include "yield.hpp"


using InsertFunctionT = bool (*)(RingBuffer*, const BufferT, MessageSizeT);

void producer(InsertFunctionT insertFunc, RingBuffer *ringBuffer, uint id) 
{
    for (size_t i = 0; i < NUM_MESSAGES; i++) {
        while(!insertFunc(ringBuffer, (BufferT)MESSAGE, sizeof(MESSAGE)))
        ;
    }
}

void consumer(RingBuffer *ringBuffer, uint numProducers, bool verify) 
{
    char *payloadBuf = new char[RING_SIZE];
    memset(payloadBuf, 0, RING_SIZE);
    MessageSizeT fetchedBytes;
    size_t receivedCount = 0;
    size_t measuredCount = 0;
    bool warmedUp = false;

    std::chrono::high_resolution_clock::time_point startTime;
    while (receivedCount < NUM_MESSAGES * numProducers) {
        if (!FetchFromMessageBuffer(ringBuffer, (BufferT)payloadBuf, &fetchedBytes)) {
            continue;
        }

        MessageSizeT messageSize = 0;
        MessageSizeT remainingSize = fetchedBytes;
        char *messagePtr = payloadBuf;
        char *startOfNext = payloadBuf;
        do {
            //* Parse the message and determine the next message start and remaining size
            ParseNextMessage(payloadBuf, fetchedBytes, &messagePtr, &messageSize, &startOfNext, &remainingSize);

            //* Verify the correctness of the message.
            if (verify) {
                try {
                    if (messageSize != PAYLOAD_SIZE || memcmp(messagePtr, MESSAGE, MESSAGE_SIZE)) {
                        std::cout << "Corrupted message!" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                } catch (const std::exception& e) {
                    std::cout << "Exception: " << e.what() << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            messagePtr = startOfNext;
            fetchedBytes = remainingSize;
            receivedCount++;
            measuredCount++;
        } while (remainingSize > 0);

        //* Start measuring throughput after warmup.
        if (!warmedUp && receivedCount >= WARMUP_MESSAGES) {
            startTime = std::chrono::high_resolution_clock::now();
            measuredCount = 0;
            warmedUp = true;
        }
    }

    //* Calculate throughput 
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    std::cout << "\tDuration:\t" << duration.count() << " ms" << std::endl;
    gThroughput = (double)(measuredCount) / (duration.count() / 1000.0);
}

int main(int argc, char *argv[]) {
    bool verify = false;
    std::string mode = "lock";
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <check> [<mode>]" << std::endl;
        exit(1);
    } else {
        verify = atoi(argv[1]);
        std::cout << "Check:\t" << verify << std::endl;
        mode = argv[2]? argv[2] : mode;
        std::cout << "Mode:\t" << mode << std::endl;
    }
    std::cout << "Memory barrier:\t" << (mem_barrier == std::memory_order_relaxed? "relaxed" : "seq const") << std::endl;

    InsertFunctionT insertFunc;
    switch (mode[0]) {
        case 'l':
            insertFunc = &LockInsertToMessageBuffer;
            break;
        case 's':
            insertFunc = &SpinInsertToMessageBuffer;
            break;
        case 'n':
            insertFunc = &NotifyInsertToMessageBuffer;
            break;
        case 't':
            insertFunc = &TailInsertToMessageBuffer;
            break;
        case 'y':
            insertFunc = &YieldInsertToMessageBuffer;
            break;
        default:
            std::cerr << "Invalid mode: " << mode << std::endl;
            exit(1);
    }

    std::vector<std::vector<std::string>> data;
    std::string filename = "data/" + mode + ".csv";
    std::vector<std::string> header = {"mode", "num_producers", "throughput_mps"};
    data.push_back(header);

    for (int numProducers = 1; numProducers <= TOTAL_CORES; numProducers *= 2) {
        std::cout << "Number of producers:\t" << numProducers << std::endl;
        std::vector <std::thread> threads;
        std::vector<double> throughputs;
        //* Repeat
        for (int i = 0; i < REPEATS; i++) {
            std::cout << "\tRepeat:\t" << i+1 << std::endl;
            gThroughput = 0;
            threads.clear();
            throughputs.clear();
            //* Allocate the ring buffer.
            BufferT buffer = new char[sizeof(RingBuffer) + CACHE_LINE];
            RingBuffer* ringBuffer = AllocateMessageBuffer(buffer);
            if (mode != "tail") ringBuffer->Tail = -1;
            
            for (int id = 0; id < numProducers; id++) {
                threads.push_back(std::thread(producer, insertFunc, ringBuffer, id));
            }
            threads.push_back(std::thread(consumer, ringBuffer, numProducers, verify));

            for (auto &thread : threads) {
                thread.join();
            }
            
            //* Deallocate the ring buffer
            DeallocateMessageBuffer(ringBuffer);
            delete[] buffer;

            throughputs.push_back(gThroughput);
            data.push_back({mode, std::to_string(numProducers), std::to_string(gThroughput)});
            //* Checkpointing to prevent server down time.
            writeCSV(filename, data);
        }
        //* Calculate average throughput.
        double sum = std::accumulate(throughputs.begin(), throughputs.end(), 0.0);
        double avg = sum / throughputs.size();
        std::cout << "Throughput:\t" << avg << " MPS" << std::endl;
    }

    return EXIT_SUCCESS;
}
