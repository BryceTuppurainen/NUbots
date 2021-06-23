/*
 * This file is part of the NUbots Codebase.
 *
 * The NUbots Codebase is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The NUbots Codebase is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the NUbots Codebase.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2013 NUBots <nubots@nubots.net>
 */

#ifndef MODULES_LOCALISATION_ROBOTMODEL_HPP
#define MODULES_LOCALISATION_ROBOTMODEL_HPP

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <nuclear>
#include <random>
#include <vector>

#include "message/input/Sensors.hpp"
#include "message/support/FieldDescription.hpp"
#include "message/vision/Goal.hpp"

#include "utility/input/ServoID.hpp"
#include "utility/localisation/transform.hpp"
#include "utility/math/angle.hpp"
#include "utility/math/coordinates.hpp"

namespace module::localisation {

    using message::input::Sensors;
    using message::support::FieldDescription;
    using message::vision::Goal;
    using utility::input::ServoID;
    using utility::localisation::fieldStateToTransform3D;
    using utility::math::angle::normalizeAngle;
    using utility::math::coordinates::cartesianToSpherical;

    template <typename Scalar>
    class RobotModel {
    public:
        static constexpr size_t size = 3;

        using StateVec = Eigen::Matrix<Scalar, size, 1>;
        using StateMat = Eigen::Matrix<Scalar, size, size>;


        enum Components : int {
            // Field center in world space
            kX = 0,
            kY = 1,
            // Angle is the angle from the robot world forward direction to the field forward direction
            kAngle = 2
        };

        // Local variables for this model. Set their values from config file
        // Number of reset particles
        int n_rogues = 0;
        // Number of particles
        int n_particles = 100;
        // Range of reset particles
        Eigen::Matrix<Scalar, 3, 1> resetRange;
        // Diagonal for 3x3 noise matrix (which is diagonal)
        Eigen::Matrix<Scalar, 3, 1> processNoiseDiagonal;

        RobotModel()
            : n_rogues(0)
            , resetRange(Eigen::Matrix<Scalar, 3, 1>::Zero())
            , processNoiseDiagonal(Eigen::Matrix<Scalar, 3, 1>::Ones()) {}

        StateVec time(const StateVec& state, double /*deltaT*/) {
            return state;
        }

        Eigen::Matrix<Scalar, Eigen::Dynamic, 1> predict(const StateVec& state,
                                                         const Eigen::Matrix<Scalar, 3, 1>& actual_position,
                                                         const Eigen::Matrix<Scalar, 4, 4>& Hcw,
                                                         const message::vision::Goal::MeasurementType& type,
                                                         const message::support::FieldDescription& fd) {

            // Create a transform from the field state
            Eigen::Transform<Scalar, 3, Eigen::Affine> Hfw;
            Hfw.translation() = Eigen::Matrix<Scalar, 3, 1>(state.x(), state.y(), 0);
            Hfw.linear() = Eigen::AngleAxis<Scalar>(state.z(), Eigen::Matrix<Scalar, 3, 1>::UnitZ()).toRotationMatrix();

            const Eigen::Transform<Scalar, 3, Eigen::Affine> Hcf(Hcw * Hfw.inverse().matrix());

            if (type == Goal::MeasurementType::CENTRE) {
                const Eigen::Matrix<Scalar, 3, 1> rGCc(Hcf * actual_position);
                return cartesianToSpherical(rGCc);
            }

            switch (FieldDescription::GoalpostType::Value(fd.dimensions.goalpost_type)) {
                case FieldDescription::GoalpostType::CIRCLE: {
                    if (type == Goal::MeasurementType::LEFT_NORMAL || type == Goal::MeasurementType::RIGHT_NORMAL) {
                        const Eigen::Matrix<Scalar, 3, 1> rNCc(
                            getCylindricalPostCamSpaceNormal(type, actual_position, Hcf, fd));
                        const Eigen::Matrix<Scalar, 2, 1> angles(
                            std::atan2(rNCc.y(), rNCc.x()),
                            std::atan2(rNCc.z(), std::sqrt(rNCc.x() * rNCc.x() + rNCc.y() * rNCc.y())));
                        return angles;
                    }
                    break;
                }
                case FieldDescription::GoalpostType::RECTANGLE: {
                    if (type == Goal::MeasurementType::LEFT_NORMAL || type == Goal::MeasurementType::RIGHT_NORMAL) {
                        const Eigen::Matrix<Scalar, 3, 1> rNCc(
                            getSquarePostCamSpaceNormal(type, actual_position, Hcf, fd));
                        const Eigen::Matrix<Scalar, 2, 1> angles(
                            std::atan2(rNCc.y(), rNCc.x()),
                            std::atan2(rNCc.z(), std::sqrt(rNCc.x() * rNCc.x() + rNCc.y() * rNCc.y())));
                        return angles;
                    }
                    break;
                }
            }
            return Eigen::Matrix<Scalar, 2, 1>::Zero();
        }

        std::vector<Eigen::Matrix<Scalar, Eigen::Dynamic, 1>> predict(const StateVec& state,
                                                                      const Eigen::Matrix<Scalar, 4, 4>& Hcw,
                                                                      const message::support::FieldDescription& fd,
                                                                      const int& num_field_points) {
            // We want to uniformly sample points inside the field line width
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<Scalar> dis(-fd.dimensions.line_width * Scalar(0.5),
                                                       fd.dimensions.line_width * Scalar(0.5));

            // Cache needed field dimension to improve readability
            const Scalar field_length           = fd.dimensions.field_length;
            const Scalar field_width            = fd.dimensions.field_width;
            const Scalar half_field_length      = fd.dimensions.field_length * Scalar(0.5);
            const Scalar half_field_width       = fd.dimensions.field_width * Scalar(0.5);
            const Scalar line_width             = fd.dimensions.line_width;
            const Scalar goal_length            = fd.dimensions.goal_area_length;
            const Scalar goal_width             = fd.dimensions.goal_area_width;
            const Scalar penalty_length         = fd.dimensions.penalty_area_length;
            const Scalar penalty_width          = fd.dimensions.penalty_area_width;
            const Scalar penalty_mark           = fd.dimensions.penalty_mark_distance;
            const Scalar center_circle_diameter = fd.dimensions.center_circle_diameter;

            // Total length of all field lines on the field in metres
            const Scalar field_line_length = field_length * 2                       // Left and right lines
                                             + field_width * 3                      // Top, mid-field, and bottom lines
                                             + line_width * 10                      // Top and bottom penalty marks
                                             + line_width * 2                       // Center mark
                                             + goal_length * 4                      // Top and bottom goal areas
                                             + goal_width * 2 + penalty_length * 4  // Top and bottom penalty areas
                                             + penalty_width * 2
                                             + center_circle_diameter * M_PI;  // Center circle circumference

            // Number of points to sample per meter
            const Scalar points_per_meter = field_line_length / Scalar(num_field_points);
            const Scalar meters_per_point = Scalar(num_field_points) / field_line_length;

            std::vector<Eigen::Matrix<Scalar, 3, 1>> rLFf_truths;

            // Sample all of field points on the field lines
            // Top line
            // Bottom line
            // Mid-field line
            int num_points = field_width * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Top line
                rLFf_truths.emplace_back(dis(gen) + half_field_length,
                                         -half_field_width + point * meters_per_point,
                                         Scalar(0));
                // Mid-field line
                rLFf_truths.emplace_back(dis(gen), -half_field_width + point * meters_per_point, Scalar(0));
                // Bottom line
                rLFf_truths.emplace_back(dis(gen) - half_field_length,
                                         -half_field_width + point * meters_per_point,
                                         Scalar(0));
            }

            // Left line
            // Right line
            num_points = field_length * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Left line
                rLFf_truths.emplace_back(-half_field_length + point * meters_per_point,
                                         dis(gen) + half_field_width,
                                         Scalar(0));
                // Right line
                rLFf_truths.emplace_back(-half_field_length + point * meters_per_point,
                                         dis(gen) - half_field_width,
                                         Scalar(0));
            }

            // Center circle
            num_points = M_PI * center_circle_diameter * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Calculate point in polar coordinates
                // Evenly divide the circle into num_points wedges
                const Scalar radius = dis(gen) + center_circle_diameter * Scalar(0.5);
                const Scalar theta  = 2.0 * M_PI * point / num_points;

                // Convert to cartesian coordinates
                rLFf_truths.emplace_back(radius * std::cos(theta), radius * std::sin(theta), Scalar(0));
            }

            // Top penalty mark side to side
            // Bottom penalty mark side to side
            num_points = line_width * 3 * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Top mark
                rLFf_truths.emplace_back(half_field_length - penalty_mark + dis(gen),
                                         line_width - point * meters_per_point,
                                         Scalar(0));
                // Bottom mark
                rLFf_truths.emplace_back(-half_field_length + penalty_mark + dis(gen),
                                         line_width - point * meters_per_point,
                                         Scalar(0));
            }

            // Top penalty mark end to end
            // Center mark end to end
            // Bottom penalty mark end to end
            num_points = line_width * 3 * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Top mark
                rLFf_truths.emplace_back(half_field_length - penalty_mark - line_width + point * meters_per_point,
                                         dis(gen),
                                         Scalar(0));
                // Center mark
                rLFf_truths.emplace_back(-line_width + point * meters_per_point, dis(gen), Scalar(0));
                // Bottom mark
                rLFf_truths.emplace_back(-half_field_length + penalty_mark - line_width + point * meters_per_point,
                                         dis(gen),
                                         Scalar(0));
            }

            // Top goal box left and right lines
            // Bottom goal box left and right lines
            num_points = goal_length * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Top Left line
                rLFf_truths.emplace_back(half_field_length - goal_length * point * meters_per_point,
                                         goal_width * Scalar(0.5) + dis(gen),
                                         Scalar(0));
                // Top Right line
                rLFf_truths.emplace_back(half_field_length - goal_length * point * meters_per_point,
                                         -goal_width * Scalar(0.5) + dis(gen),
                                         Scalar(0));

                // Bottom Left line
                rLFf_truths.emplace_back(-half_field_length + goal_length * point * meters_per_point,
                                         goal_width * Scalar(0.5) + dis(gen),
                                         Scalar(0));
                // Bottom Right line
                rLFf_truths.emplace_back(-half_field_length + goal_length * point * meters_per_point,
                                         -goal_width * Scalar(0.5) + dis(gen),
                                         Scalar(0));
            }

            // Top goal box top/bottom lines
            // Bottom goal box top/bottom lines
            num_points = goal_width * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Top line
                rLFf_truths.emplace_back(half_field_length - goal_length + dis(gen),
                                         -goal_width * Scalar(0.5) + point * meters_per_point,
                                         Scalar(0));
                // Bottom line
                rLFf_truths.emplace_back(-half_field_length + goal_length + dis(gen),
                                         -goal_width * Scalar(0.5) + point * meters_per_point,
                                         Scalar(0));
            }

            // Top penalty box left and right lines
            // Bottom penalty box left and right lines
            num_points = penalty_length * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Top Left line
                rLFf_truths.emplace_back(half_field_length - penalty_length + point * meters_per_point,
                                         penalty_width * Scalar(0.5) + dis(gen),
                                         Scalar(0));
                // Top Right line
                rLFf_truths.emplace_back(half_field_length - penalty_length + point * meters_per_point,
                                         -penalty_width * Scalar(0.5) + dis(gen),
                                         Scalar(0));

                // Bottom Left line
                rLFf_truths.emplace_back(-half_field_length + point * meters_per_point,
                                         penalty_width * Scalar(0.5) + dis(gen),
                                         Scalar(0));
                // Bottom Right line
                rLFf_truths.emplace_back(-half_field_length + point * meters_per_point,
                                         -penalty_width * Scalar(0.5) + dis(gen),
                                         Scalar(0));
            }

            // Top penalty box top/bottom lines
            // Bottom penalty box top/bottom lines
            num_points = penalty_width * points_per_meter;
            for (int point = 0; point < num_points; ++point) {
                // Top line
                rLFf_truths.emplace_back(half_field_length - penalty_length + dis(gen),
                                         -penalty_width * Scalar(0.5) + point * meters_per_point,
                                         Scalar(0));
                // Bottom line
                rLFf_truths.emplace_back(-half_field_length + penalty_length + dis(gen),
                                         -penalty_width * Scalar(0.5) + point * meters_per_point,
                                         Scalar(0));
            }

            // Create a transform from the field state
            Eigen::Transform<Scalar, 3, Eigen::Affine> Hfw;
            Hfw.translation() = Eigen::Matrix<Scalar, 3, 1>(state.x(), state.y(), 0);
            Hfw.linear() = Eigen::AngleAxis<Scalar>(state.z(), Eigen::Matrix<Scalar, 3, 1>::UnitZ()).toRotationMatrix();

            const Eigen::Transform<Scalar, 3, Eigen::Affine> Hcf(Hcw * Hfw.inverse().matrix());

            // Transform all field coordinates into camera coordinates
            for (auto& rLFf : rLFf_truths) {
                rLFf = cartesianToSpherical(Hcf * rLFf);
            }

            // TODO Cull points based on FOV
            return rLFf_truths;
        }

        StateVec limit(const StateVec& state) {
            return state;
        }

        StateMat noise(const Scalar& deltaT) {
            return processNoiseDiagonal.asDiagonal() * deltaT;
        }

        template <typename T, typename U>
        static auto difference(const T& a, const U& b) {
            return a - b;
        }

        template <typename T>
        std::vector<Eigen::Matrix<Scalar, Eigen::Dynamic, 1>> difference(
            const std::vector<Eigen::Matrix<Scalar, Eigen::Dynamic, 1>>& a,
            const T& b) {
            std::vector<Eigen::Matrix<Scalar, Eigen::Dynamic, 1>> o{};
            for (auto& i : a) {
                o.emplace_back(Eigen::Matrix<Scalar, Eigen::Dynamic, 1>(i.x() - b.x(), i.y() - b.y()));
            }
            return o;
        }

        // Getters
        int getRogueCount() const {
            return n_rogues;
        }
        Eigen::Matrix<Scalar, Eigen::Dynamic, 1> getRogueRange() const {
            return resetRange;
        }

        [[nodiscard]] int getParticleCount() const {
            return n_particles;
        }

        Eigen::Matrix<Scalar, 3, 1> getCylindricalPostCamSpaceNormal(
            const message::vision::Goal::MeasurementType& type,
            const Eigen::Matrix<Scalar, 3, 1>& post_centre,
            const Eigen::Transform<Scalar, 3, Eigen::Affine>& Hcf,
            const message::support::FieldDescription& fd) {
            if (!(type == Goal::MeasurementType::LEFT_NORMAL || type == Goal::MeasurementType::RIGHT_NORMAL))
                return Eigen::Matrix<Scalar, 3, 1>::Zero();
            // rZFf = field vertical
            const Eigen::Matrix<Scalar, 4, 1> rZFf = Hcf * Eigen::Matrix<Scalar, 4, 1>::UnitZ();

            const Eigen::Matrix<Scalar, 3, 1> rZCc(rZFf.template head<3>());

            // The vector direction across the field perpendicular to the camera view vector
            const Eigen::Matrix<Scalar, 3, 1> rLRf = rZCc.cross(Eigen::Matrix<Scalar, 3, 1>::UnitX()).normalized();

            const float dir                           = (type == Goal::MeasurementType::LEFT_NORMAL) ? 1.0f : -1.0f;
            const Eigen::Matrix<Scalar, 3, 1> rG_blCc = post_centre + 0.5 * dir * fd.dimensions.goalpost_width * rLRf;
            const Eigen::Matrix<Scalar, 3, 1> rG_tlCc = rG_blCc + fd.dimensions.goal_crossbar_height * rZCc;

            // creating the normal vector (following convention stipulated in VisionObjects)
            return (type == Goal::MeasurementType::LEFT_NORMAL) ? rG_blCc.cross(rG_tlCc).normalized()
                                                                : rG_tlCc.cross(rG_blCc).normalized();
        }

        Eigen::Matrix<Scalar, 3, 1> getSquarePostCamSpaceNormal(const message::vision::Goal::MeasurementType& type,
                                                                const Eigen::Matrix<Scalar, 3, 1>& post_centre,
                                                                const Eigen::Transform<Scalar, 3, Eigen::Affine>& Hcf,
                                                                const message::support::FieldDescription& fd) {
            if (!(type == Goal::MeasurementType::LEFT_NORMAL || type == Goal::MeasurementType::RIGHT_NORMAL))
                return Eigen::Matrix<Scalar, 3, 1>::Zero();

            // Finding 4 corners of goalpost and centre (4 corners and centre)
            Eigen::Matrix<Scalar, 4, 5> goalBaseCorners;
            goalBaseCorners.colwise() =
                Eigen::Matrix<Scalar, 4, 1>(post_centre.x(), post_centre.y(), post_centre.z(), 1);
            //
            goalBaseCorners.col(1) += Eigen::Matrix<Scalar, 4, 1>(0.5 * fd.dimensions.goalpost_depth,
                                                                  0.5 * fd.dimensions.goalpost_width,
                                                                  0,
                                                                  0);
            goalBaseCorners.col(2) += Eigen::Matrix<Scalar, 4, 1>(0.5 * fd.dimensions.goalpost_depth,
                                                                  -0.5 * fd.dimensions.goalpost_width,
                                                                  0,
                                                                  0);
            goalBaseCorners.col(3) += Eigen::Matrix<Scalar, 4, 1>(-0.5 * fd.dimensions.goalpost_depth,
                                                                  0.5 * fd.dimensions.goalpost_width,
                                                                  0,
                                                                  0);
            goalBaseCorners.col(4) += Eigen::Matrix<Scalar, 4, 1>(-0.5 * fd.dimensions.goalpost_depth,
                                                                  -0.5 * fd.dimensions.goalpost_width,
                                                                  0,
                                                                  0);

            Eigen::Matrix<Scalar, 4, 5> goalTopCorners = goalBaseCorners;
            goalTopCorners.colwise() += Eigen::Matrix<Scalar, 4, 1>(0, 0, fd.dimensions.goal_crossbar_height, 0);

            // Transform to robot camera space
            const Eigen::Matrix<Scalar, 4, 5> goalBaseCornersCam = Hcf * goalBaseCorners;
            const Eigen::Matrix<Scalar, 4, 5> goalTopCornersCam  = Hcf * goalTopCorners;

            // Get widest lines
            Eigen::Matrix<Scalar, 3, 1> widestBase(goalBaseCornersCam.col(0).template head<3>());
            Eigen::Matrix<Scalar, 3, 1> widestTop(goalTopCornersCam.col(0).template head<3>());
            float largest_angle = 0;

            for (int i = 1; i < goalBaseCornersCam.cols(); ++i) {
                const Eigen::Matrix<Scalar, 4, 1> baseCorner(goalBaseCornersCam.col(i));
                const Eigen::Matrix<Scalar, 3, 1> baseCorner3(baseCorner.template head<3>());
                const Eigen::Matrix<Scalar, 4, 1> topCorner(goalTopCornersCam.col(i));
                const float angle(std::acos(baseCorner.dot(goalBaseCornersCam.col(0))));

                // Left side will have cross product point in neg field z direction
                const Eigen::Matrix<Scalar, 3, 1> goalBaseCorner0(goalBaseCornersCam.col(0).template head<3>());
                const Eigen::Matrix<Scalar, 3, 1> crossResult(baseCorner3.cross(goalBaseCorner0));
                const Eigen::Matrix<Scalar, 3, 1> topBaseDifference((topCorner - baseCorner).template head<3>());
                const bool left_side = crossResult.dot(topBaseDifference) < 0;

                // If its the largest angle so far for that side, then update our results so far
                if ((left_side && (type == Goal::MeasurementType::LEFT_NORMAL))
                    || (!left_side && (type == Goal::MeasurementType::RIGHT_NORMAL))) {
                    if (angle > largest_angle) {
                        widestBase    = goalBaseCornersCam.col(i).template head<3>();
                        widestTop     = goalTopCornersCam.col(i).template head<3>();
                        largest_angle = angle;
                    }
                }
            }

            // Returning the normal vector (following convention stipulated in VisionObjects)
            // Normals point into the goal centre
            return (type == Goal::MeasurementType::LEFT_NORMAL) ? (widestBase.cross(widestTop)).normalized()
                                                                : (widestTop.cross(widestBase)).normalized();
        }
    };
}  // namespace module::localisation
#endif
