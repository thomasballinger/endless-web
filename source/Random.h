/* Random.h
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

#pragma once

#include <cstdint>
#include <random>



// Collection of functions for generating random numbers with a variety of
// different distributions. (This is done partly because on some systems the
// random number generation is not thread-safe.)
class Random {
public:
	// An independent sequence of random numbers, small enough for every ship
	// and projectile to have its own. While a Scope for a stream exists, the
	// functions below take their numbers from that stream on that thread.
	//
	// With one sequence for everything, any change in what one object does
	// (e.g. a missile following a different course) shifts the numbers every
	// other object gets afterwards, so the whole simulation diverges. With a
	// stream per object, only objects that are affected by the change behave
	// differently. Streams are only used if ES_RANDOM_STREAMS is set to 1;
	// otherwise Scopes do nothing and everything uses one sequence.
	class Stream {
	public:
		using result_type = uint64_t;
		static constexpr result_type min() { return 0; }
		static constexpr result_type max() { return UINT64_MAX; }
		result_type operator()();

		bool IsSeeded() const;
		void Seed(uint64_t seed);
		// A number in [0, 1) that depends only on the stream's seed.
		double SeedFraction() const;

	private:
		uint64_t state = 0;
		uint64_t seed = 0;
		bool isSeeded = false;
	};

	// Draw random numbers from the given stream until this is destroyed. If
	// the stream has not been seeded yet, it is seeded from wherever random
	// numbers came from before this scope.
	class Scope {
	public:
		explicit Scope(Stream &stream);
		~Scope();
		Scope(const Scope &) = delete;
		Scope &operator=(const Scope &) = delete;

	private:
		Stream *previous = nullptr;
		bool isActive = false;
	};

	// Whether streams are used (by default, if ES_RANDOM_STREAMS is 1).
	static bool UseStreams();
	static void SetUseStreams(bool use);
	// How many numbers have been taken from the main sequence (for debugging:
	// with streams, this should not depend on what any one object does).
	static uint64_t MainDrawCount();
	// Seed the given stream from wherever random numbers come from right now,
	// if streams are enabled and it is not seeded yet.
	static void SeedStream(Stream &stream);
	// A stream for effects that only change how the game looks, and so should
	// not take numbers from any object's stream.
	static Stream &EffectStream();

	// Everything about the main generator that decides what it draws next, so
	// that something can set it aside and put it back: e.g. a simulation that
	// runs between frames of the game, which needs its own sequence of numbers
	// whatever else draws numbers in between.
	class Saved {
	private:
		friend class Random;
		std::mt19937_64 generator;
		std::normal_distribution<double> normal;
	};
	static Saved Save();
	static void Restore(const Saved &saved);
	// While one exists, random numbers come from the given sequence (which it
	// updates), rather than the game's: e.g. for a simulation that must make
	// the same choices however often it is replayed, whatever else draws
	// numbers in between its steps.
	class Own {
	public:
		explicit Own(Saved &own);
		~Own();
		Own(const Own &) = delete;
		Own &operator=(const Own &) = delete;

	private:
		Saved &own;
		Saved outside;
	};

	// Seed the generator (e.g. to make it produce exactly the same random
	// numbers it produced previously).
	static void Seed(uint64_t seed);
	// After this method runs, Seed will discard its argument and reseed the prng with the value given here.
	static void SetFixedSeed(uint64_t seed);

	static uint32_t Int();
	static uint32_t Int(uint32_t modulus);

	static double Real();

	// Slower functions for getting random numbers from a given distribution.
	// Do not use these functions in time-critical code.

	// Return the expected number of failures before k successes, when the
	// probability of success is p. The mean value will be k / (1 - p).
	static uint32_t Polya(uint32_t k, double p = .5);
	// Get a number from a binomial distribution (i.e. integer bell curve).
	static uint32_t Binomial(uint32_t t, double p = .5);
	// Get a number from a normal distribution with standard or specified mean and stddev.
	static double Normal(double mean = 0, double sigma = 1);

	// Random numbers for things that only affect how the game looks, such as
	// the star field. These come from a separate generator, so that drawing
	// (which may or may not happen, e.g. in headless tests) can never change
	// the sequence of random numbers the simulation sees.
	static uint32_t CosmeticInt(uint32_t upper_bound);
	static double CosmeticReal();
};
