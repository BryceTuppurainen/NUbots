#ifndef MODULE_SUPPORT_OPTIMISATION_NSGA2EVALUATOR_H
#define MODULE_SUPPORT_OPTIMISATION_NSGA2EVALUATOR_H

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <nuclear>

#include "message/platform/RawSensors.hpp"
#include "message/support/optimisation/NSGA2EvaluatorMessages.hpp"
#include "message/support/optimisation/NSGA2OptimiserMessages.hpp"

namespace module {
    namespace support {
        namespace optimisation {

            using RawSensorsMsg = message::platform::RawSensors;
            using message::support::optimisation::NSGA2EvaluationRequest;
            using message::support::optimisation::NSGA2TrialExpired;

            class NSGA2Evaluator : public NUClear::Reactor {

            public:
                /// @brief Called by the powerplant to build and setup the NSGA2Evaluator reactor.
                explicit NSGA2Evaluator(std::unique_ptr<NUClear::Environment> environment);

                enum State {
                    UNKNOWN                = 0,
                    WAITING_FOR_REQUEST    = 1,
                    SETTING_UP_TRIAL       = 2,
                    RESETTING_SIMULATION   = 3,
                    STANDING               = 4,
                    WALKING                = 5,
                    TERMINATING_EARLY      = 6,
                    TERMINATING_GRACEFULLY = 7,
                    FINISHED               = 8
                };

                enum Event {
                    // From webots
                    ResetDone = 0,

                    // From optimiser
                    CheckReady = 1,
                    EvaluateRequest = 2,
                    TerminateEvaluation = 3,

                    // Internal
                    TrialSetupDone    = 4,
                    Fallen            = 5,
                    StandDone         = 6,
                    TrialTimeExpired  = 7,
                    FitnessScoresSent = 8
                };

            private:
                /// @brief Subsumption priority ID for this module
                size_t subsumptionId;

                /// @brief The amount of time to run a single trial, in seconds. Read from config.
                int trial_duration_limit = 0.0;

                /// @brief The walk command velocity, read from config
                Eigen::Vector2d walk_command_velocity = Eigen::Vector2d(0.0, 0.0);

                /// @brief The walk command rotation, read from config
                double walk_command_rotation = 0.0;

                /// @brief The arm targets during the walk (read from config, used for Stand done detection)
                double arms_l_shoulder_pitch = 0.0;
                double arms_r_shoulder_pitch = 0.0;
                double arms_l_elbow          = 0.0;
                double arms_r_elbow          = 0.0;

                /// @brief The current simulation time
                double simTime = 0.0;

                /// @brief Delta between this update and the last update
                double simTimeDelta = 0.0;

                /// @brief Keep track of when the trial started
                double trialStartTime = 0.0;

                /// @brief The number of the current generation
                int generation = 0;

                /// @brief The number of the current individual in the current generation
                int individual = 0;

                /// @brief Robot state for this evaluation, used during fitness and constraint calculation
                Eigen::Vector3d initialRobotPosition = Eigen::Vector3d::Zero();
                Eigen::Vector3d robotPosition = Eigen::Vector3d::Zero();
                bool initialPositionSet = false;
                double maxFieldPlaneSway = 0.0;

                /// @brief Keeps track of the last messages we received
                NSGA2EvaluationRequest lastEvalRequestMsg;

                /// @brief Send the evaluated fitness scores
                /// @param scores The evaluated fitness scores for the current individual (sway, distance travelled)
                /// @param constraints A list of constraints for domination calculation. These can be used to encode one
                /// or more failure conditions. Here, we set the first constraint if the robot has fallen over, and the
                /// second one if it's swayed too too much. The more negative the constraint value is, the more it has
                /// violated the constriant, which means it will be dominated by other individuals that have violated
                /// less.
                void SendFitnessScores(std::vector<double> scores, std::vector<double> constraints);

                /// @brief Implementation of the fitness function (i.e. how well the individual did)
                std::vector<double> CalculateScores();

                /// @brief Implementation of the constraints function (i.e. how much to penalise for violations)
                std::vector<double> CalculateConstraints();

                /// @brief Returns the appropriate values for the case where no constraints are violated
                std::vector<double> ConstraintsNotViolated();

                /// @brief Check sensors to see if the robot has fallen
                void CheckForFall(const RawSensorsMsg& sensors);

                /// @brief Check sensors to update Max Field Plane Sway
                void UpdateMaxFieldPlaneSway(const RawSensorsMsg& sensors);

                /// @brief Check servo positions to see if the robot is now in the standing pose
                void CheckForStandDone(const RawSensorsMsg& sensors);

                /// @brief The current state of the evaluation
                State currentState = State::WAITING_FOR_REQUEST;

                /// @brief Get the next state to transition to given the current state and an event
                State HandleTransition(State currentState, Event event);

                /// @brief Handle the WAITING_FOR_REQUEST state
                void WaitingForRequest(State previousState, Event event);

                /// @brief Handle the SETTING_UP_TRIAL state
                void SettingUpTrial(State previousState, Event event);

                /// @brief Handle the RESETTING_SIMULATION state
                void ResettingSimulation(State previousState, Event event);

                /// @brief Handle the STANDING state
                void Standing(State previousState, Event event);

                /// @brief Handle the WALKING state
                void Walking(State previousState, Event event);

                /// @brief Handle the TERMINATING_EARLY state
                void TerminatingEarly(State previousState, Event event);

                /// @brief Handle the TERMINATING_GRACEFULLY state
                void TerminatingGracefully(State previousState, Event event);

                /// @brief Handle the FINISHED state
                void Finished(State previousState, Event event);
            };

        }  // namespace optimisation
    }      // namespace support
}  // namespace module

#endif  // MODULE_SUPPORT_OPTIMISATION_NSGA2EVALUATOR_H
