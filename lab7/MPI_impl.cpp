#include <mpi.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>
#include <iomanip>

using ll = long long;

// ---------------------- Config ----------------------
static constexpr int N = 4096;                  // number of coefficients
static constexpr int MAX_COEFF = 10;            // coeffs in [-MAX_COEFF, +MAX_COEFF]
static constexpr uint64_t SEED = 42ULL;
static constexpr int KARATSUBA_THRESHOLD = 64;  // base-case size (power-of-two length)
static constexpr int REPEATS = 5;               // best-of REPEATS
static constexpr bool VERIFY = true;            // compare all results to SEQ naive

// ---------------------- Small helpers ----------------------
static int nextPow2(int x) {
    int p = 1;
    while (p < x) p <<= 1;
    return p;
}

static std::vector<ll> padTo(const std::vector<ll>& a, int n) {
    std::vector<ll> r(n, 0);
    std::copy(a.begin(), a.end(), r.begin());
    return r;
}

static std::vector<ll> addVec(const std::vector<ll>& a, const std::vector<ll>& b) {
    size_t n = std::max(a.size(), b.size());
    std::vector<ll> r(n, 0);
    for (size_t i = 0; i < n; i++) {
        ll x = (i < a.size()) ? a[i] : 0;
        ll y = (i < b.size()) ? b[i] : 0;
        r[i] = x + y;
    }
    return r;
}

static std::vector<ll> subVec(const std::vector<ll>& a, const std::vector<ll>& b) {
    size_t n = std::max(a.size(), b.size());
    std::vector<ll> r(n, 0);
    for (size_t i = 0; i < n; i++) {
        ll x = (i < a.size()) ? a[i] : 0;
        ll y = (i < b.size()) ? b[i] : 0;
        r[i] = x - y;
    }
    return r;
}

static void addTo(std::vector<ll>& dest, const std::vector<ll>& src, int shift) {
    for (size_t i = 0; i < src.size(); i++) {
        size_t k = (size_t)shift + i;
        if (k < dest.size()) dest[k] += src[i];
    }
}

// ---------------------- Sequential: naive ----------------------
static std::vector<ll> naiveMul(const std::vector<ll>& a, const std::vector<ll>& b) {
    int n = (int)a.size(), m = (int)b.size();
    std::vector<ll> c(n + m - 1, 0);
    for (int i = 0; i < n; i++) {
        ll ai = a[i];
        for (int j = 0; j < m; j++) {
            c[i + j] += ai * b[j];
        }
    }
    return c;
}

// ---------------------- Sequential: Karatsuba (power-of-two) ----------------------
static std::vector<ll> karatsubaSeqPow2(const std::vector<ll>& a, const std::vector<ll>& b, int threshold) {
    int n = (int)a.size();
    if (n <= threshold) return naiveMul(a, b);

    int half = n / 2;
    std::vector<ll> a0(a.begin(), a.begin() + half);
    std::vector<ll> a1(a.begin() + half, a.end());
    std::vector<ll> b0(b.begin(), b.begin() + half);
    std::vector<ll> b1(b.begin() + half, b.end());

    auto z0 = karatsubaSeqPow2(a0, b0, threshold);
    auto z2 = karatsubaSeqPow2(a1, b1, threshold);

    auto a01 = addVec(a0, a1);
    auto b01 = addVec(b0, b1);
    auto z1 = karatsubaSeqPow2(a01, b01, threshold);

    auto mid = subVec(subVec(z1, z0), z2);

    std::vector<ll> res(2 * n - 1, 0);
    addTo(res, z0, 0);
    addTo(res, mid, half);
    addTo(res, z2, 2 * half);
    return res;
}

static std::vector<ll> karatsubaSeq(const std::vector<ll>& a, const std::vector<ll>& b, int threshold) {
    int need = (int)a.size() + (int)b.size() - 1;
    int n = nextPow2(std::max((int)a.size(), (int)b.size()));
    auto ap = padTo(a, n);
    auto bp = padTo(b, n);
    auto cp = karatsubaSeqPow2(ap, bp, threshold);
    cp.resize(need);
    return cp;
}

// ---------------------- MPI helpers: send/recv vector<ll> ----------------------
static void sendVecLL(int dest, int tagBase, const std::vector<ll>& v, MPI_Comm comm) {
    int len = (int)v.size();
    MPI_Send(&len, 1, MPI_INT, dest, tagBase, comm);
    if (len > 0) MPI_Send(v.data(), len, MPI_LONG_LONG, dest, tagBase + 1, comm);
}

static std::vector<ll> recvVecLL(int src, int tagBase, MPI_Comm comm) {
    int len = 0;
    MPI_Recv(&len, 1, MPI_INT, src, tagBase, comm, MPI_STATUS_IGNORE);
    std::vector<ll> v(len);
    if (len > 0) MPI_Recv(v.data(), len, MPI_LONG_LONG, src, tagBase + 1, comm, MPI_STATUS_IGNORE);
    return v;
}

// ---------------------- MPI naive: scatter A, reduce partial sums ----------------------
static std::vector<ll> mpiNaiveMul(const std::vector<ll>& a, const std::vector<ll>& b, MPI_Comm comm) {
    int rank, p;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &p);

    int n = (int)a.size(), m = (int)b.size();
    int lenC = n + m - 1;

    std::vector<int> counts(p, 0), displs(p, 0);
    int base = n / p, rem = n % p;
    int off = 0;
    for (int r = 0; r < p; r++) {
        counts[r] = base + (r < rem ? 1 : 0);
        displs[r] = off;
        off += counts[r];
    }

    std::vector<ll> localA(counts[rank]);
    MPI_Scatterv(a.data(), counts.data(), displs.data(), MPI_LONG_LONG,
                 localA.data(), (int)localA.size(), MPI_LONG_LONG, 0, comm);

    std::vector<ll> localC(lenC, 0);
    int startI = displs[rank];
    for (int ii = 0; ii < (int)localA.size(); ii++) {
        ll ai = localA[ii];
        int iGlobal = startI + ii;
        for (int j = 0; j < m; j++) {
            localC[iGlobal + j] += ai * b[j];
        }
    }

    std::vector<ll> globalC;
    if (rank == 0) globalC.assign(lenC, 0);

    MPI_Reduce(localC.data(),
               rank == 0 ? globalC.data() : nullptr,
               lenC, MPI_LONG_LONG, MPI_SUM, 0, comm);

    return globalC;
}

// ---------------------- MPI Karatsuba (distributed D&C) ----------------------
// returns result only on comm-rank 0; others return empty
static std::vector<ll> karatsubaMpiPow2(const std::vector<ll>& a,
                                       const std::vector<ll>& b,
                                       MPI_Comm comm,
                                       int threshold) {
    int rank, p;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &p);

    int n = (int)a.size();

    if (n <= threshold || p < 3) {
        if (rank == 0) return karatsubaSeqPow2(a, b, threshold);
        return {};
    }

    int half = n / 2;

    int p0 = p / 3;
    int p1 = p / 3;
    int p2 = p - p0 - p1;
    if (p0 == 0 || p1 == 0 || p2 == 0) {
        if (rank == 0) return karatsubaSeqPow2(a, b, threshold);
        return {};
    }

    int color = (rank < p0) ? 0 : (rank < p0 + p1 ? 1 : 2);

    MPI_Comm sub;
    MPI_Comm_split(comm, color, rank, &sub);

    int subRank;
    MPI_Comm_rank(sub, &subRank);

    if (rank == 0) {
        std::vector<ll> a0(a.begin(), a.begin() + half);
        std::vector<ll> a1(a.begin() + half, a.end());
        std::vector<ll> b0(b.begin(), b.begin() + half);
        std::vector<ll> b1(b.begin() + half, b.end());

        auto a01 = addVec(a0, a1);
        auto b01 = addVec(b0, b1);

        sendVecLL(p0,       110, a01, comm);
        sendVecLL(p0,       120, b01, comm);
        sendVecLL(p0 + p1,  210, a1,  comm);
        sendVecLL(p0 + p1,  220, b1,  comm);
    }

    std::vector<ll> subA, subB;
    if (subRank == 0) {
        if (color == 0) {
            subA.assign(a.begin(), a.begin() + half);
            subB.assign(b.begin(), b.begin() + half);
        } else if (color == 1) {
            subA = recvVecLL(0, 110, comm);
            subB = recvVecLL(0, 120, comm);
        } else {
            subA = recvVecLL(0, 210, comm);
            subB = recvVecLL(0, 220, comm);
        }
    }

    int lenA = (subRank == 0) ? (int)subA.size() : 0;
    int lenB = (subRank == 0) ? (int)subB.size() : 0;
    MPI_Bcast(&lenA, 1, MPI_INT, 0, sub);
    MPI_Bcast(&lenB, 1, MPI_INT, 0, sub);
    if (subRank != 0) { subA.assign(lenA, 0); subB.assign(lenB, 0); }
    if (lenA > 0) MPI_Bcast(subA.data(), lenA, MPI_LONG_LONG, 0, sub);
    if (lenB > 0) MPI_Bcast(subB.data(), lenB, MPI_LONG_LONG, 0, sub);

    auto subRes = karatsubaMpiPow2(subA, subB, sub, threshold);

    if (subRank == 0 && color == 1) sendVecLL(0, 310, subRes, comm);
    if (subRank == 0 && color == 2) sendVecLL(0, 410, subRes, comm);

    MPI_Comm_free(&sub);

    if (rank != 0) return {};

    auto z0 = subRes;
    auto z1 = recvVecLL(p0,      310, comm);
    auto z2 = recvVecLL(p0 + p1, 410, comm);

    auto mid = subVec(subVec(z1, z0), z2);

    std::vector<ll> res(2 * n - 1, 0);
    addTo(res, z0, 0);
    addTo(res, mid, half);
    addTo(res, z2, 2 * half);
    return res;
}

static std::vector<ll> mpiKaratsubaMul(const std::vector<ll>& a,
                                       const std::vector<ll>& b,
                                       MPI_Comm comm,
                                       int threshold) {
    int rank;
    MPI_Comm_rank(comm, &rank);

    int need = (int)a.size() + (int)b.size() - 1;
    int n = nextPow2(std::max((int)a.size(), (int)b.size()));
    auto ap = padTo(a, n);
    auto bp = padTo(b, n);

    auto cp = karatsubaMpiPow2(ap, bp, comm, threshold);
    if (rank == 0) {
        cp.resize(need);
        return cp;
    }
    return {};
}

// ---------------------- Main: run all variants ----------------------
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0, p = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    if (rank == 0) {
        std::cout << "PolyMul MPI (runs all variants)\n";
        std::cout << "n=" << N << " maxCoeff=" << MAX_COEFF
                  << " threshold=" << KARATSUBA_THRESHOLD
                  << " repeats=" << REPEATS
                  << " procs=" << p
                  << " verify=" << (VERIFY ? "on" : "off") << "\n\n";
    }

    std::vector<ll> A, B;
    if (rank == 0) {
        std::mt19937_64 rngA(SEED);
        std::mt19937_64 rngB(SEED ^ 0x9E3779B97F4A7C15ULL);
        std::uniform_int_distribution<int> dist(-MAX_COEFF, MAX_COEFF);

        A.resize(N); B.resize(N);
        for (int i = 0; i < N; i++) { A[i] = dist(rngA); B[i] = dist(rngB); }
    } else {
        A.resize(N); B.resize(N);
    }

    MPI_Bcast(A.data(), N, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(B.data(), N, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    std::vector<ll> ref;
    if (rank == 0 && VERIFY) ref = naiveMul(A, B);

    auto run = [&](const char* name, auto&& fn) {
        MPI_Barrier(MPI_COMM_WORLD);
        long long bestNs = (1LL << 62);
        std::vector<ll> last;

        for (int r = 0; r < REPEATS; r++) {
            MPI_Barrier(MPI_COMM_WORLD);
            auto t0 = std::chrono::high_resolution_clock::now();

            last = fn();

            MPI_Barrier(MPI_COMM_WORLD);
            auto t1 = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            if (rank == 0) bestNs = std::min(bestNs, (long long)ns);
        }

        if (rank == 0) {
            double ms = bestNs / 1e6;
            std::string ok = "n/a";
            if (VERIFY) ok = (last == ref) ? "OK" : "FAIL";
            std::cout << std::left << std::setw(15) << name
                      << " best_ms=" << std::right << std::setw(10) << std::fixed << std::setprecision(3) << ms
                      << "  verify=" << ok << "\n";
        }
    };

    run("SEQ_NAIVE", [&]{
        if (rank == 0) return naiveMul(A, B);
        return std::vector<ll>{};
    });

    run("SEQ_KARATSUBA", [&]{
        if (rank == 0) return karatsubaSeq(A, B, KARATSUBA_THRESHOLD);
        return std::vector<ll>{};
    });

    run("MPI_NAIVE", [&]{
        return mpiNaiveMul(A, B, MPI_COMM_WORLD);
    });

    run("MPI_KARATSUBA", [&]{
        return mpiKaratsubaMul(A, B, MPI_COMM_WORLD, KARATSUBA_THRESHOLD);
    });

    MPI_Finalize();
    return 0;
}
