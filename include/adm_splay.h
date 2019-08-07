#ifndef __ADAMANT_SPLAY
#define __ADAMANT_SPLAY

#include <adm_database.h>
#include <pthread.h>

pthread_mutex_t node_lock = PTHREAD_MUTEX_INITIALIZER;

namespace adamant
{

class adm_splay_tree_t
{
  private:

    void zigzig(adm_splay_tree_t* granpa) noexcept
    {
      adm_splay_tree_t* tmp = parent->right;
      granpa->left=tmp;
      if(tmp) tmp->parent=granpa;

      parent->left=right;
      if(right) right->parent=parent;
    
      tmp = granpa->parent;

      parent->right=granpa;
      granpa->parent=parent;
      right=parent;
      right->parent=this;

      parent=tmp;
      if(tmp) {
        if(tmp->right==granpa) tmp->right=this;
        else tmp->left=this;
      }
    
    }

    void zagzag(adm_splay_tree_t* granpa) noexcept
    {
      adm_splay_tree_t* tmp = parent->left;
      granpa->right=tmp;
      if(tmp) tmp->parent=granpa;

      parent->right=left;
      if(left) left->parent=parent;
    
      tmp = granpa->parent;

      parent->left=granpa;
      granpa->parent=parent;
      left=parent;
      left->parent=this;

      parent=tmp;
      if(tmp) {
        if(tmp->right==granpa) tmp->right=this;
        else tmp->left=this;
      }
    }

    void zagzig(adm_splay_tree_t* granpa) noexcept
    {
      parent->right=left;
      if(left) left->parent=parent;

      granpa->left=right;
      if(right) right->parent=granpa;
    
      adm_splay_tree_t* root = granpa->parent;

      left=parent;
      left->parent=this;
      right=granpa;
      right->parent=this;

      parent=root;
      if(root) {
        if(root->right==granpa) root->right=this;
        else root->left=this;
      }
    }

    void zigzag(adm_splay_tree_t* granpa) noexcept
    {
      granpa->right=left;
      if(left) left->parent=granpa;

      parent->left=right;
      if(right) right->parent=parent;
    
      adm_splay_tree_t* root = granpa->parent;

      left=granpa;
      granpa->parent=this;
      right=parent;
      parent->parent=this;

      parent=root;
      if(root) {
        if(root->right==granpa) root->right=this;
        else root->left=this;
      }
    }
    
    void zig() noexcept
    {
      parent->left = right;
      if(right) right->parent = parent;
      right = parent;
      right->parent = this;
      parent = nullptr;
    }
    
    void zag() noexcept
    {
      parent->right = left;
      if(left) left->parent = parent;
      left = parent;
      left->parent = this;
      parent = nullptr;
    }

    adm_splay_tree_t* parent;
    adm_splay_tree_t* left;
    adm_splay_tree_t* right;
    uint64_t size;

  public:

    uint64_t start;
    uint64_t end;

    adm_object_t* object;

    adm_splay_tree_t(): parent(nullptr), left(nullptr), right(nullptr), start(0), end(0), object(nullptr) {}

    uint64_t get_size() const noexcept { return size&0x0FFFFFFFFFFFFFFF; };

    void set_size(const uint64_t s) {size = (size&0xF000000000000000)|s; };

    adm_splay_tree_t* min() noexcept
    {
      pthread_mutex_lock(&node_lock);
      //fprintf(stderr, "1 ");
      adm_splay_tree_t* m = this;
      while(m->left) m=m->left;
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&node_lock);
      return m;
    }

    adm_splay_tree_t* find(const uint64_t start) noexcept
    {
      pthread_mutex_lock(&node_lock);
      //fprintf(stderr, "1 ");
      adm_splay_tree_t* f = this;
      while(f) {
        if(f->start<=start && start<f->end) break;
        if(start<f->start) f = f->left;
        else f = f->right;
      }
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&node_lock);
      return f;
    }

    void find_with_parent(const uint64_t start, adm_splay_tree_t*& p, adm_splay_tree_t*& f) noexcept
    {
      pthread_mutex_lock(&node_lock);
      //fprintf(stderr, "1 ");
      f = this; p = f->parent;
      while(f) {
        if(f->start==start && start<f->end) break;
        p = f;
        if(start<f->start) f = f->left;
        else f = f->right;
      }
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&node_lock);
    }

    void insert(adm_splay_tree_t* a) noexcept
    {
      pthread_mutex_lock(&node_lock);
      //fprintf(stderr, "1 ");
      if(start>a->start) left = a;
      else right = a;
      a->parent = this;
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&node_lock);
    }

    adm_splay_tree_t* splay() noexcept
    {
      //fprintf(stderr, "1 ");
      while(parent) {
        adm_splay_tree_t* granpa = parent->parent;

        if(granpa) {
          if(parent->right==this) {
            if(granpa->left==parent) zagzig(granpa);
            else zagzag(granpa);
          } else {
            if(granpa->left==parent) zigzig(granpa);
            else zigzag(granpa);
          }
        }
        else {
          if(parent->left==this) zig();
          else zag();
        }
      }
      //fprintf(stderr, "2 ");
      return this;
    }

     void postorder(int indent)
    {
	if(this->left != nullptr) this->left->postorder(indent+4);
        if(this->right != nullptr) this->right->postorder(indent+4);
        if (indent) {
            std::cout << std::setw(indent) << ' ';
        }
        fprintf(stdout, "offset address: %lx, associated object: %d\n", this->start, this->object->get_object_id());
    }

    state_t get_state() const noexcept { return static_cast<state_t>(size>>60); };

    void set_state(const state_t state) {size = (size&0x0FFFFFFFFFFFFFFF)|(static_cast<uint64_t>(state)<<60); };

    void add_state(const state_t state) {size |= (static_cast<uint64_t>(state)<<60); };
};

}

#endif
