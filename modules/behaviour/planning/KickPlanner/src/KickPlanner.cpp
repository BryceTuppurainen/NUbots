/*
 * This file is part of NUbots Codebase.
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

#include "KickPlanner.h"

#include "utility/support/yaml_armadillo.h"
#include "utility/math/coordinates.h"
#include "utility/localisation/transform.h"
#include "messages/motion/WalkCommand.h"
#include "messages/localisation/FieldObject.h"
#include "messages/support/Configuration.h"
#include "messages/behaviour/Action.h"
#include "messages/behaviour/ServoCommand.h"
#include "messages/behaviour/KickPlan.h"
#include "messages/vision/VisionObjects.h"
#include "messages/support/FieldDescription.h"
#include "utility/nubugger/NUhelpers.h"



namespace modules {
namespace behaviour {
namespace planning {

    using utility::math::coordinates::sphericalToCartesian;
    using utility::localisation::transform::WorldToRobotTransform;
    using utility::localisation::transform::RobotToWorldTransform;
    using utility::nubugger::graph;

    using messages::localisation::Ball;
    using messages::localisation::Self;
    using messages::motion::KickCommand;
    using messages::motion::KickPlannerConfig;
    using messages::support::Configuration;
    using messages::motion::WalkStopCommand;
    using messages::input::LimbID;
    using messages::behaviour::KickPlan;
    using messages::support::FieldDescription;

    KickPlanner::KickPlanner(std::unique_ptr<NUClear::Environment> environment)
        : Reactor(std::move(environment)) {


        on<Trigger<Configuration<KickPlanner> > >([this](const Configuration<KickPlanner>& config) {
            cfg.max_ball_distance = config["max_ball_distance"].as<float>();
            cfg.kick_corridor_width = config["kick_corridor_width"].as<float>();
            cfg.seconds_not_seen_limit = config["seconds_not_seen_limit"].as<float>();
            emit(std::make_unique<KickPlannerConfig>(cfg));
        });

        on< Trigger<Ball>, 
            With<std::vector<Self>>,
            With<FieldDescription>,
            With<KickPlan> >([this] (
            const Ball& ball,
            const std::vector<Self>& selfs,
            const FieldDescription& fd,
            const KickPlan& kickPlan) {

            //Get time since last seen ball
            auto now = NUClear::clock::now();
            double secondsSinceLastSeen = std::chrono::duration_cast<std::chrono::microseconds>(now - ball.last_measurement_time).count() * 1e-6;
            
            //Compute target in robot coords
            auto self = selfs[0];
            arma::vec2 kickTarget = WorldToRobotTransform(self.position, self.heading, kickPlan.target);

            //Check whether to kick
            if(secondsSinceLastSeen < cfg.seconds_not_seen_limit &&
               ball.position[0] < cfg.max_ball_distance &&
               ball.position[0] > 0 &&
               std::fabs(ball.position[1]) < cfg.kick_corridor_width / 2){
               
                    emit(std::make_unique<WalkStopCommand>()); // Stop the walk
                    emit(std::make_unique<KickCommand>(KickCommand{{ball.position[0],ball.position[1],fd.ball_radius}, {kickTarget[0],kickTarget[1],0} }));
            }

        });
    }

}
}
}

