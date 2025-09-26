#include "openmc/source.h"

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#define HAS_DYNAMIC_LINKING
#endif

#include <algorithm> // for move

#ifdef HAS_DYNAMIC_LINKING
#include <dlfcn.h> // for dlopen, dlsym, dlclose, dlerror
#endif

#include "xtensor/xadapt.hpp"
#include <fmt/core.h>

#include "openmc/bank.h"
#include "openmc/capi.h"
#include "openmc/cell.h"
#include "openmc/container_util.h"
#include "openmc/error.h"
#include "openmc/file_utils.h"
#include "openmc/geometry.h"
#include "openmc/hdf5_interface.h"
#include "openmc/material.h"
#include "openmc/memory.h"
#include "openmc/message_passing.h"
#include "openmc/mgxs_interface.h"
#include "openmc/nuclide.h"
#include "openmc/random_lcg.h"
#include "openmc/search.h"
#include "openmc/settings.h"
#include "openmc/simulation.h"
#include "openmc/state_point.h"
#include "openmc/xml_interface.h"

namespace openmc {

//==============================================================================
// Global variables
//==============================================================================

namespace model {

vector<unique_ptr<Source>> external_sources;
}

//==============================================================================
// IndependentSource implementation
//==============================================================================

IndependentSource::IndependentSource(
  UPtrSpace space, UPtrAngle angle, UPtrDist energy, UPtrDist time)
  : space_ {std::move(space)}, angle_ {std::move(angle)},
    energy_ {std::move(energy)}, time_ {std::move(time)}
{}

IndependentSource::IndependentSource(pugi::xml_node node)
{
  // Check for particle type
  if (check_for_node(node, "particle")) {
    auto temp_str = get_node_value(node, "particle", true, true);
    if (temp_str == "neutron") {
      particle_ = ParticleType::neutron;
    } else if (temp_str == "photon") {
      particle_ = ParticleType::photon;
      settings::photon_transport = true;
    } else {
      fatal_error(std::string("Unknown source particle type: ") + temp_str);
    }
  }

  // Check for source strength
  if (check_for_node(node, "strength")) {
    strength_ = std::stod(get_node_value(node, "strength"));
  }

  // Check for external source file
  if (check_for_node(node, "file")) {

  } else {

    // Spatial distribution for external source
    if (check_for_node(node, "space")) {
      // Get pointer to spatial distribution
      pugi::xml_node node_space = node.child("space");

      // Check for type of spatial distribution and read
      std::string type;
      if (check_for_node(node_space, "type"))
        type = get_node_value(node_space, "type", true, true);
      if (type == "cartesian") {
        space_ = UPtrSpace {new CartesianIndependent(node_space)};
      } else if (type == "cylindrical") {
        space_ = UPtrSpace {new CylindricalIndependent(node_space)};
      } else if (type == "spherical") {
        space_ = UPtrSpace {new SphericalIndependent(node_space)};
      } else if (type == "box") {
        space_ = UPtrSpace {new SpatialBox(node_space)};
      } else if (type == "fission") {
        space_ = UPtrSpace {new SpatialBox(node_space, true)};
      } else if (type == "point") {
        space_ = UPtrSpace {new SpatialPoint(node_space)};
      } else {
        fatal_error(fmt::format(
          "Invalid spatial distribution for external source: {}", type));
      }

    } else {
      // If no spatial distribution specified, make it a point source
      space_ = UPtrSpace {new SpatialPoint()};
    }

    // Determine external source angular distribution
    if (check_for_node(node, "angle")) {
      // Get pointer to angular distribution
      pugi::xml_node node_angle = node.child("angle");

      // Check for type of angular distribution
      std::string type;
      if (check_for_node(node_angle, "type"))
        type = get_node_value(node_angle, "type", true, true);
      if (type == "isotropic") {
        angle_ = UPtrAngle {new Isotropic()};
      } else if (type == "monodirectional") {
        angle_ = UPtrAngle {new Monodirectional(node_angle)};
      } else if (type == "mu-phi") {
        angle_ = UPtrAngle {new PolarAzimuthal(node_angle)};
      } else {
        fatal_error(fmt::format(
          "Invalid angular distribution for external source: {}", type));
      }

    } else {
      angle_ = UPtrAngle {new Isotropic()};
    }

    // Determine external source energy distribution
    if (check_for_node(node, "energy")) {
      pugi::xml_node node_dist = node.child("energy");
      energy_ = distribution_from_xml(node_dist);
    } else {
      // Default to a Watt spectrum with parameters 0.988 MeV and 2.249 MeV^-1
      energy_ = UPtrDist {new Watt(0.988e6, 2.249e-6)};
    }

    // Determine external source time distribution
    if (check_for_node(node, "time")) {
      pugi::xml_node node_dist = node.child("time");
      time_ = distribution_from_xml(node_dist);
    } else {
      // Default to a Constant time T=0
      double T[] {0.0};
      double p[] {1.0};
      time_ = UPtrDist {new Discrete {T, p, 1}};
    }

    // Check for domains to reject from
    if (check_for_node(node, "domain_type")) {
      std::string domain_type = get_node_value(node, "domain_type");
      if (domain_type == "cell") {
        domain_type_ = DomainType::CELL;
      } else if (domain_type == "material") {
        domain_type_ = DomainType::MATERIAL;
      } else if (domain_type == "universe") {
        domain_type_ = DomainType::UNIVERSE;
      } else {
        fatal_error(std::string(
          "Unrecognized domain type for source rejection: " + domain_type));
      }

      auto ids = get_node_array<int>(node, "domain_ids");
      domain_ids_.insert(ids.begin(), ids.end());
    }
  }
}

SourceSite IndependentSource::sample(uint64_t* seed) const
{
  SourceSite site;
  site.particle = particle_;

  bool found = false;
  int n_reject = 0;
  static int n_accept = 0;

  // helper: derive material index from coord stack (deepest MATERIAL cell)
  auto mat_index_from_coord = [](const Particle& pp) -> int {
    int level_mat = -1;
    for (int lvl = pp.n_coord() - 1; lvl >= 0; --lvl) {
      int ci = pp.coord(lvl).cell;
      if (ci != C_NONE && model::cells[ci]->type_ == Fill::MATERIAL) { level_mat = lvl; break; }
    }
    if (level_mat < 0) return MATERIAL_VOID;

    const Cell& cell = *model::cells[pp.coord(level_mat).cell];
    int inst = 0;
    if (cell.distribcell_index_ != C_NONE &&
        (cell.material_.size() > 1 || cell.sqrtkT_.size() > 1) &&
        level_mat > 0 && pp.coord(level_mat - 1).cell != C_NONE) {
      inst = cell_instance_at_level(const_cast<Particle&>(pp), level_mat);
    }
    if (cell.material_.empty()) return MATERIAL_VOID;
    return cell.material_[cell.material_.size() > 1 ? inst : 0];
  };

  while (!found) {
    // probe particle for geometry
    Particle p;
    p.clear();                      // ensure clean state
    p.type() = particle_;
    p.u() = {0.0, 0.0, 1.0};        // dir unused, but set anyway

    // seed root coord frame explicitly
    if (p.n_coord() == 0) p.coord().resize(1);
    auto& lc0 = p.coord(0);
    lc0.cell      = C_NONE;
    lc0.universe  = (model::universe_map.count(model::root_universe)
                      ? model::universe_map.at(model::root_universe)
                      : model::root_universe); // root as INDEX
    lc0.lattice   = C_NONE;
    lc0.lattice_i = {{-1, -1, -1}};

    // sample position and locate geometry
    p.r() = space_->sample(seed);
    found = exhaustive_find_cell(p);

    if (found) {
      // fission-only gate for <space type="fission">
      if (auto* sb = dynamic_cast<SpatialBox*>(space_.get());
          sb && sb->only_fissionable()) {
        int mi = mat_index_from_coord(p);
        bool is_fiss = (mi != MATERIAL_VOID) &&
                       (mi >= 0 && mi < (int)model::materials.size()) &&
                       model::materials[mi] && model::materials[mi]->fissionable_;
        found = found && is_fiss;
      }

      // domain filters (materials / cells / universes)
      if (found && !domain_ids_.empty()) {
        bool in_domain = false;
        if (domain_type_ == DomainType::MATERIAL) {
          int mi = mat_index_from_coord(p); // robust against unset p.material()
          if (mi != MATERIAL_VOID &&
              mi >= 0 && mi < (int)model::materials.size() &&
              model::materials[mi]) {
            in_domain = contains(domain_ids_, model::materials[mi]->id());
          }
        } else {
          for (const auto& c : p.coord()) {
            if (c.cell == C_NONE || c.universe == C_NONE) continue;
            int id = (domain_type_ == DomainType::CELL)
                       ? model::cells[c.cell]->id_
                       : model::universes[c.universe]->id_;
            if (contains(domain_ids_, id)) { in_domain = true; break; }
          }
        }
        found = found && in_domain;
      }
    }

    if (!found) {
      ++n_reject;
      if (n_reject >= EXTSRC_REJECT_THRESHOLD &&
          static_cast<double>(n_accept) / n_reject <= EXTSRC_REJECT_FRACTION) {
        fatal_error("More than 95% of external source sites sampled were "
                    "rejected. Please check your external source's spatial "
                    "definition.");
      }
      continue; // resample
    }

    // accept: commit position and leave loop
    site.r = p.r();
    ++n_accept;
    break;
  }

  // Sample angle
  site.u = angle_->sample(seed);

  // Check monoenergetic energies against max
  auto ptype_idx = static_cast<int>(particle_);
  if (auto* energy_disc = dynamic_cast<Discrete*>(energy_.get())) {
    auto energies = xt::adapt(energy_disc->x());
    if (xt::any(energies > data::energy_max[ptype_idx])) {
      fatal_error("Source energy above range of energies of at least "
                  "one cross section table");
    }
  }

  // Sample energy (with max-energy guard)
  while (true) {
    site.E = energy_->sample(seed);
    if (site.E < data::energy_max[ptype_idx]) break;

    ++n_reject;
    if (n_reject >= EXTSRC_REJECT_THRESHOLD &&
        static_cast<double>(n_accept) / n_reject <= EXTSRC_REJECT_FRACTION) {
      fatal_error("More than 95% of external source sites sampled were "
                  "rejected. Please check your external source energy spectrum "
                  "definition.");
    }
  }

  // Sample particle creation time
  site.time = time_->sample(seed);

  #ifdef OPENMC_DEBUG_FISSION_SOURCE
  if (auto* sb = dynamic_cast<SpatialBox*>(space_.get()); sb && sb->only_fissionable()) {
    // Re-check that the RETURNED site is in fissionable material
    Particle q;
    q.clear();
    if (q.n_coord() == 0) q.coord().resize(1);
    auto& lq = q.coord(0);
    lq.cell = C_NONE;
    lq.universe = (model::universe_map.count(model::root_universe)
                    ? model::universe_map.at(model::root_universe)
                    : model::root_universe); // index
    lq.lattice = C_NONE; lq.lattice_i = {{-1,-1,-1}};
    q.r() = site.r;

    bool found2 = exhaustive_find_cell(q);
    bool fiss_ok = false;
    if (found2) {
      int mi2 = mat_index_from_coord(q);
      fiss_ok = (mi2 != MATERIAL_VOID) &&
                mi2 >= 0 && mi2 < (int)model::materials.size() &&
                model::materials[mi2] && model::materials[mi2]->fissionable_;
    }
    if (!fiss_ok) {
      fatal_error("Invariant violated: fission spatial source returned a non-fissionable site.");
    }
  }
  #endif

  return site;
}

//==============================================================================
// FileSource implementation
//==============================================================================

FileSource::FileSource(std::string path)
{
  // Check if source file exists
  if (!file_exists(path)) {
    fatal_error(fmt::format("Source file '{}' does not exist.", path));
  }

  // Read the source from a binary file instead of sampling from some
  // assumed source distribution
  write_message(6, "Reading source file from {}...", path);

  // Open the binary file
  hid_t file_id = file_open(path, 'r', true);

  // Check to make sure this is a source file
  std::string filetype;
  read_attribute(file_id, "filetype", filetype);
  if (filetype != "source" && filetype != "statepoint") {
    fatal_error("Specified starting source file not a source file type.");
  }

  // Read in the source particles
  read_source_bank(file_id, sites_, false);

  // Close file
  file_close(file_id);
}

SourceSite FileSource::sample(uint64_t* seed) const
{
  size_t i_site = sites_.size() * prn(seed);
  return sites_[i_site];
}

//==============================================================================
// CustomSourceWrapper implementation
//==============================================================================

CustomSourceWrapper::CustomSourceWrapper(
  std::string path, std::string parameters)
{
#ifdef HAS_DYNAMIC_LINKING
  // Open the library
  shared_library_ = dlopen(path.c_str(), RTLD_LAZY);
  if (!shared_library_) {
    fatal_error("Couldn't open source library " + path);
  }

  // reset errors
  dlerror();

  // get the function to create the custom source from the library
  auto create_custom_source = reinterpret_cast<create_custom_source_t*>(
    dlsym(shared_library_, "openmc_create_source"));

  // check for any dlsym errors
  auto dlsym_error = dlerror();
  if (dlsym_error) {
    std::string error_msg = fmt::format(
      "Couldn't open the openmc_create_source symbol: {}", dlsym_error);
    dlclose(shared_library_);
    fatal_error(error_msg);
  }

  // create a pointer to an instance of the custom source
  custom_source_ = create_custom_source(parameters);

#else
  fatal_error("Custom source libraries have not yet been implemented for "
              "non-POSIX systems");
#endif
}

CustomSourceWrapper::~CustomSourceWrapper()
{
  // Make sure custom source is cleared before closing shared library
  if (custom_source_.get())
    custom_source_.reset();

#ifdef HAS_DYNAMIC_LINKING
  dlclose(shared_library_);
#else
  fatal_error("Custom source libraries have not yet been implemented for "
              "non-POSIX systems");
#endif
}

//==============================================================================
// Non-member functions
//==============================================================================

void initialize_source()
{
  write_message("Initializing source particles...", 5);

// Generation source sites from specified distribution in user input
#pragma omp parallel for
  for (int64_t i = 0; i < simulation::work_per_rank; ++i) {
    // initialize random number seed
    int64_t id = simulation::total_gen * settings::n_particles +
                 simulation::work_index[mpi::rank] + i + 1;
    uint64_t seed = init_seed(id, STREAM_SOURCE);

    // sample external source distribution
    simulation::source_bank[i] = sample_external_source(&seed);
  }

  // Write out initial source
  if (settings::write_initial_source) {
    write_message("Writing out initial source...", 5);
    std::string filename = settings::path_output + "initial_source.h5";
    hid_t file_id = file_open(filename, 'w', true);
    write_source_bank(file_id, false);
    file_close(file_id);
  }
}

SourceSite sample_external_source(uint64_t* seed)
{
  // Determine total source strength
  double total_strength = 0.0;
  for (auto& s : model::external_sources)
    total_strength += s->strength();

  // Sample from among multiple source distributions
  int i = 0;
  if (model::external_sources.size() > 1) {
    double xi = prn(seed) * total_strength;
    double c = 0.0;
    for (; i < model::external_sources.size(); ++i) {
      c += model::external_sources[i]->strength();
      if (xi < c)
        break;
    }
  }

  // Sample source site from i-th source distribution
  SourceSite site {model::external_sources[i]->sample(seed)};

  // If running in MG, convert site.E to group
  if (!settings::run_CE) {
    site.E = lower_bound_index(data::mg.rev_energy_bins_.begin(),
      data::mg.rev_energy_bins_.end(), site.E);
    site.E = data::mg.num_energy_groups_ - site.E - 1.;
  }

  return site;
}

void free_memory_source()
{
  model::external_sources.clear();
}

//==============================================================================
// C API
//==============================================================================

extern "C" int openmc_sample_external_source(
  size_t n, uint64_t* seed, void* sites)
{
  if (!sites || !seed) {
    set_errmsg("Received null pointer.");
    return OPENMC_E_INVALID_ARGUMENT;
  }

  auto sites_array = static_cast<SourceSite*>(sites);
  for (size_t i = 0; i < n; ++i) {
    sites_array[i] = sample_external_source(seed);
  }
  return 0;
}

} // namespace openmc
