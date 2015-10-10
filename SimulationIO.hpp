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

#include "H5Helpers.hpp"

#include <H5Cpp.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace SimulationIO {

using std::map;
using std::ostream;
using std::string;
using std::vector;

// Integer exponentiation
inline int ipow(int base, int exp) {
  assert(base >= 0 && exp >= 0);
  int res = 1;
  while (exp--)
    res *= base;
  return res;
}

// Indented output
const int indentsize = 2;
const char indentchar = ' ';
inline string indent(int level) {
  return string(level * indentsize, indentchar);
}

// Common to all file elements

struct Common {
  string name;
  Common(const string &name) : name(name) {}
  virtual ~Common() {}
  virtual bool invariant() const { return !name.empty(); }
  virtual ostream &output(ostream &os, int level = 0) const = 0;
  virtual void write(H5::CommonFG &loc) const = 0;
};

// Projects

struct Project;

Project *createProject(const string &name);
Project *createProject(const string &name, const H5::CommonFG &loc);

struct TensorType;
struct Manifold;
struct TangentSpace;
struct Field;
struct CoordinateSystem;

struct Project : Common {
  map<string, TensorType *> tensortypes;
  map<string, Manifold *> manifolds;
  map<string, TangentSpace *> tangentspaces;
  map<string, Field *> fields;
  map<string, CoordinateSystem *> coordinatesystems;
  virtual bool invariant() const { return Common::invariant(); }
  Project(const Project &) = delete;
  Project(Project &&) = delete;
  Project &operator=(const Project &) = delete;
  Project &operator=(Project &&) = delete;

private:
  friend Project *createProject(const string &name);
  friend Project *createProject(const string &name, const H5::CommonFG &loc);
  Project(const string &name) : Common(name) {}
  Project(const string &name, const H5::CommonFG &loc);

public:
  virtual ~Project() { assert(0); }
  void createStandardTensortypes();
  TensorType *createTensorType(const string &name, int dimension, int rank);
  TensorType *createTensorType(const string &name, const H5::CommonFG &loc);
  Manifold *createManifold(const string &name, int dimension);
  Manifold *createManifold(const string &name, const H5::CommonFG &loc);
  TangentSpace *createTangentSpace(const string &name, int dimension);
  TangentSpace *createTangentSpace(const string &name, const H5::CommonFG &loc);
  Field *createField(const string &name, Manifold *manifold,
                     TangentSpace *tangentspace, TensorType *tensortype);
  Field *createField(const string &name, const H5::CommonFG &loc);
  CoordinateSystem *createCoordinateSystem(const string &name,
                                           Manifold *manifold);
  CoordinateSystem *createCoordinateSystem(const string &name,
                                           const H5::CommonFG &loc);
  virtual ostream &output(ostream &os, int level = 0) const;
  friend ostream &operator<<(ostream &os, const Project &project) {
    return project.output(os);
  }
  virtual void write(H5::CommonFG &loc) const;
};

// Tensor types

struct TensorComponent;

struct TensorType : Common {
  Project *project;
  int dimension;
  int rank;
  map<string, TensorComponent *> tensorcomponents; // owned
  virtual bool invariant() const {
    bool inv = Common::invariant() && bool(project) &&
               project->tensortypes.count(name) &&
               project->tensortypes.at(name) == this && dimension >= 0 &&
               rank >= 0 &&
               int(tensorcomponents.size()) <= ipow(dimension, rank);
    for (const auto &tc : tensorcomponents)
      inv &= !tc.first.empty() && bool(tc.second);
    return inv;
  }
  TensorType() = delete;
  TensorType(const TensorType &) = delete;
  TensorType(TensorType &&) = delete;
  TensorType &operator=(const TensorType &) = delete;
  TensorType &operator=(TensorType &&) = delete;

private:
  friend class Project;
  TensorType(const string &name, Project *project, int dimension, int rank)
      : Common(name), project(project), dimension(dimension), rank(rank) {}
  TensorType(const string &name, Project *project, const H5::CommonFG &loc);

public:
  virtual ~TensorType() { assert(0); }
  TensorComponent *createTensorComponent(const string &name,
                                         const std::vector<int> &indexvalues);
  TensorComponent *createTensorComponent(const string &name,
                                         const H5::CommonFG &loc);
  virtual ostream &output(ostream &os, int level = 0) const;
  friend ostream &operator<<(ostream &os, const TensorType &tensortype) {
    return tensortype.output(os);
  }
  virtual void write(H5::CommonFG &loc) const;
};

struct TensorComponent : Common {
  TensorType *tensortype;
  // We use objects to denote most Commons, but we make an exception
  // for tensor component indices and tangent space basis vectors,
  // which we number consecutively starting from zero. This simplifies
  // the representation, and it introduces a canonical order (e.g. x,
  // y, z) among the tangent space directions that people are
  // expecting.
  vector<int> indexvalues;
  virtual bool invariant() const {
    bool inv = Common::invariant() && bool(tensortype) &&
               tensortype->tensorcomponents[name] == this &&
               int(indexvalues.size()) == tensortype->rank;
    for (int i = 0; i < int(indexvalues.size()); ++i)
      inv &= indexvalues[i] >= 0 && indexvalues[i] < tensortype->dimension;
    // Ensure all tensor components are distinct
    for (const auto &tc : tensortype->tensorcomponents) {
      const auto &other = tc.second;
      if (other == this)
        continue;
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

private:
  friend class TensorType;
  TensorComponent(const string &name, TensorType *tensortype,
                  const std::vector<int> &indexvalues)
      : Common(name), tensortype(tensortype), indexvalues(indexvalues) {}
  TensorComponent(const string &name, TensorType *tensortype,
                  const H5::CommonFG &loc);

public:
  virtual ~TensorComponent() { assert(0); }
  virtual ostream &output(ostream &os, int level = 0) const;
  friend ostream &operator<<(ostream &os,
                             const TensorComponent &tensorcomponent) {
    return tensorcomponent.output(os);
  }
  virtual void write(H5::CommonFG &loc) const;
};

// High-level continuum concepts

struct Discretization;
struct Basis;
struct DiscreteField;

struct Manifold : Common {
  Project *project;
  int dimension;
  map<string, Discretization *> discretizations;
  map<string, Field *> fields;
  virtual bool invariant() const {
    bool inv = Common::invariant() && bool(project) &&
               project->manifolds.count(name) &&
               project->manifolds.at(name) == this && dimension >= 0;
    for (const auto &d : discretizations)
      inv &= !d.first.empty() && bool(d.second);
    for (const auto &f : fields)
      inv &= !f.first.empty() && bool(f.second);
    return inv;
  }
  Manifold() = delete;
  Manifold(const Manifold &) = delete;
  Manifold(Manifold &&) = delete;
  Manifold &operator=(const Manifold &) = delete;
  Manifold &operator=(Manifold &&) = delete;

private:
  friend class Project;
  Manifold(const string &name, Project *project, int dimension)
      : Common(name), project(project), dimension(dimension) {}
  Manifold(const string &name, Project *project, const H5::CommonFG &loc);

public:
  virtual ~Manifold() { assert(0); }
  void insert(const string &name, Field *field) {
    assert(!fields.count(name));
    fields[name] = field;
  }
  virtual ostream &output(ostream &os, int level = 0) const;
  friend ostream &operator<<(ostream &os, const Manifold &manifold) {
    return manifold.output(os);
  }
  virtual void write(H5::CommonFG &loc) const;
};

struct TangentSpace : Common {
  Project *project;
  int dimension;
  map<string, Basis *> bases;
  map<string, Field *> fields;
  virtual bool invariant() const {
    bool inv = Common::invariant() && bool(project) &&
               project->tangentspaces.count(name) &&
               project->tangentspaces.at(name) == this && dimension >= 0;
    for (const auto &b : bases)
      inv &= !b.first.empty() && bool(b.second);
    for (const auto &f : fields)
      inv &= !f.first.empty() && bool(f.second);
    return inv;
  }
  TangentSpace() = delete;
  TangentSpace(const TangentSpace &) = delete;
  TangentSpace(TangentSpace &&) = delete;
  TangentSpace &operator=(const TangentSpace &) = delete;
  TangentSpace &operator=(TangentSpace &&) = delete;

private:
  friend class Project;
  TangentSpace(const string &name, Project *project, int dimension)
      : Common(name), project(project), dimension(dimension) {}
  TangentSpace(const string &name, Project *project, const H5::CommonFG &loc);

public:
  virtual ~TangentSpace() { assert(0); }
  void insert(const string &name, Field *field) {
    assert(!fields.count(name));
    fields[name] = field;
  }
  virtual ostream &output(ostream &os, int level = 0) const;
  friend ostream &operator<<(ostream &os, const TangentSpace &tangentspace) {
    return tangentspace.output(os);
  }
  virtual void write(H5::CommonFG &loc) const;
};

struct Field : Common {
  Project *project;
  Manifold *manifold;
  TangentSpace *tangentspace;
  TensorType *tensortype;
  map<string, DiscreteField *> discretefields;
  virtual bool invariant() const {
    bool inv = Common::invariant() && bool(project) &&
               project->fields.count(name) &&
               project->fields.at(name) == this && bool(manifold) &&
               bool(tangentspace) && bool(tensortype) &&
               tangentspace->dimension == tensortype->dimension &&
               manifold->fields.at(name) == this &&
               tangentspace->fields.at(name) == this;
    for (const auto &df : discretefields)
      inv &= !df.first.empty() && bool(df.second);
    return inv;
  }
  Field() = delete;
  Field(const Field &) = delete;
  Field(Field &&) = delete;
  Field &operator=(const Field &) = delete;
  Field &operator=(Field &&) = delete;

private:
  friend class Project;
  Field(const string &name, Project *project, Manifold *manifold,
        TangentSpace *tangentspace, TensorType *tensortype)
      : Common(name), project(project), manifold(manifold),
        tangentspace(tangentspace), tensortype(tensortype) {
    manifold->insert(name, this);
    tangentspace->insert(name, this);
    // tensortypes->insert(this);
  }
  Field(const string &name, Project *project, const H5::CommonFG &loc);

public:
  virtual ~Field() { assert(0); }
  virtual ostream &output(ostream &os, int level = 0) const;
  friend ostream &operator<<(ostream &os, const Field &field) {
    return field.output(os);
  }
  virtual void write(H5::CommonFG &loc) const;
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

struct CoordinateField;

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
