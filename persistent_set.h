#include <cassert>  // assert
#include <iterator> // std::reverse_iterator
#include <utility>  // std::pair, std::swap

template <typename T>
struct persistent_set {
public:
  struct iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;

  persistent_set()  = default;
  persistent_set(persistent_set const& other)  : root(other.root) {}
  persistent_set(persistent_set&& other)  : root(std::move(other.root)) {}

  persistent_set& operator=(persistent_set const& other)  {
    if (this != &other) {
      root = other.root;
    }
    return *this;
  }
  persistent_set& operator=(persistent_set&& other)  {
    if (this != &other) {
      root = std::move(other.root);
    }
    return *this;
  }

  ~persistent_set() = default;

  size_t size() const  {
    return size_;
  }

  void clear()  {
    root.left.reset();
  }

  bool empty() const  {
    return !root.left && !root.right;
  }

  iterator begin() const  {
    return make_iterator(node_t::min_in_subtree(&root));
  }
  iterator end() const  {
    return make_iterator(&root);
  }

  reverse_iterator rbegin() const  {
    return reverse_iterator(end());
  }
  reverse_iterator rend() const  {
    return reverse_iterator(begin());
  }

  std::pair<iterator, bool> insert(T const& key) {
    if (find(key) != end()) {
      return {end(), false};
    }

    node_ptr_t new_node = std::make_shared<val_node_t>(key); // ok to throw

    copy_path(key, new_node, nullptr, nullptr);
    ++size_;
    return {make_iterator(new_node.get()), true};
  }

  iterator erase(iterator it);

  iterator find(T const& key)  {
    node_t* cur = root.left_ptr();
    for (;;) {
      if (cur == nullptr) {
        return end();
      }

      if (get_value(cur) < key) {
        cur = cur->right_ptr();
      } else if (key < get_value(cur)) {
        cur = cur->left_ptr();
      } else {
        return make_iterator(cur);
      }
    }
  }

  iterator lower_bound(T const& key)  {
    node_t* res = &root;
    node_t* cur = root.left_ptr();
    while (cur != nullptr) {
      if (get_value(cur) < key) {
        cur = cur->right_ptr();
      } else {
        res = cur;
        cur = cur->left_ptr();
      }
    }
    
    return make_iterator(res);
  }

  iterator upper_bound(T const& key)  {
    node_t* res = &root;
    node_t* cur = root.left_ptr();
    while(cur != nullptr) {
      if (key < get_value(cur)) {
        res = cur;
        cur = cur->left_ptr();
      } else {
        cur = cur->right_ptr();
      }
    }
    
    return make_iterator(res);
  }

  void swap(persistent_set& other)  {
    std::swap(root, other.root);
  }

  friend void swap(persistent_set& a, persistent_set& b)  {
    a.swap(b);
  }

private:
  struct node_t;
  struct val_node_t;
  using node_ptr_t = std::shared_ptr<val_node_t>;

  mutable node_t root;
  size_t size_{0};

  static T& get_value(node_t* node) {
    return static_cast<val_node_t*>(node)->value;
  }

  iterator make_iterator(node_t* ptr) const {
    return iterator(ptr, &root);
  }

  // copy path to node with value = key
  // if found replace with replace
  // else insert replace
  // link left_end and right_end at the end of copied path
  void copy_path(T const& key, node_ptr_t replace,
                 node_ptr_t left_end, node_ptr_t right_end) {
    node_t* cur = root.left_ptr();
    node_ptr_t cur_copy;
    node_t* successor = &root;
    bool side = 0; // 0 - L, 1 - R
    for (;;) {
      if (cur == nullptr || get_value(cur) == key) {
        node_t::link(successor, replace, side);
        node_t::link_left(replace.get(), left_end);
        node_t::link_right(replace.get(), right_end);
        return;
      }

      try {
        cur_copy = std::make_shared<val_node_t>(*static_cast<val_node_t*>(cur));
      } catch(...) {
        // successor is already linked to cur
        replace.reset();
        throw;
      }
      node_t::link(successor, cur_copy, side);

      if (get_value(cur_copy.get()) < key) {
        cur = cur_copy->right_ptr();

        successor = cur_copy.get();
        side = 1;
      } else if (key < get_value(cur_copy.get())) {
        cur = cur_copy->left_ptr();

        successor = cur_copy.get();
        side = 0;
      }
    }
  }
};

template <typename T>
struct persistent_set<T>::node_t {
  friend persistent_set<T>;
  
  node_t() = default;
  virtual ~node_t() = default;

  node_t(node_t const& other)
      : left(other.left),
        right(other.right) {}

  node_t& operator=(node_t const& other) {
    if (this != &other) {
      copy_from(other);
    }
    return *this;
  }

  node_t(node_t&& other) 
      : left(std::move(other.left)),
        right(std::move(other.right)) {}

  node_t& operator=(node_t&& other)  {
    if (this != &other) {
      move_from(other);
    }
    return *this;
  }

private:
  node_ptr_t left{nullptr};
  node_ptr_t right{nullptr};

  void copy_from(node_t const& other)  {
    link_left(this, other.left);
    link_right(this, other.right);
  }

  void move_from(node_t& other)  {
    copy_from(other);
    other.left.reset();
    other.right.reset();
  }

  node_t* left_ptr() {
    return left.get();
  }

  node_t* right_ptr() {
    return right.get();
  }
  
  static void link_left(node_t* parent, node_ptr_t const& left) {
    if (parent) {
      parent->left = left;
    }
  }
  
  static void link_right(node_t* parent, node_ptr_t const& right) {
    if (parent) {
      parent->right = right;
    }
  }

  static void link(node_t* parent, node_ptr_t const& child, bool side) {
    if (side) {
      link_right(parent, child);
    } else {
      link_left(parent, child);
    }
  }

  static node_t* min_in_subtree(node_t* p) {
    while (p && p->left_ptr()) {
      p = p->left_ptr();
    }
    return p;
  }

  static node_t* max_in_subtree(node_t* p) {
    while (p && p->right_ptr()) {
      p = p->right_ptr();
    }
    return p;
  }

  static node_t* prev(node_t* p, node_t* root) {
    T& key = persistent_set<T>::get_value(p);
    node_t* cur = root->left_ptr();
    node_t* successor = root;
    while (cur != nullptr) {
      if (persistent_set<T>::get_value(cur) < key) {
        successor = cur;
        cur = cur->right_ptr();
      } else {
        cur = cur->left_ptr();
      }
    }
    return successor;
  }

  static node_t* next(node_t* p, node_t* root) {
    T& key = persistent_set<T>::get_value(p);
    node_t* cur = root->left_ptr();
    node_t* successor = root;
    while (cur != nullptr) {
      if (key < persistent_set<T>::get_value(cur)) {
        successor = cur;
        cur = cur->left_ptr();
      } else {
        cur = cur->right_ptr();
      }
    }
    return successor;
  }

  bool is_leaf() const  {
    return !left && !right;
  }

  bool has_one_child() const  {
    return (left && !right) || (!left && right);
  }

  node_ptr_t get_only_child()  {
    return left ? left : right;
  }
};

template <typename T>
struct persistent_set<T>::val_node_t : persistent_set<T>::node_t {
  friend persistent_set<T>;
  
  val_node_t() = delete;
  explicit val_node_t(T const& val) : value(val) {}

  val_node_t(val_node_t const& other) : node_t(other), value(other.value) {}
  
private:
  T value;
};

template <typename T>
struct persistent_set<T>::iterator {
  friend persistent_set<T>;
  using iterator_category = std::bidirectional_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = T const;
  using pointer = T const*;
  using reference = T const&;
  
  iterator() = default;
  iterator(iterator const& other) : ptr(other.ptr), root(other.root) {}
  
  reference operator*() const {
    return persistent_set<T>::get_value(ptr);
  }
  pointer operator->() const {
    return &persistent_set<T>::get_value(ptr);
  }

  iterator& operator++() & {
    if (ptr != root) {
      ptr = node_t::next(ptr, root);
    }
    return *this;
  }
  iterator operator++(int) & {
    iterator tmp(*this);
    ++*this;
    return tmp;
  }

  iterator& operator--() & {
    if (ptr != root) {
      ptr = node_t::prev(ptr, root);
    } else {
      ptr = node_t::max_in_subtree(root->left_ptr());
    }
    return *this;
  }
  iterator operator--(int) & {
    iterator tmp(*this);
    --*this;
    return tmp;
  }

  friend bool operator==(iterator const& a, iterator const& b) {
    return a.ptr == b.ptr;
  }

  friend bool operator!=(iterator const& a, iterator const& b) {
    return a.ptr != b.ptr;
  }

private:
  node_t* ptr{nullptr};
  node_t* root;
  iterator(node_t* ptr, node_t* root)
      : ptr(ptr), root(root) {}
  
};

template <typename T>
typename persistent_set<T>::iterator
persistent_set<T>::erase(typename persistent_set<T>::iterator it) {
  if (it == end()) {
    return end();
  }

  node_t* node = it.ptr;

  if (node->is_leaf() || node->has_one_child()) {
    auto res = ++it;
    node_ptr_t child = node->get_only_child();
    auto key = get_value(node);

    copy_path(key, child, nullptr, nullptr);

    --size_;
    return res;
  } else {
    auto* n = node_t::next(node, &root);
    auto key = get_value(node);
    auto l = node->left, r = node->right;
    node_ptr_t next_copy = std::make_shared<val_node_t>(*static_cast<val_node_t*>(n)); // ok to throw
    erase(make_iterator(n)); ++size_;
    copy_path(key, next_copy, l, r);

    --size_;
    return make_iterator(next_copy.get());
  }
}
