# Single Consumer Multiple Producers Ring Buffer

Modified from [this implementation](https://fardatalab.org/files/RingBuffer.h) of a single producer single consumer ring buffer.

## Directory Structure

```sh
.
├── data                # Results
├── include
│   ├── common.hpp      # Common functions
│   ├── lock.hpp        # Simple locking
│   ├── notify.hpp      # Wait-for-notification
│   ├── optimized.hpp   # Optimized implementation
│   ├── single.hpp      # Single producer (original)
│   ├── spin.hpp        # Busy waiting for prior commits
│   ├── tail.hpp        # Change tail pointer to non-atomic
│   ├── yield.hpp       # Yielding in spin lock
│   └── free.hpp        # Lock-free producer (same as `single` but with `&` wrapping)
└── src
    ├── main.cpp        # Driver application
    └── single.cpp      # Driver for single producer
```

To check differences between implementations, run `diff` directly. For example:

```bash
diff include/spin.hpp include/notify.hpp
```

## Run experiments

```bash
make all
```

> [!NOTE]  
> Change `TOTAL_CORES` in `include/common.hpp` to the number of cores on your machine (default: 32, it is the numer of logical cores).

> [!CAUTION] 
> Use the `-DARM` flag to compile for ARM architecture.
