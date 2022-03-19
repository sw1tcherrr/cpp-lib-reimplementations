#pragma once

template <typename T, typename Tag>
struct any_iterator;

namespace detail {
using diff_type = std::ptrdiff_t;

static int const SIZE = sizeof(void*);
static int const ALIGN = alignof(void*);

using aligned_storage = std::aligned_storage<SIZE, ALIGN>;

template <typename T>
constexpr bool fits_small_buf =
    sizeof(T) <= SIZE
    && ALIGN % alignof(T) == 0
    && std::is_nothrow_move_constructible_v<T>;

template <typename T>
concept dec = requires (T it) {
  {--it};
  {it--};
};

template <typename T>
concept add_and_sub = requires (T it, diff_type d) {
  {it += d};
  {it -= d};
  {it + d};
  {it - d};
};

template <typename T>
concept sub_it = requires (T a, T b) {
  {a - b} -> std::same_as<diff_type>;
};

template <typename T>
concept cmp = requires (T a, T b) {
  {a > b};
  {a < b};
};

template <typename T>
struct base_concept {
  virtual ~base_concept() = default;

  virtual void copy(bool, aligned_storage&) = 0;
  virtual void move(bool, aligned_storage&) = 0;

  virtual T* operator->() = 0;
  virtual T const* operator->() const = 0;
  virtual T& operator*() = 0;
  virtual T const& operator*() const = 0;

  virtual void operator++() = 0;
  virtual void operator++(int) = 0;

  virtual bool operator==(void*) = 0;

  // optional operators

  virtual void operator--() = 0;
  virtual void operator--(int) = 0;

  virtual void operator+=(diff_type) = 0;
  virtual void operator-=(diff_type) = 0;

  virtual diff_type operator-(void*) = 0;

  virtual bool operator<(void*) = 0;
  virtual bool operator>(void*) = 0;
};

template<typename T>
void set_dynamic(base_concept<T> const* obj, aligned_storage& buf) noexcept {
  reinterpret_cast<void const*&>(buf) = obj;
}

template <typename T, typename It>
struct model : base_concept<T> {
  using C = base_concept<T>;

  ~model() override = default;

  explicit model(It it) : iterator(std::move(it)) {}

  model(model const& other) : iterator(other.iterator) {}
  model(model&& other) : iterator(std::move(other.iterator)) {}

  void copy(bool small, aligned_storage& buf) override {
    if (small) {
      new (&buf) model(*this);
    } else {
      set_dynamic(static_cast<C*>(new model(*this)), buf);
    }
  }

  void move(bool small, aligned_storage& buf) override {
    if (small) {
      new (&buf) model(std::move(*this));
    } else {
      set_dynamic(static_cast<C*>(new model(std::move(*this))), buf);
    }
  }

  T* operator->() override {
    return iterator.operator->();
  }
  T const* operator->() const override {
    return iterator.operator->();
  }
  T& operator*() override {
    return iterator.operator*();
  }
  T const& operator*() const override {
    return iterator.operator*();
  }

  void operator++() override {
    ++iterator;
  }
  void operator++(int) override {
    iterator++;
  }

  bool operator==(void* other) override {
    return iterator == static_cast<model*>(other)->iterator;
  }

  void operator--() {
    if constexpr (dec<It>)
      --iterator;
  }

  void operator--(int) {
    if constexpr (dec<It>)
      iterator--;
  }

  void operator+=(diff_type d) {
    if constexpr (add_and_sub<It>)
      iterator += d;
  }

  void operator-=(diff_type d) {
    if constexpr (add_and_sub<It>)
      iterator -= d;
  }

  diff_type operator-(void* other) {
    if constexpr (sub_it<It>)
      return iterator - static_cast<model*>(other)->iterator;
    else
      return 0;
  }

  bool operator<(void* other) {
    if constexpr (cmp<It>)
      return iterator < static_cast<model*>(other)->iterator;
    else
      return false;
  }

  bool operator>(void* other) {
    if constexpr (cmp<It>)
      return iterator > static_cast<model*>(other)->iterator;
    else
      return false;
  }

private:
  It iterator;
};

template <typename T>
struct storage {
  using C = base_concept<T>;

  storage() = default;

  ~storage() {
    get()->~C();
  }

  storage(storage const& other) : small(other.small) {
    other.get()->copy(small, buf);
  }

  storage(storage&& other) : small(other.small) {
    other.get()->move(small, buf);
  }

  storage& operator=(storage&& other) {
    if (this != &other) {
      get()->~C();
      small = other.small;
      other.get()->move(small, buf);
    }

    return *this;
  }

  void swap(storage& other) {
    storage tmp = std::move(*this);
    *this = std::move(other);
    other = std::move(tmp);
  }

  template <typename It>
  explicit storage(It it) : small(fits_small_buf<model<T, It>>) {
    using M = model<T, It>;
    if constexpr (fits_small_buf<M>) {
      new (&buf) M(std::move(it));
    } else {
      set_dynamic(static_cast<C*>(new M(std::move(it))), buf);
    }
  }

  C* get() const noexcept {
    if (small) {
      return reinterpret_cast<C*>(&buf);
    } else {
      return static_cast<C*>(reinterpret_cast<void*&>(buf));
    }
  }

//  C const* get() const noexcept {
//    if (small) {
//      return reinterpret_cast<C const*>(&buf);
//    } else {
//      return static_cast<C const*>(reinterpret_cast<void* const&>(buf));
//    }
//  }

  mutable std::aligned_storage<SIZE, ALIGN> buf;
  bool small;
};

} // namespace detail


template <typename T, typename Tag>
struct any_iterator {
  using value_type = T;
  using pointer = T*;
  using reference = T&;
  using difference_type = detail::diff_type;
  using iterator_category = Tag;

  any_iterator() = default;

  template <typename It>
  any_iterator(It iterator) : storage(std::move(iterator)) {}

  any_iterator(any_iterator const& other) : storage(other.storage) {}
  any_iterator(any_iterator&& other) : storage(std::move(other.storage)) {}

  any_iterator& operator=(any_iterator&& other) {
    if (this != &other) {
      auto tmp(std::move(other));
      swap(tmp);
    }

    return *this;
  }
  any_iterator& operator=(any_iterator const& other) {
    if (this != &other) {
      auto tmp = other;
      swap(tmp);
    }

    return *this;
  }

  void swap(any_iterator& other) noexcept {
    storage.swap(other.storage);
  }

  template<typename It>
  any_iterator& operator=(It it) {
    any_iterator tmp(it);
    swap(tmp);
  }

  T const& operator*() const {
    return storage.get()->operator*();
  }
  T& operator*() {
    return storage.get()->operator*();
  }

  T const* operator->() const {
    return storage.get()->operator->();
  }
  T* operator->() {
    return storage.get()->operator->();
  }

  any_iterator& operator++() & {
    storage.get()->operator++();
    return *this;
  }
  any_iterator operator++(int) & {
    auto tmp(*this);
    storage.get()->operator++(0);
    return tmp;
  }

  template<typename TT, typename TTag>
  friend bool operator==(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b);

  template<typename TT, typename TTag>
  friend bool operator!=(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b);

  // operators ONLY for appropriate iterator tags

  template<typename TT, typename TTag>
  friend any_iterator<TT, TTag>& operator--(any_iterator<TT, TTag>&)
      requires std::is_base_of_v<std::bidirectional_iterator_tag, TTag>;

  template<typename TT, typename TTag>
  friend any_iterator<TT, TTag> operator--(any_iterator<TT, TTag>&, int)
      requires std::is_base_of_v<std::bidirectional_iterator_tag, TTag>;

  template<typename TT, typename TTag>
  friend any_iterator<TT, TTag>& operator+=(any_iterator<TT, TTag>&, typename any_iterator<TT, TTag>::difference_type)
      requires std::is_base_of_v<std::random_access_iterator_tag, TTag>;

  template<typename TT, typename TTag>
  friend any_iterator<TT, TTag>& operator-=(any_iterator<TT, TTag>&, typename any_iterator<TT, TTag>::difference_type)
      requires std::is_base_of_v<std::random_access_iterator_tag, TTag>;

  template<typename TT, typename TTag>
  friend any_iterator<TT, TTag> operator+(any_iterator<TT, TTag>, typename any_iterator<TT, TTag>::difference_type)
      requires std::is_base_of_v<std::random_access_iterator_tag, TTag>;

  template<typename TT, typename TTag>
  friend any_iterator<TT, TTag> operator-(any_iterator<TT, TTag>, typename any_iterator<TT, TTag>::difference_type)
      requires std::is_base_of_v<std::random_access_iterator_tag, TTag>;

  template<typename TT, typename TTag>
  friend bool operator<(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b)
      requires std::is_base_of_v<std::random_access_iterator_tag, TTag>;

  template<typename TT, typename TTag>
  friend bool operator>(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b)
  requires std::is_base_of_v<std::random_access_iterator_tag, TTag>;

  template<typename TT, typename TTag>
  friend typename any_iterator<TT, TTag>::difference_type operator-(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b)
      requires std::is_base_of_v<std::random_access_iterator_tag, TTag>;

  ~any_iterator() = default;

private:
  detail::storage<T> storage;
};

template<typename TT, typename TTag>
bool operator==(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b) {
  return a.storage.get()->operator==(b.storage.get());
}

template<typename TT, typename TTag>
bool operator!=(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b) {
  return !(a == b);
}

template<typename TT, typename TTag>
any_iterator<TT, TTag>& operator--(any_iterator<TT, TTag>& it)
requires std::is_base_of_v<std::bidirectional_iterator_tag, TTag>
{
  it.storage.get()->operator--();
  return it;
}

template<typename TT, typename TTag>
any_iterator<TT, TTag> operator--(any_iterator<TT, TTag>& it, int)
requires std::is_base_of_v<std::bidirectional_iterator_tag, TTag>
{
  auto tmp(it);
  it.storage.get()->operator--(0);
  return tmp;
}

template<typename TT, typename TTag>
any_iterator<TT, TTag>& operator+=(any_iterator<TT, TTag>& it, typename any_iterator<TT, TTag>::difference_type d)
requires std::is_base_of_v<std::random_access_iterator_tag, TTag>
{
  it.storage.get()->operator+=(d);
  return it;
}

template<typename TT, typename TTag>
any_iterator<TT, TTag>& operator-=(any_iterator<TT, TTag>& it, typename any_iterator<TT, TTag>::difference_type d)
requires std::is_base_of_v<std::random_access_iterator_tag, TTag>
{
  it.storage.get()->operator-=(d);
  return it;
}

template<typename TT, typename TTag>
any_iterator<TT, TTag> operator+(any_iterator<TT, TTag> it, typename any_iterator<TT, TTag>::difference_type d)
requires std::is_base_of_v<std::random_access_iterator_tag, TTag>
{
  return it += d;
}

template<typename TT, typename TTag>
any_iterator<TT, TTag> operator-(any_iterator<TT, TTag> it, typename any_iterator<TT, TTag>::difference_type d)
requires std::is_base_of_v<std::random_access_iterator_tag, TTag>
{
  return it -= d;
}

template<typename TT, typename TTag>
bool operator<(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b)
requires std::is_base_of_v<std::random_access_iterator_tag, TTag>
{
  return a.storage.get()->operator<(b.storage.get());
}

template<typename TT, typename TTag>
bool operator>(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b)
requires std::is_base_of_v<std::random_access_iterator_tag, TTag>
{
  return a.storage.get()->operator>(b.storage.get());
}

template<typename TT, typename TTag>
typename any_iterator<TT, TTag>::difference_type operator-(any_iterator<TT, TTag> const& a, any_iterator<TT, TTag> const& b)
requires std::is_base_of_v<std::random_access_iterator_tag, TTag>
{
  return a.storage.get()->operator-(b.storage.get());
}
