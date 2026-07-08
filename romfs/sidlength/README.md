# HVSC Songlengths database

`Songlengths.md5` is the **Songlengths database** from the
[High Voltage SID Collection (HVSC)](https://www.hvsc.c64.org/), compiled and
maintained by the HVSC crew. It maps each SID tune's MD5 hash to the play time
of every subtune, and OSP2 uses it to report real track lengths for Commodore 64
SID files (see [`docs/audio.md`](../../docs/audio.md) and `src/player/SongLengthDb.*`).

## Source

Distributed as part of the HVSC releases (`DOCUMENTS/Songlengths.md5`) —
<https://www.hvsc.c64.org/>. It is updated with each HVSC release; refresh this
file from the matching release when updating.

## License / redistribution

The HVSC and its documentation, including the Songlengths database, are **freely
redistributable** per the HVSC terms. OSP2 bundles the file unmodified and claims
no ownership over it; all credit belongs to the HVSC crew. See the HVSC website
for the current terms.
