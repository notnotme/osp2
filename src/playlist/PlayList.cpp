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


void PlayList::create() {
    // Nothing to allocate — the entry vector and flags are default-constructed. Kept for lifecycle
    // symmetry with the other subsystems and for future use.
}

void PlayList::destroy() {
    m_entries.clear();
}

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

const std::vector<PlaylistEntry> &PlayList::entries() const {
    return m_entries;
}

std::size_t PlayList::size() const {
    return m_entries.size();
}

bool PlayList::isEmpty() const {
    return m_entries.empty();
}
