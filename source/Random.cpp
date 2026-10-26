/* Random.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "Random.h"

#include <mutex>
#include <random>

using namespace std;

// A single generator is shared by all threads, so that seeding it (e.g. with
// --rngseed) makes the simulation reproducible no matter which worker thread
// runs each step.
namespace {
	mutex generatorMutex;
	mt19937_64 gen;
	uniform_int_distribution<uint32_t> uniform;
	uniform_real_distribution<double> real;
	normal_distribution<double> normal;

	mutex cosmeticMutex;
	mt19937_64 cosmeticGen;
	uniform_int_distribution<uint32_t> cosmeticUniform;
	uniform_real_distribution<double> cosmeticReal;

	bool useFixedSeed = false;
	uint64_t fixedSeed = 0;
}



// Seed the generator (e.g. to make it produce exactly the same random
// numbers it produced previously).
void Random::Seed(uint64_t seed)
{
	lock_guard<mutex> lock(generatorMutex);
	gen.seed(useFixedSeed ? fixedSeed : seed);
}



void Random::SetFixedSeed(uint64_t seed)
{
	useFixedSeed = true;
	fixedSeed = seed;
}



uint32_t Random::Int()
{
	lock_guard<mutex> lock(generatorMutex);
	return uniform(gen);
}



uint32_t Random::Int(uint32_t upper_bound)
{
	lock_guard<mutex> lock(generatorMutex);
	const uint32_t x = uniform(gen);
	return (static_cast<uint64_t>(x) * static_cast<uint64_t>(upper_bound)) >> 32;
}



double Random::Real()
{
	lock_guard<mutex> lock(generatorMutex);
	return real(gen);
}



// Return the expected number of failures before k successes, when the
// probability of success is p. The mean value will be k / (1 - p).
uint32_t Random::Polya(uint32_t k, double p)
{
	negative_binomial_distribution<uint32_t> polya(k, p);
	lock_guard<mutex> lock(generatorMutex);
	return polya(gen);
}



// Get a number from a binomial distribution (i.e. integer bell curve).
uint32_t Random::Binomial(uint32_t t, double p)
{
	binomial_distribution<uint32_t> binomial(t, p);
	lock_guard<mutex> lock(generatorMutex);
	return binomial(gen);
}



// Get a normally distributed number with standard or specified mean and stddev.
double Random::Normal(double mean, double sigma)
{
	lock_guard<mutex> lock(generatorMutex);
	return sigma * normal(gen) + mean;
}



uint32_t Random::CosmeticInt(uint32_t upper_bound)
{
	lock_guard<mutex> lock(cosmeticMutex);
	const uint32_t x = cosmeticUniform(cosmeticGen);
	return (static_cast<uint64_t>(x) * static_cast<uint64_t>(upper_bound)) >> 32;
}



double Random::CosmeticReal()
{
	lock_guard<mutex> lock(cosmeticMutex);
	return cosmeticReal(cosmeticGen);
}
