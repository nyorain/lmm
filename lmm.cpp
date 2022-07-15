#include "lmm.hpp"
#include "list.hpp"

namespace vil {

float maxPossibleScore(float score, u32 width, u32 height, u32 i, u32 j) {
	return score + std::min(width - i, height - j);
}

// LazyMatrixMarch
LazyMatrixMarch::LazyMatrixMarch(u32 width, u32 height, LinAllocator& alloc,
	Matcher matcher, float branchThreshold) :
		alloc_(alloc), width_(width), height_(height), matcher_(std::move(matcher)),
		branchThreshold_(branchThreshold) {
	dlg_assert(width > 0);
	dlg_assert(height > 0);
	dlg_assert(matcher_);

	// add initial candidate
	insertCandidate(0, 0, 0.f);
}

void LazyMatrixMarch::addCandidate(float score, u32 i, u32 j, u32 addI, u32 addJ) {
	if(i + addI >= width() || j + addJ >= height()) {
		// we have a finished run.
		if(score > bestMatch_) {
			bestMatch_ = score;
			bestRes_ = {i, j};
		}

		return;
	}

	auto maxPossible = maxPossibleScore(score, i + addI, j + addJ);
	if(maxPossible > bestMatch_) {
		insertCandidate(i + addI, j + addJ, score);
	}
}

bool LazyMatrixMarch::step() {
	if(empty()) {
		return false;
	}

	++numSteps_;
	const auto cand = popCandidate();

	// should be true due to pruning
	// (i guess can be false when our metric does not fulfill the
	// assumption used in prune(newScore) about the ordering)
	dlg_assert(maxPossibleScore(cand.score, cand.i, cand.j) >= bestMatch_);

	auto& m = this->match(cand.i, cand.j);
	if(m.best >= cand.score + 1) {
		return true;
	}

	if(m.eval == -1.f) {
		m.eval = matcher_(cand.i, cand.j);
		++numEvals_;
	}

	auto newScore = cand.score + m.eval;
	if(newScore > m.best) {
		m.best = newScore;

		if(m.eval > 0.f) {
			addCandidate(newScore, cand.i, cand.j, 1, 1);

			// throw out all candidates that can't even reach what we have
			prune(newScore);
		}

		// TODO: yeah with fuzzy matching we should always branch
		// out... This will generate so many candidates tho :(
		// otoh they have a lower score so won't be considered.
		// And for perfect matches we still only generate 3 * n
		// candidates total.
		// TODO: only threshold = 1.f is guaranteed to be 100% correct,
		// otherwise it's a heuristic.
		if(m.eval < branchThreshold_) {
			addCandidate(cand.score, cand.i, cand.j, 1, 0);
			addCandidate(cand.score, cand.i, cand.j, 0, 1);
		}
	}

	return true;
}

LazyMatrixMarch::Result LazyMatrixMarch::run() {
	// run algorithm
	while(step()) /*noop*/;

	// gather results
	Result res;
	auto maxMatches = std::min(width(), height());
	res.matches = alloc_.alloc<ResultMatch>(maxMatches);
	auto outID = maxMatches - 1;

	dlg_assert(bestMatch_ >= 0.f);

	auto [i, j] = bestRes_;

	while(i > 0 && j > 0) {
		auto& score = match(i, j);
		auto& up = match(i - 1, j);
		if(up.best == score.best) {
			--i;
			continue;
		}

		auto& left = match(i, j - 1);
		if(left.best == score.best) {
			--j;
			continue;
		}

		auto& diag = match(i - 1, j - 1);
		dlg_assert(diag.best < score.best);
		dlg_assert(diag.eval > 0.f && diag.eval <= 1.f);
		dlg_assert(diag.eval == score.best - diag.best);

		--i;
		--j;

		dlg_assert(outID < res.matches.size()); // wrap-around check
		res.matches[outID] = {i, j, diag.eval};
		--outID;
	}

	res.matches = res.matches.last(maxMatches - outID);
	return res;
}

float LazyMatrixMarch::maxPossibleScore(float score, u32 i, u32 j) const {
	return vil::maxPossibleScore(score, width_, height_, i, j);
}

void LazyMatrixMarch::insertCandidate(u32 i, u32 j, float score) {
	Candidate* cand;
	if(freeList_.next != &freeList_) {
		cand = freeList_.next;
		unlink(*cand);
	} else {
		cand = &alloc_.construct<Candidate>();
	}

	cand->i = i;
	cand->j = j;
	cand->score = score;

	auto it = queue_.next;
	while(it != &queue_ && metric(*it) > metric(*cand)) {
		it = it->next;
	}

	insertBefore(*it, *cand);
}

LazyMatrixMarch::Candidate LazyMatrixMarch::popCandidate() {
	dlg_assert(!empty());
	auto ret = *queue_.next;

	auto& newFree = *queue_.next;
	unlink(newFree);
	insertAfter(freeList_, newFree);

	return ret;
}

LazyMatrixMarch::Candidate LazyMatrixMarch::peekCandidate() const {
	dlg_assert(!empty());
	return *queue_.next;
}

void LazyMatrixMarch::prune(float minScore) {
	// PERF: can be implement more efficiently, unlinking and
	// inserting the whole sub-linked-list
	// TODO: assumed we have maxPossibleScore as metric, not working
	// perfectly with other (e.g. score-based)

	auto it = queue_.prev;
	while(it != &queue_ && maxPossibleScore(it->score, it->i, it->j) < minScore) {
		auto prev = it->prev;

		auto& newFree = *it;
		unlink(newFree);
		insertAfter(freeList_, newFree);

		it = prev;
	}

	// alternative (but SLOW) implementation without metric assumption
#if 0
	auto it = queue.prev;
	while(it != &queue /*&& maxPossibleScore(it->score, it->i, it->j) < minScore*/) {
		auto prev = it->prev;

		if(maxPossibleScore(it->score, it->i, it->j) < minScore) {
			auto& newFree = unlink(*it);
			insertAfter(freeList, newFree);
		}

		// minimum ordering assumption
		if(it->score >= minScore) {
			break;
		}

		it = prev;
	}
#endif // 0
}

float LazyMatrixMarch::metric(const Candidate& c) const {
	// The '+ 0.01 * ' parts are basically tie-breakers.

	// Metric: prefer high score.
	// This basically means we go depth-first
	// Pro: we have a path through the matrix very quickly, allowing
	//   us to prune/not consider some bad cases
	// Contra: often we will run many complete paths through the matrix,
	//   looks like candidate bubbles. So a lot of iteration needed
	//   in the end to be sure we have the best path.
	// return c.score + 0.01 * maxPossibleScore(c.score, c.i, c.j);

	// Metric: prefer high possible score.
	// This is more like breadth-first.
	// Pro: allows efficient pruning (see prune). Also results in a lower
	//   number of total iterations
	// Contra: we might evaluate candidates we could have excluded
	//   otherwise
	return maxPossibleScore(c.score, c.i, c.j) + 0.01 * c.score;

	// Mixed metric. Should investigate this.
	// return maxPossibleScore(c.score, c.i, c.j) + c.score;
}

} // namespace vil

