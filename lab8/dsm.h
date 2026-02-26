#pragma once
#include <mpi.h>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

class DSM {
public:
  // callback(homeRank, localIndex, newValue, globalSeqNo)
  using Callback = std::function<void(int, int, int, long long)>;

  DSM(MPI_Comm comm, int varsPerProcess, Callback cb);

  // Progress incoming messages + (on rank 0) serve pending requests.
  // Call this often from your main loop.
  void poll();

  // Blocking write: returns after the operation is applied locally.
  void write(int homeRank, int localIndex, int value);

  // Blocking CAS: returns after the operation is applied locally; result is the CAS success.
  bool compareExchange(int homeRank, int localIndex, int expected, int desired);

  // Last locally-known value (meaningful if you subscribe to the variable).
  int read(int homeRank, int localIndex) const;

  // Stop the DSM (rank 0 broadcasts shutdown to everyone).
  void shutdown();

  bool running() const { return running_; }

private:
  enum class MsgType : long long { WRITE = 1, CAS = 2, SHUTDOWN = 3 };
  static constexpr int TAG_REQ = 100;
  static constexpr int TAG_OP  = 101;

  struct OrderedOp {
    long long seq;
    MsgType type;
    int home;
    int idx;
    int v1;        // WRITE: newValue, CAS: expected
    int v2;        // CAS: desired, otherwise unused
    int initiator; // rank that requested it
    long long reqId;
  };

  MPI_Comm comm_;
  int rank_{0}, world_{1};
  int varsPerProc_{0};

  Callback cb_;

  bool running_{true};

  // local replicated state (only meaningful for vars you subscribe to)
  std::vector<int> values_;

  // per-process request id (to match “my request completed”)
  long long nextReqId_{1};

  // completion tracking for blocking ops
  std::unordered_map<long long, bool> writeDone_;
  std::unordered_map<long long, bool> casDone_;
  std::unordered_map<long long, bool> casResult_;

  // sequencer state (rank 0)
  long long nextSeq_{1};

private:
  int globalVarId(int homeRank, int localIndex) const;

  // --- Subscription policy ---
  // IMPORTANT: Make this match your lab's static subscriptions.
  // Everyone can compute the same function deterministically.
  std::vector<int> subscribersFor(int homeRank, int localIndex) const;
  bool isSubscriber(int who, int homeRank, int localIndex) const;

  // --- Networking helpers ---
  void sendRequest(MsgType type, int home, int idx, int v1, int v2, long long reqId);
  void sequencerProcessRequests();              // rank 0
  void sequencerMulticast(const OrderedOp& op); // rank 0
  void applyOrderedOp(const OrderedOp& op);     // all ranks (rank 0 applies directly)

  // blocking wait until reqId is completed (polling internally)
  void waitForWrite(long long reqId);
  bool waitForCas(long long reqId);
};
