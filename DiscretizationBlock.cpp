#include "DiscretizationBlock.hpp"

#include "H5Helpers.hpp"

#include <algorithm>

namespace SimulationIO {

namespace {
template <int D>
void read_active(const H5::H5Location &group,
                 const DiscretizationBlock &discretizationblock,
                 region_t &active) {
  // The case D==0 is not yet handled correctly:
  // - C++ pads empty struct
  // - HDF5 cannot handle empty arrays
  static_assert(D > 0, "");
  if (active.valid())
    return;
  vector<RegionCalculus::box<hssize_t, D>> boxes;
  const auto &boxtype = discretizationblock.discretization.lock()
                            ->manifold.lock()
                            ->project.lock()
                            ->boxtypes.at(D);
  assert(sizeof(boxes[0]) == boxtype.getSize());
#if 0
  H5E_auto2_t func;
  void *client_data;
  H5::Exception::getAutoPrint(func, &client_data);
  H5::Exception::dontPrint();
  try {
    H5::readAttribute(group, "active", boxes, boxtype);
    active = region_t(
        RegionCalculus::make_unique<RegionCalculus::wregion<hssize_t, D>>(
            RegionCalculus::region<hssize_t, D>(std::move(boxes))));
  } catch (H5::AttributeIException ex) {
    // do nothing
  }
  H5::Exception::setAutoPrint(func, client_data);
#endif
  if (group.attrExists("active")) {
    H5::readAttribute(group, "active", boxes, boxtype);
    active = region_t(
        RegionCalculus::make_unique<RegionCalculus::wregion<hssize_t, D>>(
            RegionCalculus::region<hssize_t, D>(std::move(boxes))));
  }
}
}

void DiscretizationBlock::read(
    const H5::CommonFG &loc, const string &entry,
    const shared_ptr<Discretization> &discretization) {
  this->discretization = discretization;
  auto group = loc.openGroup(entry);
  assert(H5::readAttribute<string>(
             group, "type",
             discretization->manifold.lock()->project.lock()->enumtype) ==
         "DiscretizationBlock");
  H5::readAttribute(group, "name", name);
  assert(H5::readGroupAttribute<string>(group, "discretization", "name") ==
         discretization->name);
  if (group.attrExists("offset")) {
    vector<hssize_t> offset, shape;
    H5::readAttribute(group, "offset", offset);
    std::reverse(offset.begin(), offset.end());
    H5::readAttribute(group, "shape", shape);
    std::reverse(shape.begin(), shape.end());
    region = box_t(offset, point_t(offset) + shape);
  }
  if (group.attrExists("active")) {
    // TODO read_active<0>(group, *this, active);
    read_active<1>(group, *this, active);
    read_active<2>(group, *this, active);
    read_active<3>(group, *this, active);
    read_active<4>(group, *this, active);
  }
}

ostream &DiscretizationBlock::output(ostream &os, int level) const {
  os << indent(level) << "DiscretizationBlock " << quote(name)
     << ": Discretization " << quote(discretization.lock()->name);
  if (region.valid())
    os << " region=" << region;
  if (active.valid())
    os << " active=" << active;
  os << "\n";
  return os;
}

namespace {
template <int D>
void write_active(const H5::H5Location &group,
                  const DiscretizationBlock &discretizationblock,
                  const region_t &active) {
  // The case D==0 is not yet handled correctly:
  // - C++ pads empty struct
  // - HDF5 cannot handle empty arrays
  static_assert(D > 0, "");
  if (active.rank() != D)
    return;
  const auto &boxes =
      dynamic_cast<const RegionCalculus::wregion<hssize_t, D> *>(
          active.val.get())
          ->val.boxes;
  const auto &boxtype = discretizationblock.discretization.lock()
                            ->manifold.lock()
                            ->project.lock()
                            ->boxtypes.at(D);
  assert(sizeof(boxes[0]) == boxtype.getSize());
  H5::createAttribute(group, "active", boxes, boxtype);
}
}

void DiscretizationBlock::write(const H5::CommonFG &loc,
                                const H5::H5Location &parent) const {
  assert(invariant());
  auto group = loc.createGroup(name);
  H5::createAttribute(
      group, "type",
      discretization.lock()->manifold.lock()->project.lock()->enumtype,
      "DiscretizationBlock");
  H5::createAttribute(group, "name", name);
  H5::createHardLink(group, "discretization", parent, ".");
  if (region.valid()) {
#warning "TODO: write using boxtype HDF5 type"
    vector<hssize_t> offset = region.lower(), shape = region.shape();
    std::reverse(offset.begin(), offset.end());
    H5::createAttribute(group, "offset", offset);
    std::reverse(shape.begin(), shape.end());
    H5::createAttribute(group, "shape", shape);
  }
  if (active.valid()) {
    // TODO write_active<0>(group, *this, active);
    write_active<1>(group, *this, active);
    write_active<2>(group, *this, active);
    write_active<3>(group, *this, active);
    write_active<4>(group, *this, active);
  }
}
}
