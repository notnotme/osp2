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

#ifndef OSP2_AUDIO_TAP_H
#define OSP2_AUDIO_TAP_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>


// Lock-free single-producer / single-consumer publisher of the most recently
// decoded block of interleaved stereo int16 frames, for the visualization layer.
// The block is stored as int16 (the pipeline's native sample type) and converted
// to normalized float on read(), because the visualization layer wants float.
//
// It is a seqlock (Rigtorp-style): the producer (SDL audio thread) publishes the
// just-decoded block from PlayerController::decode(); the consumer (main thread)
// reads the latest block for rendering. Neither side blocks: the producer never
// waits, and a reader that observes a write in progress simply retries and copies
// again. A torn read is therefore possible but never observed — the sequence check
// discards it and the loop re-copies until it captures a stable snapshot. There is
// no lock and no heap allocation, so this is safe to call from the real-time audio
// callback without risking an underrun.
class AudioTap final {
public:
    // Must match PlayerController::BUFFER_FRAMES and PlayerController::CHANNELS.
    // Kept independent here so AudioTap.h does not depend on PlayerController.h
    // (PlayerController owns an AudioTap by value).
    static constexpr std::size_t MAX_FRAMES = 1024;
    static constexpr std::size_t CHANNELS = 2;

private:
    static constexpr std::size_t CAPACITY = MAX_FRAMES * CHANNELS;

    // Even = stable snapshot, odd = write in progress. Starts at 0 (never written).
    std::atomic<std::uint32_t> m_seq;
    // Guarded by m_seq, not by any mutex: only valid between two equal even reads.
    std::int16_t m_samples[CAPACITY];
    std::size_t m_frameCount;

public:
    AudioTap(const AudioTap &) = delete;
    AudioTap &operator=(const AudioTap &) = delete;
    AudioTap(AudioTap &&) = delete;
    AudioTap &operator=(AudioTap &&) = delete;

    AudioTap() noexcept
        : m_seq(0),
          m_samples{},
          m_frameCount(0) {}

    ~AudioTap() = default;

    // Producer (SDL audio thread). Copies frameCount interleaved stereo int16 frames in and
    // publishes them. Never blocks and never allocates; frameCount is clamped to MAX_FRAMES.
    void publish(const std::int16_t *frames, std::size_t frameCount) noexcept {
        if (frameCount > MAX_FRAMES) {
            frameCount = MAX_FRAMES;
        }

        // Enter the write: bump seq to odd so any concurrent reader retries.
        const std::uint32_t seq = m_seq.load(std::memory_order_relaxed);
        m_seq.store(seq + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);

        std::memcpy(m_samples, frames, frameCount * CHANNELS * sizeof(std::int16_t));
        m_frameCount = frameCount;

        // Publish: bump seq back to even so readers can capture the new block.
        std::atomic_thread_fence(std::memory_order_release);
        m_seq.store(seq + 2, std::memory_order_relaxed);
    }

    // Consumer (main thread). Reads up to maxFrames of the latest published block into
    // out (which must hold at least maxFrames * CHANNELS floats), converting the stored
    // int16 samples to normalized float. Returns the number of frames copied, or 0 if
    // nothing has been published yet. Never takes a lock; retries on a torn read, so it
    // can spin briefly under contention but never blocks indefinitely.
    [[nodiscard]] std::size_t read(float *out, std::size_t maxFrames) const noexcept {
        while (true) {
            const std::uint32_t seq_before = m_seq.load(std::memory_order_acquire);
            if (seq_before & 1u) {
                // Write in progress; retry until it completes.
                continue;
            }

            // Clamp to both the caller's buffer and the compile-time capacity. m_frameCount is
            // read non-atomically and may momentarily tear to a bogus value mid-write, so the
            // conversion length must be bounded by a trusted constant — the sequence check below
            // discards a torn snapshot, but only AFTER the copy has already run.
            std::size_t count = m_frameCount;
            if (count > maxFrames) {
                count = maxFrames;
            }
            if (count > MAX_FRAMES) {
                count = MAX_FRAMES;
            }
            const std::size_t samples = count * CHANNELS;
            for (std::size_t i = 0; i < samples; ++i) {
                out[i] = static_cast<float>(m_samples[i]) * (1.0f / 32768.0f);
            }

            std::atomic_thread_fence(std::memory_order_acquire);
            const std::uint32_t seq_after = m_seq.load(std::memory_order_relaxed);
            if (seq_before == seq_after) {
                return count;
            }
            // The block changed under us: the copy may be torn, so discard and retry.
        }
    }
};


#endif //OSP2_AUDIO_TAP_H
