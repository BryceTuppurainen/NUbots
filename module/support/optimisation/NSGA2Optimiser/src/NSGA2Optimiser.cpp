#include "NSGA2Optimiser.hpp"

#include "extension/Configuration.hpp"

#include "message/platform/webots/WebotsReady.hpp"
#include "message/platform/webots/messages.hpp"
#include "message/support/optimisation/NSGA2EvaluatorMessages.hpp"
#include "message/support/optimisation/NSGA2OptimiserMessages.hpp"

#include "utility/support/yaml_expression.hpp"

namespace module {
    namespace support {
        namespace optimisation {

            using extension::Configuration;

            using message::platform::webots::OptimisationCommand;
            using message::platform::webots::WebotsReady;
            using message::support::optimisation::NSGA2EvaluationRequest;
            using message::support::optimisation::NSGA2EvaluatorReady;
            using message::support::optimisation::NSGA2FitnessScores;
            using message::support::optimisation::NSGA2Terminate;

            using utility::support::Expression;

            NSGA2Optimiser::NSGA2Optimiser(std::unique_ptr<NUClear::Environment> environment)
                : Reactor(std::move(environment)) {

                // Read NSGA2Optimiser.yaml file and initialize the values we're going to use for the optimisation
                on<Configuration>("NSGA2Optimiser.yaml").then([this](const Configuration& config) {
                    default_leg_gains = config["gains"]["legs"].as<Expression>();

                    // The initial values of the parameters to optimise
                    std::vector<double> paramInitialValues;

                    // Parallel to paramInitialValues, sets the limit (min, max) of each parameter value
                    std::vector<std::pair<double, double>> paramLimits;

                    // Extract the initial values and limits and from config file, for all of the parameters
                    auto& walk = config["walk"];
                    for (const auto& element :
                         std::vector<std::string>({std::string("freq"), std::string("double_support_ratio")})) {
                        paramInitialValues.emplace_back(walk[element][0].as<Expression>());
                        paramLimits.emplace_back(walk[element][1].as<Expression>(), walk[element][2].as<Expression>());
                    }

                    auto& foot = walk["foot"];
                    for (const auto& element :
                         std::vector<std::string>({std::string("distance"), std::string("rise")})) {
                        paramInitialValues.emplace_back(foot[element][0].as<Expression>());
                        paramLimits.emplace_back(foot[element][1].as<Expression>(), foot[element][2].as<Expression>());
                    }

                    auto& trunk = walk["trunk"];
                    for (const auto& element : std::vector<std::string>({std::string("height"),
                                                                         std::string("pitch"),
                                                                         std::string("x_offset"),
                                                                         std::string("y_offset"),
                                                                         std::string("swing"),
                                                                         std::string("pause")})) {
                        paramInitialValues.emplace_back(trunk[element][0].as<Expression>());
                        paramLimits.emplace_back(trunk[element][1].as<Expression>(),
                                                 trunk[element][2].as<Expression>());
                    }

                    auto& pause = walk["pause"];  // config['walk']['pause']
                    for (const auto& element : std::vector<std::string>({std::string("duration")})) {
                        paramInitialValues.emplace_back(pause[element][0].as<Expression>());
                        paramLimits.emplace_back(pause[element][1].as<Expression>(),
                                                 pause[element][2].as<Expression>());
                    }

                    // Set up the NSGA2 algorithm based on our config values
                    nsga2Algorithm.SetRealVariableCount(paramInitialValues.size());
                    nsga2Algorithm.SetObjectiveCount(config["num_objectives"].as<int>());
                    nsga2Algorithm.SetContraintCount(config["num_constraints"].as<int>());
                    nsga2Algorithm.SetPopulationSize(config["population_size"].as<int>());
                    nsga2Algorithm.SetTargetGenerations(config["num_generations"].as<int>());
                    nsga2Algorithm.SetRealCrossoverProbability(
                        config["probabilities"]["real"]["crossover"].as<double>());
                    nsga2Algorithm.SetRealMutationProbability(config["probabilities"]["real"]["mutation"].as<double>());
                    nsga2Algorithm.SetEtaC(config["eta"]["C"].as<double>());
                    nsga2Algorithm.SetEtaM(config["eta"]["M"].as<double>());
                    nsga2Algorithm.SetRealVarLimits(paramLimits);
                    nsga2Algorithm.SetRandomInitialize(false);
                    nsga2Algorithm.SetInitialRealVars(paramInitialValues);

                    // Zero out binary things
                    nsga2Algorithm.SetBinCrossoverProbability(0.0);
                    nsga2Algorithm.SetBinMutationProbability(0.0);

                    // This seg faults, SetSeed in the library dereferences a null pointer
                    nsga2Algorithm.SetSeed(config["seed"].as<int>());

                    // Not sure what this should be, but it has to be set
                    nsga2Algorithm.SetBinVariableCount(0);
                });

                on<Startup>().then([this]() {
                    // Create a message to request an evaluation of an individual
                    std::unique_ptr<WebotsReady> message = std::make_unique<WebotsReady>();
                    log<NUClear::INFO>("Starting up in 4 seconds");
                    emit<Scope::DELAY>(message, std::chrono::seconds(4));
                });

                on<Trigger<WebotsReady>, Single>().then([this](const WebotsReady& message) {
                    log<NUClear::INFO>("webots ready, starting first evaluation");

                    // If initialisation succeeded, evaluate the first individual of the first generation
                    // Subsequent individuals will be evaluated after we get the evaluation scores for this individual
                    // (from the NSGA2FitnessScores trigger)
                    if (nsga2Algorithm.InitializeFirstGeneration()) {
                        populateEvaluationRequests();
                        emit(std::make_unique<NSGA2EvaluatorReady>());
                    } else {
                        log<NUClear::ERROR>("Failed to initialise NSGA2");
                    }
                });

                on<Trigger<NSGA2EvaluatorReady>, Single>().then([this](const NSGA2EvaluatorReady& message) {
                    if (pendingEvaluationRequests.empty()) {
                       log<NUClear::INFO>("Evaluator ready, but optimiser is not");
                       emit<Scope::DELAY>(std::make_unique<NSGA2EvaluatorReady>(), std::chrono::seconds(1)); // Wait for a bit for us to become ready, then ask the evaluator if it is ready
                    } else {
                        log<NUClear::INFO>("\n\nSending request to evaluator. Generation:", pendingEvaluationRequests.front()->generation, "individual", pendingEvaluationRequests.front()->id);

                        runningEvaluationRequests.emplace(pendingEvaluationRequests.front()->id); //hold onto the request in the running queue
                        emit(pendingEvaluationRequests.front());
                        pendingEvaluationRequests.pop(); // remove from the pending queue
                    }
                });

                on<Trigger<NSGA2FitnessScores>, Single>().then([this](const NSGA2FitnessScores& scores) {
                    log<NUClear::DEBUG>("Got evaluation fitness scores");

                    // Mark the request as completed
                    auto search = runningEvaluationRequests.find(scores.id);
                    if (search != runningEvaluationRequests.end()) {
                        runningEvaluationRequests.erase(search);
                    }

                    // An individual has been evaluation and we've got the scores. This updates the
                    // algorithm with the score, and evaluates the next individual.
                    if (scores.generation < nsga2Algorithm.generations) {
                        processOrdinaryGenerationIndividual(scores.id, scores.generation, scores.objScore, scores.constraints);
                    } else {
                        processFinalGenerationIndividual(scores.id, scores.generation, scores.objScore, scores.constraints);
                    }
                });

                on<Trigger<NSGA2Terminate>, Single>().then([this]() {
                    // NSGA2Terminate is emitted when we've finished all generations and all individuals
                    // Tell ourselves to terminate
                    log<NUClear::INFO>("Powerplant shutdown");
                    powerplant.shutdown();
                });
            }

            void NSGA2Optimiser::populateEvaluationRequests() {
                for (int i = 0; i < nsga2Algorithm.popSize; i++) {
                    pendingEvaluationRequests.push(constructEvaluationRequest(i,
                                            nsga2Algorithm.getCurrentPop()->generation,
                                            nsga2Algorithm.getCurrentPop()->GetIndReals(i)));
                }
            }

            bool NSGA2Optimiser::atEndOfGeneration() {
                // We are at the end of a generation when there are no requests left unprocessed (since we evaulate a generation at a time)
                return runningEvaluationRequests.empty() && pendingEvaluationRequests.empty();
            }

            void NSGA2Optimiser::processOrdinaryGenerationIndividual(int id, int generation, const std::vector<double>& objScore, const std::vector<double>& constraints) {
                // Tell the algorithm the evaluation scores for this individual
                nsga2Algorithm.getCurrentPop()->SetIndObjectiveScore(id, objScore);
                nsga2Algorithm.getCurrentPop()->SetIndConstraints(id, constraints);

                if (atEndOfGeneration()) {
                    // End the generation and save its data
                    nsga2Algorithm.CompleteGeneration();

                    // Start the next generation (creates the new children for evaluation)
                    nsga2Algorithm.InitializeNextGeneration();
                    log<NUClear::INFO>("Advanced to new generation", nsga2Algorithm.getCurrentPop()->generation);
                    populateEvaluationRequests();
                }
            }

            void NSGA2Optimiser::processFinalGenerationIndividual(int id, int generation, const std::vector<double>& objScore, const std::vector<double>& constraints) {
                // Tell the algorithm the evaluation scores for this individual
                nsga2Algorithm.getCurrentPop()->SetIndObjectiveScore(id, objScore);
                nsga2Algorithm.getCurrentPop()->SetIndConstraints(id, constraints);

                if (atEndOfGeneration()) {
                    // End the generation and save its data
                    nsga2Algorithm.CompleteGeneration();

                    // Report the population of our final generation
                    nsga2Algorithm.ReportFinalGenerationPop();

                    log<NUClear::INFO>("NSGA2 evaluation finished!");

                    // Tell Webots to terminate
                    std::unique_ptr<OptimisationCommand> msg = std::make_unique<OptimisationCommand>();
                    msg->command                             = OptimisationCommand::CommandType::TERMINATE;
                    emit(msg);

                    // Tell the NSGA2 components to finish up, but add a delay to give webots time to get the terminate
                    emit<Scope::DELAY>(std::make_unique<NSGA2Terminate>(), std::chrono::milliseconds(100));
                }
            }

            std::unique_ptr<NSGA2EvaluationRequest> NSGA2Optimiser::constructEvaluationRequest(int id, int generation, const std::vector<double>& parameters) {
                // Create a message to request an evaluation of an individual
                auto request = std::make_unique<NSGA2EvaluationRequest>();
                request->id                                     = id;
                request->generation                             = generation;

                // Add the individual's parameters to the message
                request->parameters.freq                 = parameters[0];
                request->parameters.double_support_ratio = parameters[1];
                request->parameters.foot.distance        = parameters[2];
                request->parameters.foot.rise            = parameters[3];
                request->parameters.trunk.height         = parameters[4];
                request->parameters.trunk.pitch          = parameters[5];
                request->parameters.trunk.x_offset       = parameters[6];
                request->parameters.trunk.y_offset       = parameters[7];
                request->parameters.trunk.swing          = parameters[8];
                request->parameters.trunk.pause          = parameters[9];
                request->parameters.pause.duration       = parameters[10];
                return request;
            }
        }  // namespace optimisation
    }      // namespace support
}  // namespace module
