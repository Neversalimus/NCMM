#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ncmm_graphics_scratch {
// Main-thread snapshot storage only. A loan owns its vector for the entire
// callback; only empty capacity returns to the pool. Nested callbacks therefore
// cannot invalidate an outer callback's array. A world reset also retires loans
// that were active when the reset happened.
template<class T, std::size_t MaximumRetainedElements>
class vector_storage {
    std::vector<T> retained_;
    std::uint64_t generation_ = 0;
public:
    vector_storage() = default;
    vector_storage(const vector_storage &) = delete;
    vector_storage &operator=(const vector_storage &) = delete;

    class loan {
        vector_storage &owner_;
        const std::uint64_t generation_;
        std::vector<T> values_;
        std::size_t growths_ = 0;
        const bool reused_;
        void note_growth(std::size_t before) noexcept {
            growths_ += values_.capacity() != before;
        }
    public:
        explicit loan(vector_storage &owner) noexcept : owner_(owner),
            generation_(owner.generation_), reused_(owner.retained_.capacity() != 0) {
            values_.swap(owner.retained_);
        }
        loan(const loan &) = delete;
        loan &operator=(const loan &) = delete;
        ~loan() noexcept {
            values_.clear();
            if(generation_ == owner_.generation_ &&
               values_.capacity() <= MaximumRetainedElements &&
               values_.capacity() > owner_.retained_.capacity()) {
                values_.swap(owner_.retained_);
            }
        }
        std::vector<T> &get() noexcept { return values_; }
        void reserve(std::size_t count) {
            const auto before = values_.capacity();
            values_.reserve(count);
            note_growth(before);
        }
        void resize(std::size_t count) {
            const auto before = values_.capacity();
            values_.resize(count);
            note_growth(before);
        }
        void push_back(const T &value) {
            const auto before = values_.capacity();
            values_.push_back(value);
            note_growth(before);
        }
        std::size_t growths() const noexcept { return growths_; }
        bool reused() const noexcept { return reused_; }
        std::size_t bytes() const noexcept { return values_.capacity() * sizeof(T); }
    };

    loan acquire() noexcept { return loan(*this); }
    std::size_t bytes() const noexcept { return retained_.capacity() * sizeof(T); }
    void clear() noexcept {
        ++generation_;
        std::vector<T>().swap(retained_);
    }
};
}
