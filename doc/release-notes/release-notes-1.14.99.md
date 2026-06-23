Dogecoin Core version 1.14.99 (development) release notes
==========================================================

Notable changes
===============

UTXO snapshot RPCs
------------------

This branch adds support for exporting and importing UTXO snapshots:

- `dumptxoutset "path"` writes a snapshot file and returns metadata including
  `txoutset_hash` and `nchaintx`.
- `loadtxoutset "path" "txoutset_hash"` validates and loads a snapshot into the
  active chainstate.

For this development backport, `loadtxoutset` uses a single-chainstate restore
flow: the snapshot base block must already be known and on the active chain, and
the node rewinds to genesis before loading and activating the snapshot tip.
