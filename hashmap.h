#include<list>
#include<stdexcept>
#include<vector>

template<class KeyType, class ValueType> class Iter;
template<class KeyType, class ValueType> class ConstIter;

// HashMap with separate chaining. https://en.wikipedia.org/wiki/Hash_table#Separate_chaining
// Operates using two containers:
// - the chain list, where chains are implemented with std::list<> and contain indices of data entries in main storage;
// - the main data storage, implemented with std::vector<std::pair<>>.
// Resize policy:
// - if the number of elements exceeds current capacity multiplied by kMaxLoadFactor,
// capacity is increased to double the original plus one;
// - if the capacity exceeds kMinScalingFactor times the number of elements, the size is reduced kScalingFactor-fold.

template<class KeyType, class ValueType, class Hash = std::hash<KeyType> >
class HashMap {
  public:
    typedef Iter<KeyType, ValueType> iterator;
    typedef ConstIter<KeyType, ValueType> const_iterator;

    HashMap(const Hash &hasher = Hash()) : hasher_(hasher) {}

    // Constructor from two iterators
    template<class Iterator>
    HashMap(Iterator first, Iterator last, const Hash &h = Hash()) {
        hasher_ = h;
        while (first != last) {
            insert(*first);
            ++first;
        };
    };

    // Constructor from initializer list
    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> init_list, const Hash &h = Hash()) {
        hasher_ = h;
        for (auto element : init_list) {
            insert(element);
        }
    };

    // O(1)
    iterator begin() {
        return iterator(hashmap_.begin());
    }

    // O(1)
    iterator end() {
        return iterator(hashmap_.end());
    }

    // O(1)
    const_iterator begin() const {
        return const_iterator(hashmap_.begin());
    }

    // O(1)
    const_iterator end() const {
        return const_iterator(hashmap_.end());
    }

    // Returns non-const iterator to key if it exists, end() otherwise.
    // Time complexity: amortized O(1).
    iterator find(KeyType key) {
        if (capacity_ == 0) {
            return end();
        }
        size_t candidate_position = GetBucketIndex(key);
        for (const auto& element : indices_[candidate_position]) {
            if (hashmap_[element].first == key) {
                return iterator(hashmap_.begin() + element);
            }
        }
        return end();
    }

    // Returns const iterator to key if it exists, end() otherwise.
    // Time complexity: amortized O(1), individual query expected O(1), provided the hash function is good enough.
    const_iterator find(KeyType key) const {
        if (capacity_ == 0) {
            return end();
        }
        size_t candidate_position = GetBucketIndex(key);
        for (const auto& element : indices_[candidate_position]) {
            if (hashmap_[element].first == key) {
                return const_iterator(hashmap_.begin() + element);
            }
        }
        return end();
    }

    // Empties the hashmap.
    // Time complexity: O(n), where n is the number of elements contained inside the hashmap.
    void clear() {
        capacity_ = 1;
        stored_elements_ = 0;
        hashmap_.clear();
        indices_.clear();
        indices_.resize(capacity_);
    }

    // Inserts an element in case there does not already exist one with the same key.
    // Returns an iterator to the inserted element (or the existing one).
    // Time complexity: amortized O(1), individual query O(n).
    iterator insert(std::pair<KeyType, ValueType> element) {
        auto key_position = find(element.first);
        if (key_position != end()) {
            return key_position;
        }
        UpscaleIfNecessary();
        ++stored_elements_;
        size_t pos = GetBucketIndex(element.first);
        indices_[pos].push_back(stored_elements_ - 1);
        hashmap_.push_back(element);
        return iterator(hashmap_.begin() + stored_elements_ - 1);
    }

    // Erases an element by key if there exists one, otherwise does nothing.
    // Time complexity: amortized O(1), individual query O(n).
    void erase(KeyType key) {
        iterator key_position = find(key);
        if (key_position == end()) {
            return;
        }
        EraseLastElement(SwapWithLastIfNecessary(key));
        --stored_elements_;
        DownscaleIfNecessary();
    }

    // Returns number of elements contained inside the hashmap. O(1)
    size_t size() const {
        return stored_elements_;
    }

    // Checks if the hashmap contains no elements. O(1)
    bool empty() const {
        return size() == 0;
    }

    // If there already exists an element with given key, returns a non-const iterator pointing to it.
    // Otherwise, inserts a new entry with this key and default value to the hashmap.
    // Time complexity: amortized O(1), individual query O(n).
    ValueType &operator[](KeyType key) {
        return insert({key, ValueType()})->second;
    }

    // If there exists an element with given key, returns a constant iterator pointing to it.
    // Otherwise, throws the std::out_of_range() exception.
    // Time complexity: amortized O(1), individual query expected O(1) provided the hash function is good enough.
    const ValueType &at(KeyType key) const {
        auto key_position = find(key);
        if (key_position == end()) {
            throw std::out_of_range("");
        }
        return key_position->second;
    }

    // O(1)
    Hash hash_function() const {
        return hasher_;
    }

  private:

    size_t capacity_ = 1, stored_elements_ = 0;
    // How many times the capacity of the map changes after rebuilding.
    static constexpr size_t kScalingFactor = 2;
    // The minimum discrepancies between hashmap capacity and number of stored elements for it to be rebuilt.
    static constexpr size_t kMinLoadFactor = 4;
    static constexpr size_t kMaxLoadFactor = 2;

    std::vector<std::list<size_t>> indices_ = std::vector<std::list<size_t>>(capacity_);
    std::vector<std::pair<KeyType, ValueType>> hashmap_;
    Hash hasher_;

    // Rebuilds and resizes hashmap according to its new capacity.
    void Rebuild() {
        std::vector<std::pair<KeyType, ValueType>> old_hashmap;
        old_hashmap = hashmap_;
        stored_elements_ = 0;
        hashmap_.clear();
        indices_.clear();
        indices_.resize(capacity_);
        for (auto element : old_hashmap) {
            insert(element);
        }
    }

    // Helper function. Erases an element if it is last in data storage.
    void EraseLastElement(std::list<size_t>::iterator iter) {
        size_t chain_storage_location = GetBucketIndex(LastKey());
        indices_[chain_storage_location].erase(iter);
        hashmap_.pop_back();
    }

    // Helper function. Swaps given element with last in data storage
    // Returns iterator pointing to last element in chain storage.
    std::list<size_t>::iterator SwapWithLastIfNecessary(const KeyType& key) {
        auto it_key = FindInChainList(key);
        if (LastKey() == key) {
            return it_key;
        }
        auto it_last = FindInChainList(LastKey());
        std::swap(*it_last, *it_key);
        std::swap(hashmap_[*it_key], hashmap_[*it_last]);
        return it_key;
    }

    // Returns a list iterator pointing to the position in the chain list
    // where the index of given key is recorded.
    // Time complexity: amortized O(1), expected O(1), provided the hash function is good enough.
    std::list<size_t>::iterator FindInChainList(const KeyType &key) {
        size_t chain_storage_location = GetBucketIndex(key);
        auto it = indices_[chain_storage_location].begin();
        while (*it >= stored_elements_ || hashmap_[*it].first != key) {
            ++it;
        }
        return it;
    }

    // Checks if capacity needs to be decreased, and in case it does, performs this.
    // Time complexity: O(1) amortized, O(n) individual query.
    void DownscaleIfNecessary() {
        if (stored_elements_ * kMinLoadFactor <= capacity_) {
            capacity_ /= kScalingFactor;
            Rebuild();
        }
    }

    // Checks if capacity needs to be increased, and in case it does, performs this.
    // Time complexity: O(1) amortized, O(n) individual query.
    void UpscaleIfNecessary() {
        if (stored_elements_ >= capacity_ * kMaxLoadFactor) {
            capacity_ *= kScalingFactor;
            Rebuild();
        }
    }

    // Returns index of chain, where given element could be located.
    // Time complexity: O(1).
    size_t GetBucketIndex(const KeyType& key) const {
        return hasher_(key) % capacity_;
    }

    // Returns constant reference to last key in the data storage.
    // Time complexity: O(1).
    const KeyType& LastKey() const {
        return hashmap_.back().first;
    }
};


// ForwardIterator for HashMap.
// Implemented with std::vector<> iterators.
// Allows to iterate over the hash map in linear time, accessing elements in some order.
template<class KeyType, class ValueType>
class Iter {
  public:
    Iter() = default;

    Iter(typename std::vector<std::pair<KeyType, ValueType>>::iterator iter) {
        iter_ = iter;
    }

    // O(1)
    std::pair<const KeyType, ValueType> operator*() {
        return *reinterpret_cast<std::pair<const KeyType, ValueType>*>(&*iter_);
    }

    // O(1)
    std::pair<const KeyType, ValueType> *operator->() {
        return reinterpret_cast<std::pair<const KeyType, ValueType>*>(&*iter_);
    }

    // O(1)
    bool operator==(Iter other) const {
        return iter_ == other.iter_;
    }

    // O(1)
    bool operator!=(Iter other) const {
        return iter_ != other.iter_;
    }

    // O(1)
    Iter operator++() {
        ++iter_;
        return *this;
    }

    // O(1)
    Iter operator++(int) {
        Iter copy_iter = *this;
        ++*this;
        return copy_iter;
    }

  private:
    typename std::vector<std::pair<KeyType, ValueType>>::iterator iter_;
};

// Constant version of the ForwardIterator for HashMap.
template<class KeyType, class ValueType>
class ConstIter {
  public:
    ConstIter() = default;

    ConstIter(typename std::vector<std::pair<KeyType, ValueType>>::const_iterator const_iter) {
        const_iter_ = const_iter;
    }

    // O(1)
    const std::pair<const KeyType, ValueType> operator*() const {
        return *reinterpret_cast<const std::pair<const KeyType, ValueType>*>(&(*const_iter_));
    }

    // O(1)
    const std::pair<const KeyType, ValueType> *operator->() const {
        return reinterpret_cast<const std::pair<const KeyType, ValueType>*>(&(*const_iter_));
    }

    // O(1)
    bool operator==(ConstIter other) const {
        return const_iter_ == other.const_iter_;
    }

    // O(1)
    bool operator!=(ConstIter other) const {
        return const_iter_ != other.const_iter_;
    }

    // O(1)
    ConstIter operator++() {
        ++const_iter_;
        return *this;
    }

    // O(1)
    ConstIter operator++(int) {
        ConstIter copy_const_iter = *this;
        ++*this;
        return copy_const_iter;
    }

  private:
    typename std::vector<std::pair<KeyType, ValueType>>::const_iterator const_iter_;
};
