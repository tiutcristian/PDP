#include "dsm.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

DSM::DSM(MPI_Comm comm, int varsPerProcess, Callback cb)
    : comm_(comm), varsPerProc_(varsPerProcess), cb_(std::move(cb)) {
  MPI_Comm_rank(comm_, &rank_);
  MPI_Comm_size(comm_, &world_);

  if (varsPerProc_ <= 0) throw std::runtime_error("varsPerProcess must be > 0");

  values_.assign(world_ * varsPerProc_, 0);
}

int DSM::globalVarId(int homeRank, int localIndex) const {
  if (homeRank < 0 || homeRank >= world_) throw std::runtime_error("invalid homeRank");
  if (localIndex < 0 || localIndex >= varsPerProc_) throw std::runtime_error("invalid localIndex");
  return homeRank * varsPerProc_ + localIndex;
}

std::vector<int> DSM::subscribersFor(int homeRank, int localIndex) const {
  (void)localIndex; // unused in this sample policy

  std::vector<int> subs;
  subs.push_back(homeRank);

  if (world_ >= 2) subs.push_back((homeRank + 1) % world_);
  if (world_ >= 3) subs.push_back((homeRank + 2) % world_);

  std::sort(subs.begin(), subs.end());
  subs.erase(std::unique(subs.begin(), subs.end()), subs.end());
  return subs;
}

bool DSM::isSubscriber(int who, int homeRank, int localIndex) const {
  auto subs = subscribersFor(homeRank, localIndex);
  return std::binary_search(subs.begin(), subs.end(), who);
}

void DSM::sendRequest(MsgType type, int home, int idx, int v1, int v2, long long reqId) {
  // Request layout: [type, home, idx, v1, v2, reqId]
  long long buf[6];
  buf[0] = static_cast<long long>(type);
  buf[1] = home;
  buf[2] = idx;
  buf[3] = v1;
  buf[4] = v2;
  buf[5] = reqId;

  MPI_Send(buf, 6, MPI_LONG_LONG, 0, TAG_REQ, comm_);
}

void DSM::sequencerMulticast(const OrderedOp& op) {
  // OrderedOp layout: [seq, type, home, idx, v1, v2, initiator, reqId]
  long long buf[8];
  buf[0] = op.seq;
  buf[1] = static_cast<long long>(op.type);
  buf[2] = op.home;
  buf[3] = op.idx;
  buf[4] = op.v1;
  buf[5] = op.v2;
  buf[6] = op.initiator;
  buf[7] = op.reqId;

  // Only send to subscribers of that variable (per lab note).
  auto subs = subscribersFor(op.home, op.idx);
  for (int dest : subs) {
    if (dest == 0) continue; // rank 0 applies locally (no self-send needed)
    MPI_Send(buf, 8, MPI_LONG_LONG, dest, TAG_OP, comm_);
  }
}

void DSM::sequencerProcessRequests() {
  int flag = 0;
  MPI_Status st;

  while (true) {
    MPI_Iprobe(MPI_ANY_SOURCE, TAG_REQ, comm_, &flag, &st);
    if (!flag) break;

    long long rbuf[6];
    MPI_Recv(rbuf, 6, MPI_LONG_LONG, st.MPI_SOURCE, TAG_REQ, comm_, &st);

    auto type = static_cast<MsgType>(rbuf[0]);
    int home  = static_cast<int>(rbuf[1]);
    int idx   = static_cast<int>(rbuf[2]);
    int v1    = static_cast<int>(rbuf[3]);
    int v2    = static_cast<int>(rbuf[4]);
    long long reqId = rbuf[5];

    // Enforce “only subscribers may change the variable”
    if (!isSubscriber(st.MPI_SOURCE, home, idx)) {
      // In a real project you might send an error back.
      // For the lab, fail fast:
      throw std::runtime_error("Non-subscriber attempted to modify a variable");
    }

    OrderedOp op;
    op.seq       = nextSeq_++;
    op.type      = type;
    op.home      = home;
    op.idx       = idx;
    op.v1        = v1;
    op.v2        = v2;
    op.initiator = st.MPI_SOURCE;
    op.reqId     = reqId;

    // Apply locally on rank 0 (if subscribed); then multicast to other subscribers.
    applyOrderedOp(op);
    sequencerMulticast(op);
  }
}

void DSM::applyOrderedOp(const OrderedOp& op) {
  if (op.type == MsgType::SHUTDOWN) {
    running_ = false;
    // also mark completion for initiator if needed
  }

  // Only subscribers update state and can trigger callbacks.
  if (!isSubscriber(rank_, op.home, op.idx)) {
    // Still, initiator might be this rank; but initiator must be a subscriber by rule.
    return;
  }

  int gid = globalVarId(op.home, op.idx);
  int& cur = values_[gid];

  bool changed = false;
  bool casSuccess = false;

  switch (op.type) {
    case MsgType::WRITE:
      changed = (cur != op.v1);
      cur = op.v1;
      if (op.initiator == rank_) writeDone_[op.reqId] = true;
      break;

    case MsgType::CAS:
      casSuccess = (cur == op.v1);
      if (casSuccess) {
        changed = (cur != op.v2);
        cur = op.v2;
      }
      if (op.initiator == rank_) {
        casDone_[op.reqId] = true;
        casResult_[op.reqId] = casSuccess;
      }
      break;

    case MsgType::SHUTDOWN:
      // nothing else
      break;
  }

  if (changed && cb_) {
    cb_(op.home, op.idx, cur, op.seq);
  }
}

void DSM::poll() {
  if (!running_) return;

  // rank 0 acts as sequencer
  if (rank_ == 0) {
    sequencerProcessRequests();
    return; // rank 0 has nothing to receive via TAG_OP (it applies directly)
  }

  // other ranks: receive any pending ordered ops (from rank 0)
  int flag = 0;
  MPI_Status st;

  while (true) {
    MPI_Iprobe(0, TAG_OP, comm_, &flag, &st);
    if (!flag) break;

    long long buf[8];
    MPI_Recv(buf, 8, MPI_LONG_LONG, 0, TAG_OP, comm_, &st);

    OrderedOp op;
    op.seq       = buf[0];
    op.type      = static_cast<MsgType>(buf[1]);
    op.home      = static_cast<int>(buf[2]);
    op.idx       = static_cast<int>(buf[3]);
    op.v1        = static_cast<int>(buf[4]);
    op.v2        = static_cast<int>(buf[5]);
    op.initiator = static_cast<int>(buf[6]);
    op.reqId     = buf[7];

    applyOrderedOp(op);

    if (op.type == MsgType::SHUTDOWN) {
      running_ = false;
      break;
    }
  }
}

void DSM::waitForWrite(long long reqId) {
  while (running_) {
    poll();
    auto it = writeDone_.find(reqId);
    if (it != writeDone_.end() && it->second) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("DSM stopped while waiting for write");
}

bool DSM::waitForCas(long long reqId) {
  while (running_) {
    poll();
    auto it = casDone_.find(reqId);
    if (it != casDone_.end() && it->second) {
      return casResult_[reqId];
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  throw std::runtime_error("DSM stopped while waiting for CAS");
}

void DSM::write(int homeRank, int localIndex, int value) {
  if (!isSubscriber(rank_, homeRank, localIndex)) {
    throw std::runtime_error("write() not allowed: caller is not subscribed to variable");
  }

  long long reqId = nextReqId_++;

  if (rank_ == 0) {
    // rank 0 can order directly without sending a request to itself
    OrderedOp op;
    op.seq       = nextSeq_++;
    op.type      = MsgType::WRITE;
    op.home      = homeRank;
    op.idx       = localIndex;
    op.v1        = value;
    op.v2        = 0;
    op.initiator = 0;
    op.reqId     = reqId;

    applyOrderedOp(op);
    sequencerMulticast(op);

    writeDone_[reqId] = true;
    return;
  }

  sendRequest(MsgType::WRITE, homeRank, localIndex, value, 0, reqId);
  waitForWrite(reqId);
}

bool DSM::compareExchange(int homeRank, int localIndex, int expected, int desired) {
  if (!isSubscriber(rank_, homeRank, localIndex)) {
    throw std::runtime_error("compareExchange() not allowed: caller is not subscribed to variable");
  }

  long long reqId = nextReqId_++;

  if (rank_ == 0) {
    OrderedOp op;
    op.seq       = nextSeq_++;
    op.type      = MsgType::CAS;
    op.home      = homeRank;
    op.idx       = localIndex;
    op.v1        = expected;
    op.v2        = desired;
    op.initiator = 0;
    op.reqId     = reqId;

    applyOrderedOp(op);
    sequencerMulticast(op);

    casDone_[reqId] = true;
    return casResult_[reqId];
  }

  sendRequest(MsgType::CAS, homeRank, localIndex, expected, desired, reqId);
  return waitForCas(reqId);
}

int DSM::read(int homeRank, int localIndex) const {
  int gid = globalVarId(homeRank, localIndex);
  return values_[gid];
}

void DSM::shutdown() {
  if (rank_ != 0) return;

  OrderedOp op;
  op.seq       = nextSeq_++;
  op.type      = MsgType::SHUTDOWN;
  op.home      = 0;
  op.idx       = 0;
  op.v1        = 0;
  op.v2        = 0;
  op.initiator = 0;
  op.reqId     = 0;

  // broadcast shutdown to everyone (no subscription filtering for shutdown)
  long long buf[8];
  buf[0] = op.seq;
  buf[1] = static_cast<long long>(op.type);
  buf[2] = op.home;
  buf[3] = op.idx;
  buf[4] = op.v1;
  buf[5] = op.v2;
  buf[6] = op.initiator;
  buf[7] = op.reqId;

  for (int dest = 1; dest < world_; ++dest) {
    MPI_Send(buf, 8, MPI_LONG_LONG, dest, TAG_OP, comm_);
  }

  running_ = false;
}
