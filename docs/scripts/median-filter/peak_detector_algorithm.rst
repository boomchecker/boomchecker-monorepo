Algorithm Overview
==================

Motivation and Purpose
----------------------

The detector isolates short impulse-like peaks in noisy recordings without
requiring heap allocations. It keeps a sliding window of multiple taps (blocks)
and uses per-offset medians to estimate background noise. The design trades a
small amount of memory for deterministic runtime and predictable latency.

Terminology
-----------

- **tap**: A contiguous block of ``tap_size`` samples processed together.
- **offset**: Sample position inside a tap (``0 .. tap_size-1``).
- **middle tap**: The chronological middle block inside the sliding window. A
  peak must appear here to be eligible because surrounding taps provide
  context.

Architecture
------------

- A ring buffer holds ``num_taps`` blocks, each of length ``tap_size``.
- For every offset there are two heaps (max/min) that store the samples for that
  offset across all taps. Lazy invalidation via generation counters avoids
  deleting stale nodes when the write cursor wraps.
- A running RMS accumulator sums squares of all samples in the window to deliver
  a dynamic noise estimate.

Step-by-step Algorithm
----------------------

1. **Fill the window**: Push blocks into the ring until ``num_taps`` taps are
   available. Each sample updates its per-offset heaps and the RMS sum.
2. **Noise estimate via median**: For every offset, take the max-heap top as the
   median of that offset across taps. This forms a noise profile for the entire
   middle tap.
3. **Find maximum deviation**: In the middle tap, find the offset whose sample
   deviates the most (positive) from its noise median. This is the peak
   candidate.
4. **RMS gate**: Convert the RMS accumulator into ``rms_noise`` and require that
   ``peak_value > det_level`` *and* ``peak_value > det_rms * rms_noise``.
5. **Before/after energy test**: Compute medians inside the middle tap before
   and after the candidate offset. The "before" energy must exceed the "after"
   energy scaled by ``det_energy``. This rejects symmetric spikes or pure noise.
6. **Absolute position**: Translate the offset in the middle tap to an absolute
   position by combining ``block_start_offset`` with tap indices. The newest tap
   is always known, so the middle tap index is derived relative to it.

Differences vs. Paper + Rationale
---------------------------------

- **Lazy invalidation**: Instead of removing old samples, generation counters
  mark stale nodes. This keeps updates O(log N) and avoids heap compaction on
  embedded targets.
- **Fixed-size buffers**: All memory is supplied by the caller; no dynamic
  allocation occurs. This makes the detector predictable and safe for RT
  contexts.
- **Truncated median slices**: The small before/after median uses a fixed stack
  buffer (``MEDIAN_SLICE_MAX_LEN``) to avoid heap use. In practice ``tap_size``
  is well within that bound.

API Integration
---------------

The Sphinx API section is powered by Doxygen XML via Breathe. Key entry points:

- ``detector_state_size``: Compute required buffer size for a configuration.
- ``detector_init`` / ``detector_reset``: Prepare or clear the detector state
  without dynamic allocation.
- ``detector_feed_block``: Online processing that returns peak hits as soon as
  the window is full.
- ``detect_recording_i16``: Convenience offline routine for entire recordings.
