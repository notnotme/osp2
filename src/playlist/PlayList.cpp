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

#include "PlayList.h"

#include <utility>


PlayList::PlayList()
    : m_rng(std::random_device{}()) {}

void PlayList::add(PlaylistEntry entry) {
    m_entries.push_back(std::move(entry));
}

void PlayList::removeAt(const std::size_t index) {
    if (index < m_entries.size()) {
        m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void PlayList::clear() {
    m_entries.clear();
}

bool PlayList::shuffle() const {
    return m_shuffle;
}

bool PlayList::repeat() const {
    return m_repeat;
}

void PlayList::setShuffle(const bool value) {
    m_shuffle = value;
}

void PlayList::setRepeat(const bool value) {
    m_repeat = value;
}

void PlayList::toggleShuffle() {
    m_shuffle = !m_shuffle;
}

void PlayList::toggleRepeat() {
    m_repeat = !m_repeat;
}

std::optional<std::size_t> PlayList::nextIndex(const std::size_t current, const int direction) {
    const std::size_t size = m_entries.size();
    if (size == 0) {
        return std::nullopt;
    }

    // Shuffle only affects the forward/auto-advance target of a multi-entry list; a single entry falls
    // through to the linear case below. Pick a uniform index in [0,size-1) then skip over `current`,
    // yielding a uniform pick among the size-1 other entries in one draw.
    if (m_shuffle && direction > 0 && size > 1) {
        std::uniform_int_distribution<std::size_t> dist(0, size - 2);
        std::size_t pick = dist(m_rng);
        if (pick >= current) {
            ++pick;
        }
        return pick;
    }

    // Linear: covers PREVIOUS always, and NEXT when shuffle is off. Repeat wraps at either end.
    const long t = static_cast<long>(current) + direction;
    if (t >= 0 && t < static_cast<long>(size)) {
        return static_cast<std::size_t>(t);
    }
    if (m_repeat) {
        return t >= static_cast<long>(size) ? std::size_t{0} : size - 1;
    }
    return std::nullopt;
}

const std::vector<PlaylistEntry> &PlayList::entries() const {
    return m_entries;
}

std::size_t PlayList::size() const {
    return m_entries.size();
}

bool PlayList::isEmpty() const {
    return m_entries.empty();
}
