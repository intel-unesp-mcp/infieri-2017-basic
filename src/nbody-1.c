#include <math.h>
#include <mkl_vsl.h>
#include <omp.h>
#include <stdio.h>

typedef struct { float x, y, z, vx, vy, vz; } ParticleType;

int main(const int argc, const char** argv) {
    const int nParticles = 30000, nSteps = 20; // Simulation parameters
    const float dt = 0.01f; // Particle propagation time step
    // Particle data stored as an array of structures
    ParticleType* particle = (ParticleType*) malloc(nParticles*sizeof(ParticleType));

    // Initialize particles
    VSLStreamStatePtr rnStream; vslNewStream( &rnStream, VSL_BRNG_MT19937, 1 );
    vsRngUniform(VSL_RNG_METHOD_UNIFORM_STD, rnStream, 6*nParticles, (float*)particle, -1.0f, 1.0f);

    // Propagate particles
    printf("Propagating particles using %d threads...\n", omp_get_max_threads());
    double rate = 0.0, dRate = 0.0; // Benchmarking data
    const int skipSteps = 1; // Skip first iteration is warm-up on Xeon Phi coprocessor
    for (int step = 1; step <= nSteps; step++) {
        const double tStart = omp_get_wtime(); // Start timing

        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < nParticles; i++) { // Parallel loop over particles that experience force
            float Fx = 0.0f; float Fy = 0.0f; float Fz = 0.0f; // Components of force on particle i

            for (int j = 0; j < nParticles; j++) { // Vectorized loop over particles that exert force
                if (j != i) {
                    // Newton's law of universal gravity calculation.
                    const float dx = particle[j].x - particle[i].x;
                    const float dy = particle[j].y - particle[i].y;
                    const float dz = particle[j].z - particle[i].z;
                    const float drSquared = dx*dx + dy*dy + dz*dz;
                    const float drPowerN32 = 1.0f/(drSquared*sqrtf(drSquared));

                    // Reduction to calculate the net force
                    Fx += dx * drPowerN32; Fy += dy * drPowerN32; Fz += dz * drPowerN32;
                }
            }
            // Move particles in response to the gravitational force
            particle[i].vx += dt*Fx; particle[i].vy += dt*Fy; particle[i].vz += dt*Fz;
        }
        for (int i = 0 ; i < nParticles; i++) { // Not much work, serial loop
            particle[i].x += particle[i].vx*dt;
            particle[i].y += particle[i].vy*dt;
            particle[i].z += particle[i].vz*dt;
        }

        const double tEnd = omp_get_wtime(); // End timing
        if (step > skipSteps) // Collect statistics
            { rate += 1.0/(tEnd - tStart); dRate += 1.0/((tEnd - tStart)*(tEnd-tStart)); }
        printf("Step %d: %.3f seconds\n", step, (tEnd-tStart));
    }
    rate/=(double)(nSteps-skipSteps); dRate=sqrt(dRate/(double)(nSteps-skipSteps)-rate*rate);
    printf("Average rate for iterations %d through %d: %.3f +/- %.3f steps per second.\n", skipSteps+1, nSteps, rate, dRate);

    free(particle);
}
