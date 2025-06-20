#include <cstddef>
#include <type_traits>
#include <utility>
#include <iterator>
#include <algorithm>
#include <ranges>
#include <stdexcept>

// Basic Allocator
template <typename T>
class DefaultAllocator {
public:
    using ValueType = T;
    using Pointer = T*;
    using ConstPointer = const T*;
    using AllocatorType = DefaultAllocator<T>;
};

// Basic AllocatorTraits
template <typename T>
class AllocatorHelper {
public:
    static void allocate(T*& p, const DefaultAllocator<T>&, const size_t n) {
        p = static_cast<T*>(::operator new(n * sizeof(T)));
    }
    static void construct(T* p, const DefaultAllocator<T>&, const T& value) {
        ::new (static_cast<void*>(p)) T(value);
    }
    template <typename... Args>
    static void construct(T* p, const DefaultAllocator<T>&, Args&&... args) {
        ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
    }
    static void destroy(T* p, const DefaultAllocator<T>&) {
        p->~T();
    }
    static void deallocate(T* p, const DefaultAllocator<T>&) {
        ::operator delete(p);
    }
};

// Reverse Iterator Placeholder
template <typename T>
class ReverseIteratorStub {
public:
    ReverseIteratorStub() {}
    ReverseIteratorStub(T* ptr, size_t = 0) {}
};

// Vector Implementation
template <class T, class Allocator = DefaultAllocator<T>>
class Vector {
private:
    T* buffer_;
    size_t size_;
    size_t capacity_;
    Allocator allocator_;

    template <typename Input, typename Strategy>
    void allocate_with_strategy(Strategy&& strategy, const Input& input, const size_t count, const Allocator& allocator = Allocator()) {
        if (count == 0) {
            buffer_ = nullptr;
            size_ = 0;
            capacity_ = 0;
            allocator_ = allocator;
            return;
        }

        size_t new_size = count;
        size_t new_capacity = (new_size >= 4) ? new_size * 2 : new_size;
        strategy(new_size, allocator, input, new_capacity);
    }

    template <typename Iterator, typename Fun>
    void range_with_check(Fun&& fun, const Iterator first, const Iterator last) {
        size_t length = std::distance(first, last);

        if (length == 0) {
            return; // Allow empty ranges
        }

        if (length > capacity_) {
            reserve((capacity_ == 0) ? length : std::max(capacity_ * 2, length));
        }

        fun(first, last);
    }

    template <std::ranges::range R>
    void insert_range(R&& r, bool clear_before) {
        if (clear_before) clear();
        auto first = std::ranges::begin(r);
        auto last = std::ranges::end(r);

        range_with_check(
            [this](auto start, auto end) {
                while (start != end) {
                    emplace_back(*start);
                    ++start;
                }
            },
            first, last
        );
    }

public:
    using ValueType = T;
    using Iterator = T*;
    using ConstIterator = const T*;
    using ReverseIterator = ReverseIteratorStub<Iterator>;
    using ConstReverseIterator = ReverseIteratorStub<const Iterator>;
    using SizeType = size_t;
    using DifferenceType = ptrdiff_t;
    using AllocatorType = typename Allocator::AllocatorType;
    using Pointer = typename Allocator::Pointer;
    using ConstPointer = typename Allocator::ConstPointer;

    Vector() noexcept(std::is_nothrow_default_constructible<Allocator>::value)
        : buffer_(nullptr), size_(0), capacity_(0), allocator_(Allocator()) {
    }

    Vector(size_t n, const T& value, const Allocator& alloc = Allocator()) {
        allocate_with_strategy<T>([this](size_t n, const Allocator& a, const T& v, size_t c) {
            AllocatorHelper<T>::allocate(buffer_, a, c);
            for (size_t i = 0; i < n; ++i)
                AllocatorHelper<T>::construct(&buffer_[i], a, v);
            size_ = n;
            capacity_ = c;
            allocator_ = a;
            }, value, n, alloc);
    }

    Vector(const Vector& other)
        : buffer_(nullptr), size_(0), capacity_(0), allocator_(other.allocator_) {
        if (other.size_ > 0) {
            reserve(other.capacity_);
            for (size_t i = 0; i < other.size_; ++i) {
                AllocatorHelper<T>::construct(&buffer_[i], allocator_, other.buffer_[i]);
            }
            size_ = other.size_;
        }
    }

    Vector(Vector&& other) noexcept
        : buffer_(other.buffer_), size_(other.size_), capacity_(other.capacity_), allocator_(std::move(other.allocator_)) {
        other.buffer_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    Vector& operator=(const Vector& other) {
        if (this != &other) {
            clear();
            if (other.size_ > 0) {
                reserve(other.capacity_);
                for (size_t i = 0; i < other.size_; ++i) {
                    AllocatorHelper<T>::construct(&buffer_[i], allocator_, other.buffer_[i]);
                }
                size_ = other.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            clear();
            buffer_ = other.buffer_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            allocator_ = std::move(other.allocator_);
            other.buffer_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    bool operator==(const Vector& other) const {
        if (size_ != other.size_) return false;
        for (SizeType i = 0; i < size_; ++i) {
            if (!(buffer_[i] == other.buffer_[i])) return false;
        }
        return true;
    }

    auto operator<=>(const Vector& other) const {
        return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
    }

    SizeType size() const noexcept { return size_; }
    SizeType capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }

    T* data() const noexcept { return buffer_; }

    Iterator begin() noexcept { return buffer_; }
    ConstIterator begin() const noexcept { return buffer_; }
    ConstIterator cbegin() const noexcept { return buffer_; }

    Iterator end() noexcept { return buffer_ + size_; }
    ConstIterator end() const noexcept { return buffer_ + size_; }
    ConstIterator cend() const noexcept { return buffer_ + size_; }

    T& operator[](SizeType index) { return buffer_[index]; }
    const T& operator[](SizeType index) const { return buffer_[index]; }

    void insert_at(SizeType index, const T& value) {
        if (index > size_) {
            throw std::out_of_range("Insert index out of range");
        }
        if (size_ == capacity_) {
            reserve((capacity_ == 0) ? 1 : capacity_ * 2);
        }

        // Move elements to make space
        for (SizeType i = size_; i > index; --i) {
            if (i <= size_) { // Only construct if we haven't already
                AllocatorHelper<T>::construct(&buffer_[i], allocator_, std::move(buffer_[i - 1]));
                AllocatorHelper<T>::destroy(&buffer_[i - 1], allocator_);
            }
        }

        AllocatorHelper<T>::construct(&buffer_[index], allocator_, value);
        ++size_;
    }

    void assign(SizeType count, const T& value) {
        clear();
        if (count > capacity_) {
            reserve(count);
        }
        for (SizeType i = 0; i < count; ++i) {
            AllocatorHelper<T>::construct(&buffer_[i], allocator_, value);
        }
        size_ = count;
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            reserve((capacity_ == 0) ? 1 : capacity_ * 2);
        }
        AllocatorHelper<T>::construct(&buffer_[size_], allocator_, std::forward<Args>(args)...);
        ++size_;
    }

    void reserve(SizeType new_capacity) {
        if (new_capacity <= capacity_) return;

        T* new_buffer = nullptr;
        AllocatorHelper<T>::allocate(new_buffer, allocator_, new_capacity);

        for (SizeType i = 0; i < size_; ++i) {
            AllocatorHelper<T>::construct(&new_buffer[i], allocator_, std::move(buffer_[i]));
            AllocatorHelper<T>::destroy(&buffer_[i], allocator_);
        }

        if (buffer_) {
            AllocatorHelper<T>::deallocate(buffer_, allocator_);
        }

        buffer_ = new_buffer;
        capacity_ = new_capacity;
    }

    template <typename... Args>
    Iterator emplace_at(ConstIterator pos, Args&&... args) {
        size_t index = std::distance(cbegin(), pos);

        if (index > size_)
            throw std::out_of_range("Insert position is out of range");

        if (size_ == capacity_) {
            size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;
            reserve(new_capacity);
        }

        // Move elements to make space
        for (size_t i = size_; i > index; --i) {
            AllocatorHelper<T>::construct(&buffer_[i], allocator_, std::move(buffer_[i - 1]));
            AllocatorHelper<T>::destroy(&buffer_[i - 1], allocator_);
        }

        AllocatorHelper<T>::construct(&buffer_[index], allocator_, std::forward<Args>(args)...);
        ++size_;

        return buffer_ + index;
    }

    Allocator get_allocator() const {
        return allocator_;
    }

    void clear() {
        for (size_t i = 0; i < size_; ++i) {
            AllocatorHelper<T>::destroy(&buffer_[i], allocator_);
        }
        size_ = 0;
        // Don't deallocate buffer or reset capacity - just clear contents
    }

    void swap(Vector& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        std::swap(allocator_, other.allocator_);
    }

    ~Vector() {
        for (size_t i = 0; i < size_; ++i) {
            AllocatorHelper<T>::destroy(&buffer_[i], allocator_);
        }
        if (buffer_) {
            AllocatorHelper<T>::deallocate(buffer_, allocator_);
        }
    }

    template <std::ranges::range R>
    void assign_range(R&& r) {
        insert_range(std::forward<R>(r), true);
    }

    template <std::ranges::range R>
    void append_range(R&& r) {
        insert_range(std::forward<R>(r), false);
    }

    void erase(ConstIterator pos) {
        if (pos < cbegin() || pos >= cend()) {
            throw std::out_of_range("Iterator out of range");
        }

        SizeType index = std::distance(cbegin(), pos);
        AllocatorHelper<T>::destroy(&buffer_[index], allocator_);

        // Move elements down
        for (size_t i = index; i < size_ - 1; ++i) {
            AllocatorHelper<T>::construct(&buffer_[i], allocator_, std::move(buffer_[i + 1]));
            AllocatorHelper<T>::destroy(&buffer_[i + 1], allocator_);
        }

        --size_;
    }

    void erase(ConstIterator first, ConstIterator last) {
        if (first < cbegin() || last > cend() || first > last) {
            throw std::out_of_range("Invalid erase range");
        }

        size_t start = std::distance(cbegin(), first);
        size_t end = std::distance(cbegin(), last);
        size_t count = end - start;

        // Destroy elements in range
        for (size_t i = start; i < end; ++i) {
            AllocatorHelper<T>::destroy(&buffer_[i], allocator_);
        }

        // Move elements after `last` to `first`
        for (size_t i = end; i < size_; ++i) {
            AllocatorHelper<T>::construct(&buffer_[i - count], allocator_, std::move(buffer_[i]));
            AllocatorHelper<T>::destroy(&buffer_[i], allocator_);
        }

        size_ -= count;
    }

    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    void pop_back() {
        if (size_ == 0) {
            throw std::runtime_error("Cannot pop from empty vector");
        }
        AllocatorHelper<T>::destroy(&buffer_[size_ - 1], allocator_);
        --size_;
    }

    T& back() {
        if (size_ == 0) {
            throw std::runtime_error("Vector is empty");
        }
        return buffer_[size_ - 1];
    }

    const T& back() const {
        if (size_ == 0) {
            throw std::runtime_error("Vector is empty");
        }
        return buffer_[size_ - 1];
    }

    T& front() {
        if (size_ == 0) {
            throw std::runtime_error("Vector is empty");
        }
        return buffer_[0];
    }

    const T& front() const {
        if (size_ == 0) {
            throw std::runtime_error("Vector is empty");
        }
        return buffer_[0];
    }
};

#if __cplusplus < 202002L

// Transform a range to container
template <typename Container, std::ranges::input_range Range, typename... Args>
    requires (!std::ranges::view<Range>)
constexpr Container to(Range&& range, Args&&... args) {
    Container container(std::forward<Args>(args)...);
    for (auto&& element : range)
        container.emplace_back(std::forward<decltype(element)>(element));
    return container;
}

// Basic range adapter for vector
namespace vector_adapters {
    template <std::ranges::range R, typename T = std::ranges::range_value_t<R>>
    auto to_vector(R&& r) -> Vector<T> {
        return to<Vector<T>>(std::forward<R>(r));
    }

    template <std::ranges::range R, typename Func>
    auto transform_to_vector(R&& r, Func&& func) {
        auto transformed = r | std::views::transform(std::forward<Func>(func));
        using T = std::remove_cvref_t<decltype(*std::ranges::begin(transformed))>;
        return to<Vector<T>>(transformed);
    }

    template <std::ranges::range R, typename Pred>
    auto filter_to_vector(R&& r, Pred&& pred) {
        auto filtered = r | std::views::filter(std::forward<Pred>(pred));
        using T = std::remove_cvref_t<decltype(*std::ranges::begin(filtered))>;
        return to<Vector<T>>(filtered);
    }

    template <std::ranges::range R>
    auto take_to_vector(R&& r, std::size_t count) {
        auto taken = r | std::views::take(count);
        using T = std::remove_cvref_t<decltype(*std::ranges::begin(taken))>;
        return to<Vector<T>>(taken);
    }

    template <std::ranges::range R>
    auto drop_to_vector(R&& r, std::size_t count) {
        auto dropped = r | std::views::drop(count);
        using T = std::remove_cvref_t<decltype(*std::ranges::begin(dropped))>;
        return to<Vector<T>>(dropped);
    }

    template <std::ranges::bidirectional_range R>
    auto reverse_to_vector(R&& r) {
        auto reversed = r | std::views::reverse;
        using T = std::remove_cvref_t<decltype(*std::ranges::begin(reversed))>;
        return to<Vector<T>>(reversed);
    }

    template <std::ranges::range R1, std::ranges::range R2>
    auto zip_to_vector(R1&& r1, R2&& r2) {
        auto zipped = std::views::zip(r1, r2);
        using T = std::remove_cvref_t<decltype(*std::ranges::begin(zipped))>;
        return to<Vector<T>>(zipped);
    }

    template <std::ranges::range R>
    auto chunk_to_vector(R&& r, std::size_t chunk_size) {
        auto chunked = std::views::chunk(r, chunk_size);
        using T = std::remove_cvref_t<decltype(*std::ranges::begin(chunked))>;
        return to<Vector<T>>(chunked);
    }

    template <std::ranges::range R>
    auto enumerate_to_vector(R&& r) {
        auto enumerated = std::views::enumerate(r);
        using T = std::remove_cvref_t<decltype(*std::ranges::begin(enumerated))>;
        return to<Vector<T>>(enumerated);
    }
}

#endif

int main() {
    // Example usage
    Vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    Vector<int> v2(5, 42);

    return 0;
}