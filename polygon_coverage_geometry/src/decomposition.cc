#include "polygon_coverage_geometry/bcd.h"
#include "polygon_coverage_geometry/decomposition.h"
#include "polygon_coverage_geometry/weakly_monotone.h"

#include <ros/assert.h>
#include <ros/console.h>

namespace polygon_coverage_planning {

std::vector<Direction_2> findEdgeDirections(const PolygonWithHoles& pwh) {
  // Get all possible polygon directions.
  std::vector<Direction_2> directions;
  for (size_t i = 0; i < pwh.outer_boundary().size(); ++i) {
    directions.push_back(pwh.outer_boundary().edge(i).direction());
  }
  for (PolygonWithHoles::Hole_const_iterator hit = pwh.holes_begin();
       hit != pwh.holes_end(); ++hit) {
    for (size_t i = 0; i < hit->size(); i++) {
      directions.push_back(hit->edge(i).direction());
    }
  }

  // Remove redundant directions.
  std::set<size_t> to_remove;
  for (size_t i = 0; i < directions.size() - 1; ++i) {
    for (size_t j = i + 1; j < directions.size(); ++j) {
      if (CGAL::orientation(directions[i].vector(), directions[j].vector()) ==
          CGAL::COLLINEAR)
        to_remove.insert(j);
    }
  }
  for (std::set<size_t>::reverse_iterator rit = to_remove.rbegin();
       rit != to_remove.rend(); ++rit) {
    directions.erase(std::next(directions.begin(), *rit));
  }

  // Add opposite directions.
  std::vector<Direction_2> temp_directions = directions;
  for (size_t i = 0; i < temp_directions.size(); ++i) {
    directions.push_back(-temp_directions[i]);
  }

  return directions;
}

std::vector<Direction_2> findPerpEdgeDirections(const PolygonWithHoles& pwh) {
  std::vector<Direction_2> directions = findEdgeDirections(pwh);
  for (auto& d : directions) {
    d = Direction_2(-d.dy(), d.dx());
  }

  return directions;
}

double findBestSweepDir(const Polygon_2& cell, Direction_2* best_dir) {
  // Get all sweepable edges.
  PolygonWithHoles pwh(cell);
  std::vector<Direction_2> edge_dirs = getAllSweepableEdgeDirections(cell);

  // Find minimum altitude.
  double min_altitude = std::numeric_limits<double>::max();
  for (const auto& dir : edge_dirs) {
    auto s = findSouth(cell, Line_2(Point_2(CGAL::ORIGIN), dir));
    auto n = findNorth(cell, Line_2(Point_2(CGAL::ORIGIN), dir));
    auto orthogonal_vec =
        dir.vector().perpendicular(CGAL::Orientation::POSITIVE);
    Line_2 line_through_n(*n, orthogonal_vec.direction());
    auto s_proj = line_through_n.projection(*s);
    double altitude =
        std::sqrt(CGAL::to_double(CGAL::squared_distance(*n, s_proj)));

    if (altitude < min_altitude) {
      min_altitude = altitude;
      if (best_dir) *best_dir = dir;
    }
  }

  return min_altitude;
}

bool computeBestBCDFromPolygonWithHoles(const PolygonWithHoles& pwh,
                                        std::vector<Polygon_2>* bcd_polygons) {
  {
    ROS_ASSERT(bcd_polygons);
    bcd_polygons->clear();
    double min_altitude_sum = std::numeric_limits<double>::max();

    // Get all possible decomposition directions.
    std::vector<Direction_2> directions = findPerpEdgeDirections(pwh);

    // For all possible rotations:
    for (const auto& dir : directions) {
      // Calculate decomposition.
      std::vector<Polygon_2> cells = computeBCD(pwh, dir);

      // Calculate minimum altitude sum for each cell.
      double min_altitude_sum_tmp = 0.0;
      for (const auto& cell : cells) {
        min_altitude_sum_tmp += findBestSweepDir(cell);
      }

      // Update best decomposition.
      if (min_altitude_sum_tmp < min_altitude_sum) {
        min_altitude_sum = min_altitude_sum_tmp;
        *bcd_polygons = cells;
      }
    }

    if (bcd_polygons->empty())
      return false;
    else
      return true;
  }
}

}  // namespace polygon_coverage_planning
