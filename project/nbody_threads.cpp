#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>
#include <random>
#include <ctime>
#include <barrier> 


const double G = 1.0;
const double DT = 0.005;
const int NUM_STEPS = 1500;
const int N = 1024;
const int NUM_THREADS = 16;

const int POZITION_RANGE = 400;
const int VELOCITY_RANGE = 8;
const int MASS = 200;


struct Particle {
    double x, y, z;
    double vx, vy, vz;
    double ax, ay, az;
    double mass;
};


void compute_forces(std::vector<Particle>& particles, int start, int end) {
    for (int i = start; i < end; ++i) {
        double fx = 0.0, fy = 0.0, fz = 0.0;
        for (int j = 0; j < particles.size(); ++j) {
            if (i == j) continue;

            double dx = particles[j].x - particles[i].x;
            double dy = particles[j].y - particles[i].y;
            double dz = particles[j].z - particles[i].z;

            double dist_sq = dx * dx + dy * dy + dz * dz + 1e-2;
            double dist = std::sqrt(dist_sq);
            double force = (G * particles[i].mass * particles[j].mass) / dist_sq;

            fx += force * (dx / dist);
            fy += force * (dy / dist);
            fz += force * (dz / dist);
        }
        particles[i].ax = fx / particles[i].mass;
        particles[i].ay = fy / particles[i].mass;
        particles[i].az = fz / particles[i].mass;
    }
}

void integrate_first_half(std::vector<Particle>& particles, int start, int end) {
    for (int i = start; i < end; ++i) {
        particles[i].vx += particles[i].ax * 0.5 * DT;
        particles[i].vy += particles[i].ay * 0.5 * DT;
        particles[i].vz += particles[i].az * 0.5 * DT;

        particles[i].x += particles[i].vx * DT;
        particles[i].y += particles[i].vy * DT;
        particles[i].z += particles[i].vz * DT;
    }
}

void integrate_second_half(std::vector<Particle>& particles, int start, int end) {
    for (int i = start; i < end; ++i) {
        particles[i].vx += particles[i].ax * 0.5 * DT;
        particles[i].vy += particles[i].ay * 0.5 * DT;
        particles[i].vz += particles[i].az * 0.5 * DT;
    }
}


void worker(int id, std::vector<Particle>& particles, std::barrier<>& bariera,
    int start_idx, int end_idx, std::ofstream* outfile) {

    // Pre-calculate forces
    compute_forces(particles, start_idx, end_idx);

    bariera.arrive_and_wait();

    for (int step = 0; step < NUM_STEPS; ++step) {
        integrate_first_half(particles, start_idx, end_idx);
        bariera.arrive_and_wait();

        compute_forces(particles, start_idx, end_idx);
        bariera.arrive_and_wait();

        integrate_second_half(particles, start_idx, end_idx);

        if (id == 0) {
            for (size_t i = 0; i < particles.size(); ++i) {
                *outfile << step << "," << i << ","
                    << particles[i].x << ","
                    << particles[i].y << ","
                    << particles[i].z << "\n";
            }
        }

        // Wait for I/O to finish
        bariera.arrive_and_wait();
    }
}


int main() {
    std::vector<Particle> particles(N);
    srand((unsigned int)time(NULL));

    for (int i = 0; i < N; ++i) {
        particles[i].x = ((double)rand() / RAND_MAX) * POZITION_RANGE - (POZITION_RANGE / 2.0);
        particles[i].y = ((double)rand() / RAND_MAX) * POZITION_RANGE - (POZITION_RANGE / 2.0);
        particles[i].z = ((double)rand() / RAND_MAX) * POZITION_RANGE - (POZITION_RANGE / 2.0);

        particles[i].vx = ((double)rand() / RAND_MAX) * VELOCITY_RANGE - (VELOCITY_RANGE / 2.0);
        particles[i].vy = ((double)rand() / RAND_MAX) * VELOCITY_RANGE - (VELOCITY_RANGE / 2.0);
        particles[i].vz = ((double)rand() / RAND_MAX) * VELOCITY_RANGE - (VELOCITY_RANGE / 2.0);

        particles[i].mass = ((double)rand() / RAND_MAX) * MASS + 10.0;
    }

    std::ofstream outfile;
    outfile.open("traj.csv");
    outfile << "step,i,x,y,z\n";


    std::barrier<> bariera(NUM_THREADS);

    std::vector<std::thread> threads;
    int chunk = N / NUM_THREADS;

    std::cout << "Start (" << N << " bodies, " << NUM_THREADS << " threads)" << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_THREADS; ++i) {
        int start = i * chunk;
        int end = (i == NUM_THREADS - 1) ? N : (i + 1) * chunk;
        threads.emplace_back(worker, i, std::ref(particles), std::ref(bariera),
            start, end, &outfile);
    }

    for (auto& t : threads) t.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    outfile.close();
    std::cout << "Time: " << duration.count() << " seconds" << std::endl;

    return 0;
}