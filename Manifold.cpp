#include "Manifold.hpp"

#include "CoordinateSystem.hpp"
#include "Discretization.hpp"
#include "Field.hpp"
#include "SubDiscretization.hpp"

#include "H5Helpers.hpp"

#include <algorithm>
#include <set>

namespace SimulationIO {

using std::equal;
using std::set;

void Manifold::read(const H5::CommonFG &loc, const string &entry,
                    const shared_ptr<Project> &project) {
  this->project = project;
  auto group = loc.openGroup(entry);
  assert(H5::readAttribute<string>(group, "type", project->enumtype) ==
         "Manifold");
  H5::readAttribute(group, "name", name);
  assert(H5::readGroupAttribute<string>(group, "project", "name") ==
         project->name);
  configuration = project->configurations.at(
      H5::readGroupAttribute<string>(group, "configuration", "name"));
  assert(H5::readGroupAttribute<string>(
             group, string("configuration/manifolds/") + name, "name") == name);
  H5::readAttribute(group, "dimension", dimension);
  H5::readGroup(group, "discretizations",
                [&](const H5::Group &group, const string &name) {
                  readDiscretization(group, name);
                });
  H5::readGroup(group, "subdiscretizations",
                [&](const H5::Group &group, const string &name) {
                  readSubDiscretization(group, name);
                });
  // Cannot check "fields", "coordinatesystems" since they have not been read
  // yet
  // assert(H5::checkGroupNames(group, "fields", fields));
  // assert(H5::checkGroupNames(group, "coordinatesystems", fields));
  configuration->insert(name, shared_from_this());
}

ostream &Manifold::output(ostream &os, int level) const {
  os << indent(level) << "Manifold " << quote(name) << ": Configuration "
     << quote(configuration->name) << " dim=" << dimension << "\n";
  for (const auto &d : discretizations)
    d.second->output(os, level + 1);
  for (const auto &sd : subdiscretizations)
    sd.second->output(os, level + 1);
  for (const auto &f : fields)
    os << indent(level + 1) << "Field " << quote(f.second.lock()->name) << "\n";
  for (const auto &cs : coordinatesystems)
    os << indent(level + 1) << "CoordinateSystem "
       << quote(cs.second.lock()->name) << "\n";
  return os;
}

void Manifold::write(const H5::CommonFG &loc,
                     const H5::H5Location &parent) const {
  assert(invariant());
  auto group = loc.createGroup(name);
  H5::createAttribute(group, "type", project.lock()->enumtype, "Manifold");
  H5::createAttribute(group, "name", name);
  H5::createHardLink(group, "project", parent, ".");
  H5::createHardLink(group, "configuration", parent,
                     string("configurations/") + configuration->name);
  H5::createHardLink(group, string("project/configurations/") +
                                configuration->name + "/manifolds",
                     name, group, ".");
  H5::createAttribute(group, "dimension", dimension);
  H5::createGroup(group, "discretizations", discretizations);
  H5::createGroup(group, "subdiscretizations", subdiscretizations);
  group.createGroup("fields");
  group.createGroup("coordinatesystems");
}

shared_ptr<Discretization>
Manifold::createDiscretization(const string &name,
                               const shared_ptr<Configuration> &configuration) {
  auto discretization =
      Discretization::create(name, shared_from_this(), configuration);
  checked_emplace(discretizations, discretization->name, discretization);
  assert(discretization->invariant());
  return discretization;
}

shared_ptr<Discretization> Manifold::readDiscretization(const H5::CommonFG &loc,
                                                        const string &entry) {
  auto discretization = Discretization::create(loc, entry, shared_from_this());
  checked_emplace(discretizations, discretization->name, discretization);
  assert(discretization->invariant());
  return discretization;
}

shared_ptr<SubDiscretization> Manifold::createSubDiscretization(
    const string &name, const shared_ptr<Discretization> &parent_discretization,
    const shared_ptr<Discretization> &child_discretization,
    const vector<double> &factor, const vector<double> &offset) {
  auto subdiscretization =
      SubDiscretization::create(name, shared_from_this(), parent_discretization,
                                child_discretization, factor, offset);
  checked_emplace(subdiscretizations, subdiscretization->name,
                  subdiscretization);
  assert(subdiscretization->invariant());
  return subdiscretization;
}

shared_ptr<SubDiscretization>
Manifold::readSubDiscretization(const H5::CommonFG &loc, const string &entry) {
  auto subdiscretization =
      SubDiscretization::create(loc, entry, shared_from_this());
  checked_emplace(subdiscretizations, subdiscretization->name,
                  subdiscretization);
  assert(subdiscretization->invariant());
  return subdiscretization;
}
}
