/*
 * Copyright (C) 2026 Romain Graillot
 *
 * This file is part of OSP2.
 *
 * OSP2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSP2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef OSP2_PLAY_LIST_H
#define OSP2_PLAY_LIST_H

#include <cstddef>
#include <optional>
#include <random>
#include <vector>

#include "PlaylistEntry.h"


// Ordered, session-only playlist (no persistence this round). A small final module owned by value
// in Platform and injected into Application; plain RAII, no explicit lifecycle.
// Single-threaded, main-thread only — no mutex (unlike PlayerController): every access happens
// from the UI/update path.
class PlayList final {
private:
    std::vector<PlaylistEntry> m_entries;
    bool m_shuffle = false;
    bool m_repeat = false;
    std::mt19937 m_rng; // advanced by nextIndex() when shuffle picks a random forward target

public:
    PlayList(const PlayList &) = delete;
    PlayList &operator=(const PlayList &) = delete;
    explicit PlayList();
    ~PlayList() = default;

    void add(PlaylistEntry entry);
    void removeAt(std::size_t index); // bounds-checked no-op if out of range
    void clear();

    // Traversal policy for playlist-aware advance. `current` is the playing entry's index, `direction`
    // is +1 (NEXT / auto-advance) or -1 (PREVIOUS). Returns the next index or nullopt when there is no
    // next (empty list, or a boundary with repeat off). Shuffle randomizes only the forward target
    // (direction > 0, size > 1) — PREVIOUS always steps linearly; repeat wraps at either end. Non-const:
    // it advances the shuffle RNG. Deliberate simplification: no shuffle history for PREVIOUS.
    [[nodiscard]] std::optional<std::size_t> nextIndex(std::size_t current, int direction);

    [[nodiscard]] bool shuffle() const;
    [[nodiscard]] bool repeat() const;
    void setShuffle(bool value);
    void setRepeat(bool value);
    void toggleShuffle();
    void toggleRepeat();

    [[nodiscard]] const std::vector<PlaylistEntry> &entries() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool isEmpty() const;
};


#endif //OSP2_PLAY_LIST_H
