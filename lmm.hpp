#pragma once

#include "linalloc.hpp"
#include <memory_resource>
#include <functional>
#include <utility>
#include <cstdint>
#include <span>

namespace vil {

// Implementation of the lazy matrix march algorithm.
// It implements the fuzzy longest common subsequence (FLCS) problem,
// where (compared to the common LCS) problem, you work with
// match values in range [0, 1] instead of binary equality. Most
// fast (i.e. better than the trivial O(n^2)) solutions to LCS
// are firmly based on the binary equality and can't easily be
// extended to match values. Some even depend on a finite alphabet
// in the sequences.
//
// The algorithm has a worst-case runtime O(n^2) where n is the
// maximum number of elements in the given sequences. But in the case
// of mostly similar sequences, it will be ~O(n).
// The idea (and implementation) of the algorithm can be described
// as a best-path finding through the lazily evaluated matching matrix.
// Memory consumption is currently always O(n^2) since this was never
// a big problem. It could be reduced, being ~O(n) for the well-matching
// cases as well.
struct LazyMatrixMarch {
	// Describes a match between the ith sequence item in the first sequence
	// with the jth sequence item in the second sequence, with a match
	// equal to 'matchVal'.
	struct ResultMatch {
		u32 i;
		u32 j;
		float matchVal;
	};

	struct Result {
		// accumulated matching value of the best path
		float totalMatch;
		// All the matches found on the best path
		// Span lifetime depends on LinAllocator this is constructed with
		span<ResultMatch> matches;
	};

	struct EvalMatch {
		// The result of the matcher function at this position.
		// Lazily evaluated, -1.f if it never was called
		float eval {-1.f};
		// The best path found so far to this position
		// -1.f when we never had a path here
		float best {-1.f};
	};

	// The function evaluating the match between the ith element in the
	// first sequence with the jth element in the second sequence.
	// Note how the LazyMatrixMarch algorithm itself never sees the sequences
	// itself, does not care about their types of properties.
	// Expected to return a matching value in range [0, 1] where 0
	// means no match and a value >0 means there's a match, returning
	// it's weight/value/importance/quality.
	using Matcher = std::function<float(u32 i, u32 j)>;

	// width: length of the first sequence
	// height: length of the second sequence
	// alloc: an allocator guaranteed to outlive this
	// matcher: the matching functions holding information about the sequences
	LazyMatrixMarch(u32 width, u32 height, LinAllocator& alloc,
		Matcher matcher, float branchThreshold = 0.95);

	// Runs the algorithm to completion (can also be called if 'step' was
	// called before) and returns the best path and its matches.
	Result run();

	// Returns false if there's nothing to do anymore.
	bool step();

	u32 width() const { return width_; }
	u32 height() const { return height_; }

	// debug information
	u32 numEvals() const { return numEvals_; }
	u32 numSteps() const { return numSteps_; }

private:
	struct Candidate {
		u32 i;
		u32 j;
		float score;

		Candidate* prev {};
		Candidate* next {};
	};

	void addCandidate(float score, u32 i, u32 j, u32 addI, u32 addJ);
	EvalMatch& match(u32 i, u32 j) { return matchMatrix_[width() * i + j]; }

	void insertCandidate(u32 i, u32 j, float score);
	Candidate popCandidate();
	Candidate peekCandidate() const;
	void prune(float minScore);
	bool empty() const { return queue_.next == &queue_; }
	float metric(const Candidate& c) const;
	float maxPossibleScore(float score, u32 i, u32 j) const;

private:
	LinAllocator& alloc_;
	u32 width_;
	u32 height_;
	Matcher matcher_;
	span<EvalMatch> matchMatrix_; // lazily evaluated
	float bestMatch_ {-1.f};
	std::pair<u32, u32> bestRes_ {};
	float branchThreshold_;

	Candidate queue_; // linked list, anchor
	Candidate freeList_; // linked list, anchor

	// debug functionality
	u32 numEvals_ {};
	u32 numSteps_ {};
};

float maxPossibleScore(float score, u32 width, u32 height, u32 i, u32 j);

} // namespace vil


