#ifndef MODULE_INPUT_PS4CONTROLLER_HPP
#define MODULE_INPUT_PS4CONTROLLER_HPP

#include <nuclear>

namespace module::input {

    class PS4Controller : public NUClear::Reactor {

    public:
        /// @brief Called by the powerplant to build and setup the PS3Controller reactor.
        explicit PS4Controller(std::unique_ptr<NUClear::Environment> environment);

    private:
        void connect();
        void disconnect();

        int controller_fd;
        int accelerometer_fd;
        std::string controller_path;
        std::string accelerometer_path;
        ReactionHandle controller_reaction;
        ReactionHandle accelerometer_reaction;

        bool dpad_left_pressed;
        bool dpad_right_pressed;
        bool dpad_up_pressed;
        bool dpad_down_pressed;
    };

}  // namespace module::input

#endif  // MODULE_INPUT_PS4CONTROLLER_HPP
