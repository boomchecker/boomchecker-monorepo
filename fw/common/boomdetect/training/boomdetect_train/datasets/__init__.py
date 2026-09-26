"""Datasets: where the audio comes from, what each clip is, and the frame cache.

A *manifest* is the single table every later stage works from: one row per
clip with its source, label, category, grouping key and split. Building it is
the only place that knows a directory layout or a parquet schema. The *frame
cache* stores the front end's output for every clip once (RMS, MFCC, log-mel
and the spectral scalars per frame), so that training a new model family or
changing a window policy never touches audio again.
"""
