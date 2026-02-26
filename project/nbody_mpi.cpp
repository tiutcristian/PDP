#include <mpi.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>

struct Params {
    int N = 1024;
    int steps = 1500;
    double dt = 0.01;
    double G = 1.0;
    double eps = 1e-3;
    uint64_t seed = 42;
};

static void compute_counts_displs(int N, int P, std::vector<int>& counts, std::vector<int>& displs) {
    counts.assign(P, 0);
    displs.assign(P, 0);
    int base = N / P;
    int rem  = N % P;
    for (int r = 0; r < P; ++r) counts[r] = base + (r < rem ? 1 : 0);
    displs[0] = 0;
    for (int r = 1; r < P; ++r) displs[r] = displs[r - 1] + counts[r - 1];
}

static double frand01() {
    // [0,1]
    return (double)std::rand() / (double)RAND_MAX;
}

static void init_bodies(int N,
                        std::vector<double>& pos3,
                        std::vector<double>& vel3,
                        std::vector<double>& mass,
                        uint64_t seed) {
    pos3.resize((size_t)N * 3);
    vel3.resize((size_t)N * 3);
    mass.resize((size_t)N);

    std::srand((unsigned)seed);

    const int POZITION_RANGE = 400;
    const int VELOCITY_RANGE = 20;
    const int MASS_RANGE = 200;

    for (int i = 0; i < N; ++i) {
        mass[i] = frand01() * MASS_RANGE + 10.0;

        pos3[(size_t)i*3 + 0] = frand01() * POZITION_RANGE - (POZITION_RANGE / 2.0);
        pos3[(size_t)i*3 + 1] = frand01() * POZITION_RANGE - (POZITION_RANGE / 2.0);
        pos3[(size_t)i*3 + 2] = frand01() * POZITION_RANGE - (POZITION_RANGE / 2.0);

        vel3[(size_t)i*3 + 0] = frand01() * VELOCITY_RANGE - (VELOCITY_RANGE / 2.0);
        vel3[(size_t)i*3 + 1] = frand01() * VELOCITY_RANGE - (VELOCITY_RANGE / 2.0);
        vel3[(size_t)i*3 + 2] = frand01() * VELOCITY_RANGE - (VELOCITY_RANGE / 2.0);
    }
}

static void compute_acc_local(
    const Params& par,
    int N,
    const std::vector<double>& g_pos3,
    const std::vector<double>& g_mass,
    int i0, int localN,
    std::vector<double>& acc3_out
) {
    acc3_out.assign((size_t)localN * 3, 0.0);
    const double eps2 = par.eps * par.eps;

    for (int il = 0; il < localN; ++il) {
        int i = i0 + il;
        double xi = g_pos3[(size_t)i*3 + 0];
        double yi = g_pos3[(size_t)i*3 + 1];
        double zi = g_pos3[(size_t)i*3 + 2];

        double ax = 0.0, ay = 0.0, az = 0.0;

        for (int j = 0; j < N; ++j) {
            if (j == i) continue;

            double dx = g_pos3[(size_t)j*3 + 0] - xi;
            double dy = g_pos3[(size_t)j*3 + 1] - yi;
            double dz = g_pos3[(size_t)j*3 + 2] - zi;

            double r2 = dx*dx + dy*dy + dz*dz + eps2;
            double invR = 1.0 / std::sqrt(r2);
            double invR3 = invR * invR * invR;

            double s = par.G * g_mass[j] * invR3;
            ax += s * dx;
            ay += s * dy;
            az += s * dz;
        }

        acc3_out[(size_t)il*3 + 0] = ax;
        acc3_out[(size_t)il*3 + 1] = ay;
        acc3_out[(size_t)il*3 + 2] = az;
    }
}


int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0, P = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    Params par;
    if (argc >= 2) par.N = std::stoi(argv[1]);
    if (argc >= 3) par.steps = std::stoi(argv[2]);
    if (argc >= 4) par.dt = std::stod(argv[3]);

    if (par.N <= 0 || par.steps < 0 || par.dt <= 0.0) {
        if (rank == 0) std::cerr << "Usage: ./nbody_mpi N steps dt\n";
        MPI_Finalize();
        return 1;
    }

    std::vector<int> counts, displs;
    compute_counts_displs(par.N, P, counts, displs);

    const int localN = counts[rank];
    const int i0 = displs[rank];

    std::vector<int> counts3(P), displs3(P);
    for (int r = 0; r < P; ++r) {
        counts3[r] = counts[r] * 3;
        displs3[r] = displs[r] * 3;
    }

    std::vector<double> g_pos3((size_t)par.N * 3, 0.0);
    std::vector<double> g_mass((size_t)par.N, 0.0);

    std::vector<double> l_pos3((size_t)localN * 3, 0.0);
    std::vector<double> l_vel3((size_t)localN * 3, 0.0);
    std::vector<double> l_mass((size_t)localN, 0.0);

    std::vector<double> init_pos3, init_vel3, init_mass;
    if (rank == 0) init_bodies(par.N, init_pos3, init_vel3, init_mass, par.seed);

    MPI_Scatterv(rank == 0 ? init_pos3.data() : nullptr, counts3.data(), displs3.data(), MPI_DOUBLE,
                 l_pos3.data(), localN * 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    MPI_Scatterv(rank == 0 ? init_vel3.data() : nullptr, counts3.data(), displs3.data(), MPI_DOUBLE,
                 l_vel3.data(), localN * 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    MPI_Scatterv(rank == 0 ? init_mass.data() : nullptr, counts.data(), displs.data(), MPI_DOUBLE,
                 l_mass.data(), localN, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    MPI_Allgatherv(l_pos3.data(), localN * 3, MPI_DOUBLE,
                   g_pos3.data(), counts3.data(), displs3.data(), MPI_DOUBLE, MPI_COMM_WORLD);

    MPI_Allgatherv(l_mass.data(), localN, MPI_DOUBLE,
                   g_mass.data(), counts.data(), displs.data(), MPI_DOUBLE, MPI_COMM_WORLD);

    std::vector<double> acc_old, acc_new;

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    std::ofstream out;
    if (rank == 0) {
        out.open("traj.csv");
        out << "step,i,x,y,z\n";
    }

    for (int step = 0; step < par.steps; ++step) {
        // a(t)
        compute_acc_local(par, par.N, g_pos3, g_mass, i0, localN, acc_old);

        // pos(t+dt)
        const double dt = par.dt;
        const double half_dt2 = 0.5 * dt * dt;

        for (int il = 0; il < localN; ++il) {
            l_pos3[(size_t)il*3 + 0] += l_vel3[(size_t)il*3 + 0] * dt + acc_old[(size_t)il*3 + 0] * half_dt2;
            l_pos3[(size_t)il*3 + 1] += l_vel3[(size_t)il*3 + 1] * dt + acc_old[(size_t)il*3 + 1] * half_dt2;
            l_pos3[(size_t)il*3 + 2] += l_vel3[(size_t)il*3 + 2] * dt + acc_old[(size_t)il*3 + 2] * half_dt2;
        }

        MPI_Allgatherv(l_pos3.data(), localN * 3, MPI_DOUBLE,
                       g_pos3.data(), counts3.data(), displs3.data(), MPI_DOUBLE, MPI_COMM_WORLD);

        if (rank == 0) {
            for (int i = 0; i < par.N; ++i) {
                out << step << "," << i << ","
                    << g_pos3[(size_t)i*3 + 0] << ","
                    << g_pos3[(size_t)i*3 + 1] << ","
                    << g_pos3[(size_t)i*3 + 2] << "\n";
            }
        }

        // a(t+dt)
        compute_acc_local(par, par.N, g_pos3, g_mass, i0, localN, acc_new);

        // vel(t+dt)
        const double half_dt = 0.5 * dt;
        for (int il = 0; il < localN; ++il) {
            l_vel3[(size_t)il*3 + 0] += (acc_old[(size_t)il*3 + 0] + acc_new[(size_t)il*3 + 0]) * half_dt;
            l_vel3[(size_t)il*3 + 1] += (acc_old[(size_t)il*3 + 1] + acc_new[(size_t)il*3 + 1]) * half_dt;
            l_vel3[(size_t)il*3 + 2] += (acc_old[(size_t)il*3 + 2] + acc_new[(size_t)il*3 + 2]) * half_dt;
        }
    }
    if (rank == 0) out.close();

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();

    // checksum
    double local_sum = 0.0;
    for (double x : l_pos3) local_sum += x;
    double global_sum = 0.0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "N=" << par.N << " steps=" << par.steps << " dt=" << par.dt
                  << " ranks=" << P << "\n";
        std::cout << "Time: " << std::fixed << std::setprecision(6) << (t1 - t0) << " s\n";
        std::cout << "Checksum(sum(pos)): " << std::setprecision(12) << global_sum << "\n";
    }

    MPI_Finalize();
    return 0;
}