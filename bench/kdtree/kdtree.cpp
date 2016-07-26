#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cassert>
#include <cmath>
#include <thread>
#include <mutex>
#include <cilk/cilk.h>

typedef float point_t[3];

class node {
public:
  float min[3] = {0.0f, 0.0f, 0.0f};
  float max[3] = {1.0f, 1.0f, 1.0f};
  point_t point;
  volatile bool isleaf = true;
  node *left = nullptr;
  node *right = nullptr;
  mutable std::mutex mut;

  node() = default;
  node(node& other) :
    isleaf(other.isleaf), left(other.left), right(other.right)
  {
    memcpy(min, other.min, sizeof(point_t));
    memcpy(max, other.max, sizeof(point_t));
    memcpy(point, other.point, sizeof(point_t));
  }
  node& operator=(const node& other)
  {
    isleaf = other.isleaf;
    left = other.left;
    right = other.right;
    memcpy(min, other.min, sizeof(point_t));
    memcpy(max, other.max, sizeof(point_t));
    memcpy(point, other.point, sizeof(point_t));
  }

  node(const int init)
  {
    point[0] = point[1] = point[2] = ((float) init) /
      std::numeric_limits<int>::max();
  }

  bool operator==(node& rhs) const
  {
    for (int i = 0; i < 3; ++i) {
      if (min[i] != rhs.min[i] || max[i] != rhs.max[i])
        return false;
      if (point[i] != rhs.point[i])
        return false;
    }

    if (isleaf) return rhs.isleaf;
    return (*left == *rhs.left) && (*right == *rhs.right);
  }
};

static size_t num_bodies;
static size_t max_depth;
static node* bodies;
static node* bodies2;

void read_file(const char* filename)
{
  std::ifstream input(filename, std::ifstream::in);
  int temp;
  for (int i = 0; i < num_bodies; ++i) {
    input >> temp;
    bodies[i] = node(temp);
  }
  input.close();
}

void make_leaf(node* root, const point_t point, const int split)
{
  node* left = root->left = new node(*root);
  node* right = root->right = new node(*root);

  root->left->max[split] = root->point[split];
  root->right->min[split] = root->point[split];

  const float *leftpt, *rightpt;
  if (root->point[split] < point[split]) {
    leftpt = &root->point[0];
    rightpt = &point[0];
  } else {
    leftpt = &point[0];
    rightpt = &root->point[0];
  }

  memcpy(root->left->point, leftpt, sizeof(point_t));
  memcpy(root->right->point, rightpt, sizeof(point_t));
  root->isleaf = false;
}

void insert(node* root, const point_t point, const int split)
{
  if (root->isleaf) {
    //root->mut.lock();
    std::lock_guard<std::mutex> lock(root->mut);
    __sync_synchronize();
    if (root->isleaf) {
      make_leaf(root, point, split);
      return;
    }
    //root->mut.unlock();
  }

  root = (point[split] < root->left->max[split]) ? root->left : root->right;
  insert(root, point, (split + 1) % 3);
}

int main(int argc, char *argv[])
{
  num_bodies = (argc > 1) ? atol(argv[argc - 1]) : 1024;
  bodies = (node*) malloc(num_bodies * sizeof(node));

  if (argc == 3) read_file(argv[1]);
  else if (argc > 3) {
    std::cerr << argv[0] << "<input file> <num points>" << std::endl;
    std::exit(1);
  } else {
    srand(time(NULL));
    for (int i = 0; i < num_bodies; ++i) {
      bodies[i] = node(rand());
    }
  }

  // @todo: start timing
  // call insert(root, bodylist, nbodies)
  node* root = new node(bodies[0]);
  int depth = (int) ceil(log2((double)num_bodies)) + 1;

  int begin = 0;
  for (int d = 0; d <= depth; ++d) {
    int end = begin + (1 << d);
    if (end > num_bodies) end = num_bodies;
    for (int i = begin; i < end; ++i) {
      insert(root, bodies[i].point, 0);
    }
    begin = end;
  }
  // @todo: end timing

  // Debugging
  node *seqroot = new node(bodies[0]);
  for (int i = 0; i < num_bodies; ++i) {
    insert(seqroot, bodies[i].point, 0);
  }
  assert(*seqroot == *root);

  free(bodies);


  return 0;
}
