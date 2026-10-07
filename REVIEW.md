# Source publication review — 2026-10-07

The user authorized a separate public source repository. Existing backups
remain private and their Git history is not reused. This is an altered fork.

## Scope

Only active compilation roots, required headers, notices, the Makefile and six
explicitly reviewed original/Noto-derived resources are included. Every copied
file has a size and SHA-256 inventory entry. A historical non-executable comment
transcribing Nintendo's entire BMG is removed from the public header copy;
the implementation and original author notice are unchanged.

Excluded: old Git history, screenshots, extracted Nintendo HTML/fonts/UI packs,
themes, WADs, NAND dumps, IOS79 experiments, application/object binaries,
private signing material, user settings, hardware logs and diagnostic captures.
Original PC files and backups are preserved. Automated token/local-path scans
supplement manual review; they cannot prove absence of every possible secret.

## Resolved source and notice gaps

- The legacy object-only patcher is no longer linked or distributed. Readable
  C++ preserves tables, validation order, memory aliases, failure codes,
  write order and protection restoration. Its reference object matches the
  original project's publicly supplied Git blob. Tests compare actual new C++
  returns and write traces with emulated legacy PowerPC over **2,912 cases**:
  all supported combinations, both directions, unusual nonzero flags, every
  failed patch word, unsupported markers and disabled protection. The compiled
  new native PowerPC backend separately passes the same 2,912 cases. Wii cache
  timing and real-hardware startup are not certified by these tests.
- The original project labels itself GPL; GPL-v2 files retain their notices.
  Full GPL v2 terms accompany the combined distribution.
- Paul E. Jones's author-posted SHA-1 archive and fingerprint were checked.
  Core function bodies match; its Freeware Public License grant is preserved.
- The matching Apple IEEE helper's copyright, warranty and express reuse/
  distribution permission are retained in source and a separate notice.
- The cited ASH reference's MIT notice is preserved. Its first published ASH
  commit is pinned for provenance, not falsely identified as the exact revision
  used to write this fork's bounds-checked C++ implementation.
- Renderer, Monocypher, TinyXML, sigslot, RSA MD5, libogc and Noto notices are
  retained. External SDK libraries are not vendored; their licenses still apply.

See [LICENSES/PROVENANCE.md](LICENSES/PROVENANCE.md) for primary references.
This documents release checks, not legal advice or a claim that all possible
historical dependencies have been exhaustively certified.
