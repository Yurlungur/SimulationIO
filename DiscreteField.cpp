#include "DiscreteField.hpp"

#include "DiscreteFieldBlock.hpp"

#include "H5Helpers.hpp"

namespace SimulationIO {

DiscreteField::DiscreteField(const H5::CommonFG &loc, const string &entry,
                             Field *field)
    : field(field) {
  auto group = loc.openGroup(entry);
  string type;
  H5::readAttribute(group, "type", field->project->enumtype, type);
  assert(type == "DiscreteField");
  H5::readAttribute(group, "name", name);
#warning "TODO: check link field"
  // TODO: Read and interpret objects (shallowly) instead of naively only
  // looking at their names
  {
    auto obj = group.openGroup("discretization");
    // H5::Group obj;
    // H5::readAttribute(group, "discretization", obj);
    string name;
    auto attr = H5::readAttribute(obj, "name", name);
    discretization = field->manifold->discretizations.at(name);
  }
  {
    auto obj = group.openGroup("basis");
    // H5::Group obj;
    // H5::readAttribute(group, "basis", obj);
    string name;
    auto attr = H5::readAttribute(obj, "name", name);
    basis = field->tangentspace->bases.at(name);
  }
  H5::readGroup(group, "discretefieldblocks",
                [&](const H5::Group &group, const string &name) {
                  createDiscreteFieldBlock(group, name);
                });
  discretization->noinsert(this);
  basis->noinsert(this);
}

ostream &DiscreteField::output(ostream &os, int level) const {
  os << indent(level) << "DiscreteField \"" << name << "\": field=\""
     << field->name << "\" discretization=\"" << discretization->name
     << "\" basis=\"" << basis->name << "\"\n";
  for (const auto &db : discretefieldblocks)
    db.second->output(os, level + 1);
  return os;
}

void DiscreteField::write(const H5::CommonFG &loc,
                          const H5::H5Location &parent) const {
  auto group = loc.createGroup(name);
  H5::createAttribute(group, "type", field->project->enumtype, "DiscreteField");
  H5::createAttribute(group, "name", name);
  H5::createHardLink(group, "field", parent, ".");
  H5::createHardLink(group, "discretization", parent,
                     string("manifold/discretizations/") +
                         discretization->name);
  H5::createHardLink(group, "basis", parent,
                     string("tangentspace/bases/") + basis->name);
  // H5::createAttribute(group, "field", parent, ".");
  // H5::createAttribute(group, "discretization", parent,
  //                     string("manifold/discretizations/") +
  //                         discretization->name);
  // H5::createAttribute(group, "basis", parent,
  //                     string("tangentspace/bases/") +
  //                     basis->name);
  H5::createGroup(group, "discretefieldblocks", discretefieldblocks);
}

DiscreteFieldBlock *DiscreteField::createDiscreteFieldBlock(
    const string &name, DiscretizationBlock *discretizationblock) {
  auto discretefieldblock =
      new DiscreteFieldBlock(name, this, discretizationblock);
  checked_emplace(discretefieldblocks, discretefieldblock->name,
                  discretefieldblock);
  assert(discretefieldblock->invariant());
  return discretefieldblock;
}

DiscreteFieldBlock *
DiscreteField::createDiscreteFieldBlock(const H5::CommonFG &loc,
                                        const string &entry) {
  auto discretefieldblock = new DiscreteFieldBlock(loc, entry, this);
  checked_emplace(discretefieldblocks, discretefieldblock->name,
                  discretefieldblock);
  assert(discretefieldblock->invariant());
  return discretefieldblock;
}
}
