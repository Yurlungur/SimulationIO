#include "Field.hpp"

#include "DiscreteField.hpp"

#include "H5Helpers.hpp"

namespace SimulationIO {

void Field::read(const H5::CommonFG &loc, const string &entry,
                 const shared_ptr<Project> &project) {
  this->project = project;
  auto group = loc.openGroup(entry);
  assert(H5::readAttribute<string>(group, "type", project->enumtype) ==
         "Field");
  H5::readAttribute(group, "name", name);
  assert(H5::readGroupAttribute<string>(group, "project", "name") ==
         project->name);
  // TODO: Read and interpret objects (shallowly) instead of naively only
  // looking at their names
  manifold = project->manifolds.at(
      H5::readGroupAttribute<string>(group, "manifold", "name"));
  configuration = project->configurations.at(
      H5::readGroupAttribute<string>(group, "configuration", "name"));
  assert(H5::readGroupAttribute<string>(
             group, string("configuration/fields/") + name, "name") == name);
  assert(H5::readGroupAttribute<string>(
             group, string("manifold/fields/") + name, "name") == name);
  tangentspace = project->tangentspaces.at(
      H5::readGroupAttribute<string>(group, "tangentspace", "name"));
  assert(H5::readGroupAttribute<string>(
             group, string("tangentspace/fields/") + name, "name") == name);
  tensortype = project->tensortypes.at(
      H5::readGroupAttribute<string>(group, "tensortype", "name"));
  H5::readGroup(group, "discretefields",
                [&](const H5::Group &group, const string &name) {
                  readDiscreteField(group, name);
                });
  configuration->insert(name, shared_from_this());
  manifold->insert(name, shared_from_this());
  tangentspace->insert(name, shared_from_this());
  tensortype->noinsert(shared_from_this());
}

ostream &Field::output(ostream &os, int level) const {
  os << indent(level) << "Field " << quote(name) << ": Configuration "
     << quote(configuration->name) << " Manifold " << quote(manifold->name)
     << " TangentSpace " << quote(tangentspace->name) << " TensorType "
     << quote(tensortype->name) << "\n";
  for (const auto &df : discretefields)
    df.second->output(os, level + 1);
  return os;
}

void Field::write(const H5::CommonFG &loc, const H5::H5Location &parent) const {
  assert(invariant());
  auto group = loc.createGroup(name);
  H5::createAttribute(group, "type", project.lock()->enumtype, "Field");
  H5::createAttribute(group, "name", name);
  H5::createHardLink(group, "project", parent, ".");
  H5::createHardLink(group, "configuration", parent,
                     string("configurations/") + configuration->name);
  H5::createHardLink(group, string("project/configurations/") +
                                configuration->name + "/fields",
                     name, group, ".");
  H5::createHardLink(group, "manifold", parent,
                     string("manifolds/") + manifold->name);
  H5::createHardLink(group,
                     string("project/manifolds/") + manifold->name + "/fields",
                     name, group, ".");
  H5::createHardLink(group, "tangentspace", parent,
                     string("tangentspaces/") + tangentspace->name);
  H5::createHardLink(group, string("project/tangentspaces/") +
                                tangentspace->name + "/fields",
                     name, group, ".");
  H5::createHardLink(group, "tensortype", parent,
                     string("tensortypes/") + tensortype->name);
  H5::createGroup(group, "discretefields", discretefields);
}

shared_ptr<DiscreteField>
Field::createDiscreteField(const string &name,
                           const shared_ptr<Configuration> &configuration,
                           const shared_ptr<Discretization> &discretization,
                           const shared_ptr<Basis> &basis) {
  auto discretefield = DiscreteField::create(
      name, shared_from_this(), configuration, discretization, basis);
  checked_emplace(discretefields, discretefield->name, discretefield);
  assert(discretefield->invariant());
  return discretefield;
}
shared_ptr<DiscreteField> Field::readDiscreteField(const H5::CommonFG &loc,
                                                   const string &entry) {
  auto discretefield = DiscreteField::create(loc, entry, shared_from_this());
  checked_emplace(discretefields, discretefield->name, discretefield);
  assert(discretefield->invariant());
  return discretefield;
}
}
