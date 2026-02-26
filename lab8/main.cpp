#include "dsm.h"
#include <mpi.h>

#include <chrono>
#include <cstdio>
#include <thread>

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  int rank = 0, world = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world);

  constexpr int VARS_PER_PROCESS = 2;

  DSM dsm(MPI_COMM_WORLD, VARS_PER_PROCESS,
          [rank](int home, int idx, int newValue, long long seq) {
            std::printf("[rank %d] CALLBACK seq=%lld var(%d,%d)=%d\n",
                        rank, seq, home, idx, newValue);
            std::fflush(stdout);
          });

  MPI_Barrier(MPI_COMM_WORLD);

  // Variable (0,0) is subscribed by {0,1,2}
  // Variable (1,0) is subscribed by {1,2,3}

  if (rank == 1) {
    dsm.write(0, 0, 10);
  }
  if (rank == 2) {
    // Try CAS after a short delay - race with other ops across processes
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    bool ok = dsm.compareExchange(0, 0, 10, 99);
    std::printf("[rank %d] CAS(0,0,10->99) result=%s\n", rank, ok ? "true" : "false");
    std::fflush(stdout);
  }
  if (rank == 0) {
    // Another write, potentially concurrent with rank2 CAS
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    dsm.write(0, 0, 11);
  }

  // Pump the DSM for a bit so everyone receives all ops.
  auto start = std::chrono::steady_clock::now();
  while (dsm.running()) {
    dsm.poll();
    if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(200))
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  MPI_Barrier(MPI_COMM_WORLD);

  if (rank == 0) {
    dsm.shutdown();
  } else {
    while (dsm.running()) {
      dsm.poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  MPI_Finalize();
  return 0;
}
