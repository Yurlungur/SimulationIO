#ifndef SIMULATIONIO_HPP
#define SIMULATIONIO_HPP

// Note: Unless noted otherwise, all pointers are non-null

// When translating to HDF5:
// - structs become objects, usually groups
// - simple struct fields (int, string) become attributes of the group
// - pointers become links inside the group
// - sets containing non-pointers become objects inside the group
// - sets containing pointers become links inside a subgroup of the group
// - vectors of simple types (int, string) become attributes
// - other vectors become objects inside a subgroup the group, sorted
//   alphabetically

#include <hdf5.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace SimulationIO {

using std::map;
using std::ostream;
using std::string;
using std::vector;

namespace {
inline int ipow(int base, int exp) {
  assert(base >= 0 && exp >= 0);
  int res = 1;
  while (exp--)
    res *= base;
  return res;
}
}

// Indented output
const int indentsize = 2;
const char indentchar = ' ';
inline string indent(int level) {
  return string(level * indentsize, indentchar);
}

// Type hierarchy
struct Component {
  virtual string getname() const = 0;
  virtual ostream &output(ostream &os, int level = 0) const = 0;
  virtual void write(hid_t loc) const = 0;
};

// Tensor types

struct TensorType;
struct TensorComponent;

extern map<string, TensorType *> tensortypes;

struct TensorType {
  string name;
  int dimension;
  int rank;
  vector<TensorComponent *> storedcomponents; // owned
  bool invariant() const {
    return !name.empty() && dimension >= 0 && rank >= 0 &&
           int(storedcomponents.size()) <= ipow(dimension, rank);
  }
  TensorType() = delete;
  TensorType(const TensorType &) = delete;
  TensorType(TensorType &&) = delete;
  TensorType &operator=(const TensorType &) = delete;
  TensorType &operator=(TensorType &&) = delete;
  TensorType(const string &name, int dimension, int rank)
      : name(name), dimension(dimension), rank(rank) {
    assert(invariant());
    assert(tensortypes.find(name) == tensortypes.end());
    tensortypes[name] = this;
  }
  ~TensorType() = delete;
  void push_back(TensorComponent *tensorcomponent) {
    storedcomponents.push_back(tensorcomponent);
  }
  ostream &output(ostream &os, int level = 0) const;
  friend ostream &operator<<(ostream &os, const TensorType &tensortype) {
    return tensortype.output(os);
  }
  void write(hid_t loc) const {}
  TensorType(hid_t loc, const string &name) { assert(0); }
};

struct TensorComponent {
  string name;
  TensorType *tensortype;
  // We use objects to denote most concepts, but we make an exception
  // for tensor component indices and tangent space basis vectors,
  // which we number consecutively starting from zero. This simplifies
  // the representation, and it introduces a canonical order (e.g. x,
  // y, z) among the tangent space directions that people are
  // expecting.
  int storedcomponent;
  vector<int> indexvalues;
  bool invariant() const {
    bool inv = !name.empty() && storedcomponent >= 0 &&
               storedcomponent < int(tensortype->storedcomponents.size()) &&
               int(indexvalues.size()) == tensortype->rank;
    for (int i = 0; i < int(indexvalues.size()); ++i)
      inv &= indexvalues[i] >= 0 && indexvalues[i] < tensortype->dimension;
    inv &= tensortype->storedcomponents[storedcomponent] == this;
    for (int c = 0; c < storedcomponent; ++c) {
      const auto &other = tensortype->storedcomponents[c];
      bool samesize = other->indexvalues.size() == indexvalues.size();
      inv &= samesize;
      if (samesize) {
        bool isequal = true;
        for (int i = 0; i < int(indexvalues.size()); ++i)
          isequal &= other->indexvalues[i] == indexvalues[i];
        inv &= !isequal;
      }
    }
    return inv;
  }
  TensorComponent() = delete;
  TensorComponent(const TensorComponent &) = delete;
  TensorComponent(TensorComponent &&) = delete;
  TensorComponent &operator=(const TensorComponent &) = delete;
  TensorComponent &operator=(TensorComponent &&) = delete;
  TensorComponent(const string &name, TensorType *tensortype,
                  int storedcomponent, const std::vector<int> &indexvalues)
      : name(name), tensortype(tensortype), storedcomponent(storedcomponent),
        indexvalues(indexvalues) {
    tensortype->push_back(this);
    assert(invariant());
  }
  ~TensorComponent() = delete;
  ostream &output(ostream &os, int level = 0) const;
  friend ostream &operator<<(ostream &os,
                             const TensorComponent &tensorcomponent) {
    return tensorcomponent.output(os);
  }
  void write(hid_t loc) const {}
  TensorComponent(hid_t loc, const string &name) { assert(0); }
};

// High-level continuum concepts

struct Manifold;
struct TangentSpace;
struct Field;
struct Discretization;
struct Basis;
struct DiscreteField;

extern map<string, Manifold *> manifolds;
extern map<string, TangentSpace *> tangentspaces;
extern map<string, Field *> fields;

struct Manifold {
  string name;
  int dimension;
  map<string, Discretization *> discretizations;
  map<string, Field *> fields;
  bool invariant() const { return !name.empty() && dimension >= 0; }
  Manifold() = delete;
  Manifold(const Manifold &) = delete;
  Manifold(Manifold &&) = delete;
  Manifold &operator=(const Manifold &) = delete;
  Manifold &operator=(Manifold &&) = delete;
  Manifold(const string &name, int dimension)
      : name(name), dimension(dimension) {
    assert(invariant());
    assert(manifolds.find(name) == manifolds.end());
    manifolds[name] = this;
  }
  void insert(const string &name, Field *field) {
    assert(fields.find(name) == fields.end());
    fields[name] = field;
  }
};

struct TangentSpace {
  string name;
  int dimension;
  map<string, Basis *> bases;
  map<string, Field *> fields;
  bool invariant() const { return !name.empty() && dimension >= 0; }
  TangentSpace() = delete;
  TangentSpace(const TangentSpace &) = delete;
  TangentSpace(TangentSpace &&) = delete;
  TangentSpace &operator=(const TangentSpace &) = delete;
  TangentSpace &operator=(TangentSpace &&) = delete;
  TangentSpace(const string &name, int dimension)
      : name(name), dimension(dimension) {
    assert(invariant());
    assert(tangentspaces.find(name) == tangentspaces.end());
    tangentspaces[name] = this;
  }
  void insert(const string &name, Field *field) {
    assert(fields.find(name) == fields.end());
    fields[name] = field;
  }
};

struct Field {
  string name;
  Manifold *manifold;
  TangentSpace *tangentspace;
  TensorType *tensortype;
  map<string, DiscreteField *> discretefields;
  bool invariant() const {
    return !name.empty() && tangentspace->dimension == tensortype->dimension &&
           manifold->fields.at(name) == this &&
           tangentspace->fields.at(name) == this;
  }
  Field() = delete;
  Field(const Field &) = delete;
  Field(Field &&) = delete;
  Field &operator=(const Field &) = delete;
  Field &operator=(Field &&) = delete;
  Field(const string &name, Manifold *manifold, TangentSpace *tangentspace,
        TensorType *tensortype)
      : name(name), manifold(manifold), tangentspace(tangentspace),
        tensortype(tensortype) {
    manifold->insert(name, this);
    tangentspace->insert(name, this);
    // tensortypes->insert(this);
    assert(invariant());
    assert(fields.find(name) == fields.end());
    fields[name] = this;
  }
};

// Manifold discretizations

struct DiscretizationBlock;

struct Discretization {
  string name;
  Manifold *manifold;
  map<string, DiscretizationBlock *> discretizationblocks;
  bool invariant() const { return true; }
};

struct DiscretizationBlock {
  // Discretization of a certain region, represented by contiguous data
  string name;
  Discretization *discretization;
  // bounding box? in terms of coordinates?
  // connectivity? neighbouring blocks?
  // overlaps?
  bool invariant() const { return true; }
};

// Tangent space bases

struct BasisVector;
struct CoordinateBasis;
struct CoordinateBasisElement;

struct Basis {
  string name;
  TangentSpace *tangentspace;
  vector<BasisVector *> basisvectors;
  map<string, CoordinateBasis *> coordinatebases;
  bool invariant() const {
    return int(basisvectors.size()) == tangentspace->dimension;
  }
};

struct BasisVector {
  string name;
  Basis *basis;
  // Since a BasisVector denotes essentially only an integer, we
  // should be able to replace it by an integer. Not sure this is
  // worthwhile. This essentially only gives names to directions; could use
  // vector<string> in TangentSpace for this instead.
  int direction;
  map<string, CoordinateBasisElement *> coordinatebasiselements;
  bool invariant() const {
    return direction >= 0 && direction < int(basis->basisvectors.size()) &&
           basis->basisvectors[direction] == this;
  }
};

// Discrete fields

struct DiscreteFieldBlock;
struct DiscreteFieldBlockData;

struct DiscreteField {
  string name;
  Field *field;
  Discretization *discretization;
  Basis *basis;
  map<string, DiscreteFieldBlock *> discretefieldblocks;
  bool invariant() const {
    return field->manifold == discretization->manifold &&
           field->tangentspace == basis->tangentspace;
  }
};

struct DiscreteFieldBlock {
  // Discrete field on a particular region (discretization block)
  string name;
  DiscreteField *discretefield;
  DiscretizationBlock *discretizationblock;
  map<string, DiscreteFieldBlockData *> discretefieldblockdata;
  bool invariant() const { return true; }
};

struct DiscreteFieldBlockData {
  // Tensor component for a discrete field on a particular region
  string name;
  DiscreteFieldBlock *discretefieldblock;
  TensorComponent *tensorcomponent;
  hid_t hdf5_dataset;
  bool invariant() const {
    return discretefieldblock->discretefield->field->tensortype ==
           tensorcomponent->tensortype;
  }
};

// Coordinates

struct CoordinateSystem;
struct CoordinateField;

extern map<string, CoordinateSystem *> coordinates;

struct CoordinateSystem {
  string name;
  Manifold *manifold;
  vector<CoordinateField *> coordinatefields;
  map<string, CoordinateBasis *> coordinatebases;
  bool invariant() const { return true; }
};

struct CoordinateField {
  CoordinateSystem *coordinatesystem;
  int direction;
  Field *field;
  bool invariant() const {
    return direction >= 0 &&
           direction < int(coordinatesystem->coordinatefields.size()) &&
           coordinatesystem->coordinatefields[direction] == this;
  }
};

struct CoordinateBasis {
  CoordinateSystem *coordinatesystem;
  Basis *basis;
  vector<CoordinateBasisElement *> coordinatebasiselements;
};

struct CoordinateBasisElement {
  CoordinateBasis *coordinatebasis;
  CoordinateField *coordinatefield;
  BasisVector *basisvector;
  bool invariant() const {
    return coordinatefield->direction == basisvector->direction;
  }
};
}

#endif // #SIMULATIONIO_HPP
