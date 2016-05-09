#ifndef _WATERMELON_CORE_H_
#define _WATERMELON_CORE_H_

#include <cstddef>
#include <functional>
#include <atomic>
#include <iterator>
#include <memory>
#include <vector>

namespace wat {

const int DEFAULT_SIZE = 100;

template < class Key,
           class T,
           class Hash = std::hash<Key>,
           class Pred = std::equal_to<Key>,
           class Alloc = std::allocator< std::pair<const Key, T> >
         >
class unordered_map
{
  public:

    typedef Key key_type;
    typedef T mapped_type;
    typedef std::pair<const key_type, mapped_type> value_type;
    typedef Hash hasher;
    typedef Pred key_equal;
    typedef Alloc allocator_type;
    typedef typename Alloc::reference reference;
    typedef typename Alloc::const_reference const_reference;
    typedef typename Alloc::pointer pointer;
    typedef typename Alloc::const_pointer const_pointer;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    static const int EMPTY = 0;
    static const int FULL= 1;
    static const int DELETED = 2;
    struct bucket_info
    {
      bucket_info() : v(nullptr), state(EMPTY) {}
      bucket_info(bucket_info& b) : v(b.v), state(b.state.load()) {}
      bucket_info(std::shared_ptr<value_type>& v, int state)
        : v(v), state(state) {}
      std::shared_ptr<value_type> v;
      std::atomic<int> state;
    };
    typedef std::shared_ptr<bucket_info> bucket;
    typedef std::shared_ptr<std::vector<std::atomic<bucket>>> table;

    struct iterator : public std::forward_iterator_tag
    {
      iterator() : iterator(0, 0, NULL) {}
      iterator(const iterator &i) : iterator(i.index, i.buckets) {}
      iterator(size_type index, table buckets)
        : index(index), buckets(buckets) {}
      ~iterator() {}

      iterator& operator=(const iterator& i)
      {
        index = i.index;
        buckets = i.buckets;
        return *this;
      }

      bool operator==(const iterator& i) const
      {
        return (buckets == i.buckets) && (index == i.index);
      }

      bool operator!=(const iterator& i) const
      {
        return !(*this == i);
      }

      value_type& operator*()
      {
        return *(buckets->at(index).load().v);
      }
      const value_type& operator*() const
      {
        return *(buckets->at(index).load().v);
      }

      iterator& operator++()
      {
        index++;
        while (index < buckets->size())
        {
          bucket& b = buckets->at(index).load();
          if (b.state != FULL)
          {
            break;
          }
          index++;
        }
        return *this;
      }

      iterator operator++(int)
      {
        iterator ret(*this);
        operator++();
        return ret;
      }

      size_type index;
      table buckets;
    };

    struct const_iterator : public std::forward_iterator_tag
    {
      const_iterator() : const_iterator(0, 0, NULL) {}
      const_iterator(const const_iterator &i) :
        const_iterator(i.index, i.buckets) {}
      const_iterator(size_type index, table buckets)
        : index(index), buckets(buckets) {}
      ~const_iterator() {}

      const_iterator& operator=(const iterator& i)
      {
        index = i.index;
        buckets = i.buckets;
        return *this;
      }

      bool operator==(const const_iterator& i) const
      {
        return (buckets == i.buckets) && (index == i.index);
      }

      bool operator!=(const const_iterator& i) const
      {
        return !(*this == i);
      }

      const value_type& operator*() const
      {
        return *(buckets->at(index).load().v);
      }

      const_iterator& operator++()
      {
        index++;
        while (index < buckets->size())
        {
          bucket& b = buckets->at(index).load();
          if (b.state != FULL)
          {
            break;
          }
          index++;
        }
        return *this;
      }

      const_iterator operator++(int)
      {
        const_iterator ret(*this);
        operator++();
        return ret;
      }

      size_type index;
      table buckets;
    };

    // empty constructors
    explicit unordered_map(size_type n = DEFAULT_SIZE,
        const hasher& hf = hasher(),
        const key_equal& eql = key_equal(),
        const allocator_type& alloc = allocator_type())
      : bucket_count(n), element_count(0), residence(0),
        hf(hf), eql(eql), alloc(alloc)
    {
      buckets = std::make_shared<std::vector<std::atomic<bucket>>>(n);
      for (int i = 0; i < n; i++)
      {
        buckets->at(i) = std::make_shared<bucket_info>();
      }
    }
    explicit unordered_map(const allocator_type& alloc)
      : unordered_map(DEFAULT_SIZE, hasher(), key_equal(), alloc) {}

    // range constructor
    template <class InputIterator>
    unordered_map(InputIterator first, InputIterator last,
        size_type n = DEFAULT_SIZE,
        const hasher& hf = hasher(),
        const key_equal& eql = key_equal(),
        const allocator_type& alloc = allocator_type())
    : unordered_map(n, hf, eql, alloc)
    {
      insert(first, last);
    }

    // copy constructors
    unordered_map(const unordered_map& ump)
      : unordered_map(ump, allocator_type()) {}
    unordered_map(const unordered_map& ump, const allocator_type& alloc)
      : unordered_map(ump.bucket_count, ump.hf, ump.eql, alloc)
    {
      *this = ump;
    }

    // move constructors
    unordered_map(unordered_map&& ump)
      : unordered_map(std::forward<unordered_map&&>(ump), allocator_type()) {}
    unordered_map(unordered_map&& ump, const allocator_type& alloc)
      : unordered_map(ump.bucket_count, ump.hf, ump.eql, alloc)
    {
      operator=(std::forward<unordered_map&&>(ump));
    }
// initializer list
    unordered_map(std::initializer_list<value_type> il,
        size_type n = DEFAULT_SIZE,
        const hasher& hf = hasher(),
        const key_equal& eql = key_equal(),
        const allocator_type& alloc = allocator_type())
      : unordered_map(n, hf, eql, alloc)
    {
      *this = il;
    }

    // destructor
    ~unordered_map() { }

    // assignment
    unordered_map& operator=(const unordered_map& ump)
    {
      clear();
      insert(ump.begin(), ump.end());
      return *this;
    }
    unordered_map& operator=(unordered_map&& ump)
    {
      clear();
      std::swap(buckets, ump.buckets);
      std::swap(bucket_count, ump.bucket_count);
      std::swap(element_count, ump.element_count);
      std::swap(residence, ump.residence);
      return *this;
    }
    unordered_map& operator=(std::initializer_list<value_type> il)
    {
      clear();
      insert(il);
      return *this;
    }

    // capacity
    bool empty() const noexcept
    {
      return element_count == 0;
    }
    size_type size() const noexcept
    {
      return element_count;
    }
    size_type max_size() const noexcept
    {
      // TODO
      return -1;
    }

    iterator begin() noexcept
    {
      return iterator(0, buckets);
    }
    const_iterator begin() const noexcept
    {
      return cbegin();
    }
    iterator end() noexcept
    {
      return iterator(bucket_count, buckets);
    }
    const_iterator end() const noexcept
    {
      return cend();
    }
    const_iterator cbegin() const noexcept
    {
      return const_iterator(0, buckets);
    }
    const_iterator cend() const noexcept
    {
      return const_iterator(bucket_count, buckets);
    }

    // element access
    mapped_type& operator[](const key_type& k)
    {
      return emplace(k, value_type()).first->second;
    }
    mapped_type& operator[](key_type&& k)
    {
      return emplace(std::forward<key_type&&>(k), value_type()).first->second;
    }
    mapped_type& at(const key_type& k)
    {
      auto ret = findBucket(k);
      if (ret.second) {
        return ret.first->second;
      } else {
        throw std::out_of_range("wat::unordered_map::at");
      }
    }
    const mapped_type& at(const key_type& k) const
    {
      auto ret = findBucket(k);
      if (ret.second) {
        return ret.first->second;
      } else {
        throw std::out_of_range("wat::unordered_map::at");
      }
    }

    // element lookup
    iterator find(const key_type& k)
    {
      auto ret = findBucket(k);
      if (ret.second) {
        return ret.first;
      } else {
        return end();
      }
    }
    const_iterator find(const key_type& k) const
    {
      auto ret = findBucket(k);
      if (ret.second) {
        return const_iterator(ret.first);
      } else {
        return cend();
      }
    }
    size_type count(const key_type& k) const
    {
      return findBucket(k).second ? 1 : 0;
    }
    std::pair<iterator,iterator> equal_range(const key_type& k)
    {
      auto ret = findBucket(k);
      if (ret.second) {
        iterator second = ret.first;
        second++;
        return std::make_pair(ret.first, second);
      } else {
        return std::make_pair(end(), end());
      }
    }
    std::pair<const_iterator,const_iterator> equal_range(const key_type& k) const
    {
      auto ret = findBucket(k);
      if (ret.second) {
        const_iterator second = ret.first;
        second++;
        return std::make_pair(const_iterator(ret.first), second);
      } else {
        return std::make_pair(cend(), cend());
      }
    }

    // insertion
    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
      std::shared_ptr<value_type> v = std::allocate_shared(alloc, std::forward<Args>(args)...);
      return createBucket(v);
    }
    template <class... Args>
    iterator emplace_hint(const_iterator position, Args&&... args)
    {
      return emplace(std::forward<Args&&...>(args...));
    }
    std::pair<iterator,bool> insert(const value_type& val)
    {
      std::shared_ptr<value_type> v =
        std::allocate_shared(alloc, std::forward<const value_type&>(val));
      return createBucket(v);
    }
    template <class P>
      std::pair<iterator,bool> insert(P&& val)
    {
      std::shared_ptr<value_type> v =
        std::allocate_shared<value_type>(alloc, std::forward<P&&>(val));
      return createBucket(v);
    }
    iterator insert(const_iterator hint, const value_type& val)
    {
      return insert(val);
    }
    template <class P>
      iterator insert(const_iterator hint, P&& val)
    {
      return insert(std::forward<P&&>(val));
    }
    template <class InputIterator>
      void insert(InputIterator first, InputIterator last)
    {
      for (InputIterator i = first; i != last; i++)
      {
        insert(*i);
      }
    }
    void insert(std::initializer_list<value_type> il)
    {
      insert(il.begin(), il.end());
    }

    // deletion
    iterator erase(const_iterator position)
    {
      iterator ret(position);
      ret++;
      bucket& b = position.buckets.at(position.index).load();
      if (b.state == FULL && b.state.compare_exchange_strong(FULL, DELETED))
      {
        element_count--;
      }
      return ret;
    }
    size_type erase(const key_type& k)
    {
      return eraseBucket(k) ? 1 : 0;
    }
    iterator erase(const_iterator first, const_iterator last)
    {
      for (const_iterator i = first; i != last; i++)
      {
        erase(i);
      }
    }
    void clear() noexcept
    {
      // TODO:
    }

    // TODO: swap, equality

  private:
    /*
    typedef std::shared_ptr<value_type> bucket;
    typedef std::shared_ptr<std::vector<std::atomic<bucket>>> table;
    */

    std::pair<iterator, bool> createBucket(std::shared_ptr<value_type>& val)
    {
      table workingTable = buckets;
      size_t hashValue = hf(val->first);
      for (int i = 0; i < workingTable->size(); i++)
      {
        size_t index = (i + hashValue) % workingTable->size();
        bucket bucket = workingTable->at(index).load();

        // skip empty buckets
        if (bucket->state == DELETED)
        {
          continue;
        }
        int expected = EMPTY;
        if (bucket->state == EMPTY &&
            bucket->state.compare_exchange_strong(expected, FULL))
        {
          element_count++;
          residence++;
          return std::make_pair(iterator(index, workingTable), true);
        }
        if (hashValue == hf(bucket->v->second) &&
            eql(val->second, bucket->v->second)) {
          return std::make_pair(iterator(index, workingTable), false);
        }
      }
      return std::make_pair(end(), false);
    }
    std::pair<iterator, bool> findBucket(const key_type& val)
    {
      table workingTable = buckets;
      int hashValue = hf(val->first);
      for (int i = 0; i < workingTable->size(); i++)
      {
        int index = (i + hashValue) % workingTable->size();
        bucket bucket = (*workingTable)[index].load();

        // skip empty buckets
        if (bucket->state == DELETED)
        {
          continue;
        }
        if (bucket->state == EMPTY)
        {
          break;
        }
        if (hashValue == hf(bucket->second) && eql(val->second, bucket->second)) {
          return std::make_pair(iterator(index, workingTable), true);
        }
      }
      return std::make_pair(end(), false);
    }
    bool eraseBucket(const key_type& val)
    {
      table workingTable = buckets;
      int hashValue = hf(val->first);
      for (int i = 0; i < workingTable->size(); i++)
      {
        int index = (i + hashValue) % workingTable->size();
        bucket bucket = (*workingTable)[index].load();

        // skip empty buckets
        if (bucket->state == DELETED)
        {
          continue;
        }
        if (bucket->state == EMPTY)
        {
          return false;
        }
        if (hashValue == hf(bucket->second) && eql(val->second, bucket->second)) {
          int expected = FULL;
          if (bucket->state == FULL &&
              bucket->state.compare_exchange_strong(expected, DELETED))
          {
            element_count--;
            return std::make_pair(iterator(index, workingTable), true);
          }
        }
      }
      return false;
    }
    void resize();

    size_type bucket_count;
    std::atomic<size_type> element_count;
    std::atomic<size_type> residence;
    const hasher& hf;
    const key_equal& eql;
    const allocator_type& alloc;

    table buckets;
};

}

#endif /* _WATERMELON_CORE_H */

