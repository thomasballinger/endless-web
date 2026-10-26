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

#include <cstdlib>
#include <mutex>
#include <random>
#include <string>

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

	uint64_t mainDraws = 0;

	// The stream the simulation is taking numbers from on this thread, if any.
	thread_local Random::Stream *currentStream = nullptr;

	bool useStreams = [] {
		const char *setting = getenv("ES_RANDOM_STREAMS");
		return setting && string(setting) == "1";
	}();
}



// This is SplitMix64, which has a single 64-bit word of state and passes
// standard statistical tests.
Random::Stream::result_type Random::Stream::operator()()
{
	uint64_t z = (state += 0x9e3779b97f4a7c15ull);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	return z ^ (z >> 31);
}



bool Random::Stream::IsSeeded() const
{
	return isSeeded;
}



void Random::Stream::Seed(uint64_t seed)
{
	state = seed;
	this->seed = seed;
	isSeeded = true;
}



double Random::Stream::SeedFraction() const
{
	Stream copy;
	copy.state = seed ^ 0x2545f4914f6cdd1dull;
	return (copy() >> 11) * 0x1p-53;
}



Random::Scope::Scope(Stream &stream)
{
	if(!useStreams)
		return;
	SeedStream(stream);
	previous = currentStream;
	currentStream = &stream;
	isActive = true;
}



Random::Scope::~Scope()
{
	if(isActive)
		currentStream = previous;
}



bool Random::UseStreams()
{
	return useStreams;
}



void Random::SetUseStreams(bool use)
{
	useStreams = use;
}



uint64_t Random::MainDrawCount()
{
	lock_guard<mutex> lock(generatorMutex);
	return mainDraws;
}



void Random::SeedStream(Stream &stream)
{
	if(!useStreams || stream.IsSeeded())
		return;
	uint64_t seed = 0;
	if(currentStream)
		seed = (*currentStream)();
	else
	{
		lock_guard<mutex> lock(generatorMutex);
	++mainDraws;
		seed = gen();
	}
	stream.Seed(seed);
}



Random::Stream &Random::EffectStream()
{
	thread_local Stream stream = [] {
		Stream seeded;
		seeded.Seed(0x5eed);
		return seeded;
	}();
	return stream;
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
	if(currentStream)
		return (*currentStream)() >> 32;
	lock_guard<mutex> lock(generatorMutex);
	++mainDraws;
	return uniform(gen);
}



uint32_t Random::Int(uint32_t upper_bound)
{
	if(currentStream)
	{
		const uint32_t x = (*currentStream)() >> 32;
		return (static_cast<uint64_t>(x) * static_cast<uint64_t>(upper_bound)) >> 32;
	}
	lock_guard<mutex> lock(generatorMutex);
	++mainDraws;
	const uint32_t x = uniform(gen);
	return (static_cast<uint64_t>(x) * static_cast<uint64_t>(upper_bound)) >> 32;
}



double Random::Real()
{
	if(currentStream)
		return ((*currentStream)() >> 11) * 0x1p-53;
	lock_guard<mutex> lock(generatorMutex);
	++mainDraws;
	return real(gen);
}



// Return the expected number of failures before k successes, when the
// probability of success is p. The mean value will be k / (1 - p).
uint32_t Random::Polya(uint32_t k, double p)
{
	negative_binomial_distribution<uint32_t> polya(k, p);
	if(currentStream)
		return polya(*currentStream);
	lock_guard<mutex> lock(generatorMutex);
	++mainDraws;
	return polya(gen);
}



// Get a number from a binomial distribution (i.e. integer bell curve).
uint32_t Random::Binomial(uint32_t t, double p)
{
	binomial_distribution<uint32_t> binomial(t, p);
	if(currentStream)
		return binomial(*currentStream);
	lock_guard<mutex> lock(generatorMutex);
	++mainDraws;
	return binomial(gen);
}



// Get a normally distributed number with standard or specified mean and stddev.
double Random::Normal(double mean, double sigma)
{
	if(currentStream)
		return sigma * normal_distribution<double>()(*currentStream) + mean;
	lock_guard<mutex> lock(generatorMutex);
	++mainDraws;
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
