# Lazy Matrix March

Implementation of the lazy matrix march algorithm.
It implements the fuzzy longest common subsequence (FLCS) problem,
where (compared to the common LCS) problem, you work with
match values in range [0, 1] instead of binary equality. Most
fast (i.e. better than the trivial O(n^2)) solutions to LCS
are firmly based on the binary equality and can't easily be
extended to fuzzy matching. Some even depend on a finite alphabet
in the sequences. Our algorithm instead reinterprets the problem
to a path-finding problem through the matching matrix between the
elements in the sequences, allowing us to evaluate the matching matrix lazily.

The algorithm still has a worst-case runtime O(n^2) where n is the
maximum number of elements in the given sequences. But in the case
of mostly similar sequences, it will be ~O(n).
Memory consumption is currently always O(n^2) but could be reduced, 
being ~O(n) for the well-matching cases as well (memory just wasn't
a concern so far).

Files:
- [lmm.hpp](lmm.hpp), [lmm.cpp](lmm.cpp): Main implementation of the algorithm
- [linalloc.hpp](linalloc.hpp), [linalloc.cpp](linalloc.cpp): Utility linear
  block-based allocator, to avoid many tiny allocations in the LMM algorithm
  itself. Feel free to replace it with your own allocator but we have
  some requirements regarding the lifetime (and currently don't
  free anything as it's not needed with the way the linear allocator
  is usually used)
- [common.hpp](common.hpp): some compatibility macros so the source could mostly
  be left as is was in [vil](github.com/nyorain/vil)
- [meson.build](meson.build): Simple build script

---

This was originally developed for [vil](https://github.com/nyorain/vil),
where we need this for command hierarchy matching, associating
commands between different frames and submissions.
Due to the hierarchical nature of our matching (and the way applications
usually submit very similar workloads in each frame), we are interested
in making the case of similar sequences fast.
