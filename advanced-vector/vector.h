#pragma once 
#include <algorithm> 
#include <cassert> 
#include <cstdlib> 
#include <iterator> 
#include <new> 
#include <utility> 
#include <memory> 

template <typename T>
class RawMemory {
public:

    RawMemory() = default;

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
        :buffer_(std::move(other.buffer_)), capacity_(std::move(other.capacity_))
    {
        other.buffer_ = nullptr;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё 
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate 
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    explicit Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    explicit Vector(Vector&& other) noexcept
        :data_(std::move(other.data_)), size_(std::move(other.size_))
    {}

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }
    const_iterator begin() const noexcept {
        return cbegin();
    }
    const_iterator end() const noexcept {
        return cend();
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                size_t copy_count = std::min(rhs.Size(), size_);
                std::copy_n(rhs.begin(), copy_count, data_.GetAddress());
                if (copy_count < size_) {
                    DestroyN(data_ + copy_count, size_ - copy_count);
                }
                else if (copy_count < rhs.Size()) {
                    std::uninitialized_copy_n(&rhs[copy_count], rhs.Size() - copy_count, data_ + copy_count);
                }
                size_ = rhs.Size();
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = std::move(rhs.size_);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    static constexpr bool IsTHaveMoveCon() {
        return std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (IsTHaveMoveCon()) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        DestroyN(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size <= size_) {
            DestroyN(data_ + new_size + 1, size_ - new_size);
        }
        else {
            if (new_size > data_.Capacity()) {
                Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }

    template <typename... Args> 
    iterator Emplace(const_iterator pos, Args&&... args) { 
        size_t distance_to_pos = std::distance(cbegin(), pos); 
        iterator new_element_ptr = nullptr;
        if (size_ == data_.Capacity()) { 
            new_element_ptr = AddWithReallocation(distance_to_pos, std::forward<Args>(args)...); 
        } 
        else { 
            new_element_ptr = AddWithoutReallocation(pos, distance_to_pos, std::forward<Args>(args)...); 
        } 
        size_++; 
        return new_element_ptr; 
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(cend(), std::forward<Args>(args)...);
    }

    /*noexcept(std::is_nothrow_move_assignable_v<T>)*/
    iterator Erase(const_iterator pos) {
        size_t distance_to_pos = std::distance(cbegin(), pos);
        Destroy(data_ + distance_to_pos);
        std::move(data_ + distance_to_pos + 1, data_ + size_, data_ + distance_to_pos);
        --size_;
        return data_ + distance_to_pos;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    void PopBack() noexcept {
        --size_;
        Destroy(data_ + size_);
    }

    ~Vector() {
        if (data_.GetAddress() != nullptr) {
            DestroyN(data_.GetAddress(), size_);
        }
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    static void DestroyN(T* buf, size_t n) noexcept {
        std::destroy_n(buf, n);
    }

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

    template <typename... Args>
    iterator AddWithReallocation(size_t distance_to_pos, Args&&... args) {
        size_t new_size = (size_ == 0) ? 1 : 2 * size_;
        RawMemory<T> new_data(new_size);
        T* new_element_ptr = new (new_data + distance_to_pos) T{ std::forward<Args>(args)... };
        if constexpr (IsTHaveMoveCon()) {
            std::uninitialized_move_n(data_.GetAddress(), distance_to_pos, new_data.GetAddress());
            std::uninitialized_move_n(data_ + distance_to_pos, size_ - distance_to_pos, new_data + distance_to_pos + 1);
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), distance_to_pos, new_data.GetAddress());
            std::uninitialized_copy_n(data_ + distance_to_pos, size_ - distance_to_pos, new_data + distance_to_pos + 1);
        }
        DestroyN(data_.GetAddress(), size_);
        data_.Swap(new_data);
        return new_element_ptr;
    }

    template <typename... Args>
    iterator AddWithoutReallocation(const_iterator pos, size_t distance_to_pos, Args&&... args) {
        iterator new_element_ptr = nullptr;
        if (pos == end()) {
            new_element_ptr = new (data_ + distance_to_pos) T{ std::forward<Args>(args)... };
        }
        else {
            new (end()) T{ std::forward <T>(*(end() - 1)) };
            T temp{ std::forward<Args>(args)... };
            std::move_backward(begin() + distance_to_pos, end() - 1, end());
            data_[distance_to_pos] = std::move(temp);
            new_element_ptr = &data_[distance_to_pos];
        }
        return new_element_ptr;
    }

};