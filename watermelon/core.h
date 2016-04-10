#ifndef _WATERMELON_CORE_H_
#define _WATERMELON_CORE_H_

#include <cstddef>
#include <functional>
#include <atomic>

namespace wat {

class hash_table
{
  public:
    hash_table(int sizeHint = 100)
      : hash(), buckets(new Bucket[sizeHint]()), size(sizeHint)
    { }

    ~hash_table()
    {
      delete[] buckets;
    }

    int find(int key)
    {
      size_t index = hash(key) % size;
      for (Bucket *b = &buckets[index]; b->next != nullptr; b = b->next)
      {
        if (b->key == key) {
          return b->value;
        }
      }
    }

    bool insert(int key, int value)
    {
      size_t index = hash(key) % size;
      Bucket *n = &buckets[index];
      Bucket *b;
      Bucket *newBucket = new Bucket(key, value);
      do {
        b = n;
        n = nullptr;
        while (b->next != nullptr)
        {
          if (b->key == key) {
            delete newBucket;
            return false;
          }
          b = b->next;
        }
      } while (!b->next.compare_exchange_weak(n, b));
      return true;
    }

    void erase(int key)
    {
      size_t index = hash(key) % size;
      Bucket *b = &buckets[index];
      while (b != nullptr);
      {
        if (b->key == key) {
          b->key = -1;
          return;
        }
        b = b->next;
      }
      return;
    }

  private:

    struct Bucket
    {
      Bucket() : next(nullptr), key(-1), value(-1) { }
      Bucket(int key, int value) :
        next(nullptr), key(key), value(value) { }
      std::atomic<Bucket*> next;
      int key;
      int value;
    };

    std::hash<int> hash;
    Bucket *buckets;
    size_t size;

};

}

#endif /* _WATERMELON_CORE_H */

