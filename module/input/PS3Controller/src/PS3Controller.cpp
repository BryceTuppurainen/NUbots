#include "PS3Controller.hpp"

#include <array>
#include <chrono>
#include <fcntl.h>
#include <fmt/format.h>
#include <limits>

#include "extension/Configuration.hpp"

#include "message/input/PSController.hpp"

namespace module::input {

    enum class EventType : uint8_t { BUTTON = 0x01, AXIS = 0x02, INIT = 0x80 };
    enum class Button : uint8_t {
        CROSS          = 0,
        CIRCLE         = 1,
        TRIANGLE       = 2,
        SQUARE         = 3,
        L1             = 4,
        R1             = 5,
        SELECT         = 8,
        START          = 9,
        PS             = 10,
        JOYSTICK_LEFT  = 11,
        JOYSTICK_RIGHT = 12,
        DPAD_UP        = 13,
        DPAD_DOWN      = 14,
        DPAD_LEFT      = 15,
        DPAD_RIGHT     = 16
    };
    enum class Axis : uint8_t {
        JOYSTICK_LEFT_HORIZONTAL = 0,
        JOYSTICK_LEFT_VERTICAL,
        L2,
        JOYSTICK_RIGHT_HORIZONTAL,
        JOYSTICK_RIGHT_VERTICAL,
        R2,
        ACCELEROMETER_X = 23,
        ACCELEROMETER_Y,
        ACCELEROMETER_Z
    };

#pragma pack(push, 1)
    struct JoystickEvent {
        uint32_t timestamp;  // event timestamp in milliseconds
        int16_t value;       // value
        uint8_t type;        // event type
        uint8_t number;      // axis/button number
    };
    // Check that this struct is not cache alligned
    static_assert(sizeof(JoystickEvent) == 8, "The compiler is adding padding to this struct, Bad compiler!");
#pragma pack(pop)

    using extension::Configuration;

    using message::input::Accelerometer;
    using message::input::CircleButton;
    using message::input::CrossButton;
    using message::input::DPadButton;
    using message::input::L1Button;
    using message::input::L2Trigger;
    using message::input::LeftJoystick;
    using message::input::LeftJoystickButton;
    using message::input::PSButton;
    using message::input::R1Button;
    using message::input::R2Trigger;
    using message::input::RightJoystick;
    using message::input::RightJoystickButton;
    using message::input::SelectButton;
    using message::input::SquareButton;
    using message::input::StartButton;
    using message::input::TriangleButton;

    PS3Controller::PS3Controller(std::unique_ptr<NUClear::Environment> environment)
        : Reactor(std::move(environment)), controller_fd(-1), accelerometer_fd(-1) {

        on<Configuration>("PS3Controller.yaml").then([this](const Configuration& config) {
            // Only reconnect if we are changing the path to the device
            if ((controller_path.compare(config["controller_path"].as<std::string>()) != 0)
                /* || (accelerometer_path.compare(config["accelerometer_path"].as<std::string>()) != 0) */) {
                controller_path    = config["controller_path"].as<std::string>();
                NUClear::log<NUClear::INFO>(fmt::format("controller path on configuration: ", controller_path));

                /* accelerometer_path = config["accelerometer_path"].as<std::string>(); */
                connect();
            }
        });

        on<Shutdown>().then([this] { disconnect(); });

        // Keep an eye on the joystick devices
        on<Every<1, std::chrono::seconds>, Single>().then([this] {
            // If it's closed then we should try to reconnect
            log<NUClear::WARN>("controller_fd: ", controller_fd);
            // log<NUClear::WARN>("accelerometer_fd: ", accelerometer_fd);
            if ((controller_fd > -1) /* && (accelerometer_fd > -1) */) {
                bool controller_valid    = !(fcntl(controller_fd, F_GETFL) < 0 && errno == EBADF);
                // bool accelerometer_valid = !(fcntl(accelerometer_fd, F_GETFL) < 0 && errno == EBADF);
                if (!controller_valid /* || !accelerometer_valid */) {
                    log<NUClear::WARN>("Joystick is not valid. Reconnecting.");
                    connect();
                }
            }
            if ((controller_fd < 0) /* || (accelerometer_fd < 0) */) {
                log<NUClear::WARN>("Joystick is not valid. Reconnecting.");
                connect();
            }
        });
    }

    void PS3Controller::connect() {
        // Make sure joystick file descriptors are closed.
        disconnect();
        //NOTE: Hard coded file path while debugging - L.Craft
        controller_path = std::filesystem::path("/dev/input/js0");
        // NUClear::log<NUClear::INFO>(fmt::format("Connecting to {} and {}", controller_path, accelerometer_path));
        NUClear::log<NUClear::INFO>(fmt::format("Connecting to {}", controller_path));
        controller_fd    = open(controller_path.c_str(), O_RDONLY | O_NONBLOCK);
        // accelerometer_fd = open(accelerometer_path.c_str(), O_RDONLY | O_NONBLOCK);
        // accelerometer_fd = 0;

        if (controller_fd > -1) {
            // Details on Linux Joystick API
            // https://www.kernel.org/doc/Documentation/input/joystick-api.txt
            // Trigger when the joystick has an data to read
            controller_reaction = on<IO>(controller_fd, IO::READ | IO::ERROR).then([this](const IO::Event& event) {
                // Check for IO errors
                if (event.events & IO::ERROR) {
                    disconnect();
                }
                else {
                    // Try to read a JoystickEvent worth of data
                    for (JoystickEvent event;
                         read(controller_fd, &event, sizeof(JoystickEvent)) == sizeof(JoystickEvent);) {
                        // Extract type from event buffer an unsigned 8-bit value
                        bool is_axis = false, is_button = false, is_init = false;
                        if (event.type & static_cast<uint8_t>(EventType::INIT)) {
                            event.type &= ~static_cast<uint8_t>(EventType::INIT);
                            is_init = true;
                        }
                        switch (static_cast<EventType>(event.type)) {
                            case EventType::BUTTON: is_button = true; break;
                            case EventType::AXIS: is_axis = true; break;
                            case EventType::INIT: /* This is handled above. */ break;
                        }

                        // Extract number from event buffer an unsigned 8-bit value
                        if (is_button) {
                            switch (static_cast<Button>(event.number)) {
                                case Button::CROSS:
                                    emit(std::make_unique<CrossButton>(event.value > 0, is_init));
                                    break;
                                case Button::CIRCLE:
                                    emit(std::make_unique<CircleButton>(event.value > 0, is_init));
                                    break;
                                case Button::TRIANGLE:
                                    emit(std::make_unique<TriangleButton>(event.value > 0, is_init));
                                    break;
                                case Button::SQUARE:
                                    emit(std::make_unique<SquareButton>(event.value > 0, is_init));
                                    break;
                                case Button::L1: emit(std::make_unique<L1Button>(event.value > 0, is_init)); break;
                                case Button::R1: emit(std::make_unique<R1Button>(event.value > 0, is_init)); break;
                                case Button::SELECT:
                                    emit(std::make_unique<SelectButton>(event.value > 0, is_init));
                                    break;
                                case Button::START:
                                    emit(std::make_unique<StartButton>(event.value > 0, is_init));
                                    break;
                                case Button::PS: emit(std::make_unique<PSButton>(event.value > 0, is_init)); break;
                                case Button::JOYSTICK_LEFT:
                                    emit(std::make_unique<LeftJoystickButton>(event.value > 0, is_init));
                                    break;
                                case Button::JOYSTICK_RIGHT:
                                    emit(std::make_unique<RightJoystickButton>(event.value > 0, is_init));
                                    break;
                                case Button::DPAD_UP:
                                    emit(std::make_unique<DPadButton>(DPadButton::Direction::UP,
                                                                      event.value > 0,
                                                                      is_init));
                                    break;
                                case Button::DPAD_DOWN:
                                    emit(std::make_unique<DPadButton>(DPadButton::Direction::DOWN,
                                                                      event.value > 0,
                                                                      is_init));
                                    break;
                                case Button::DPAD_LEFT:
                                    emit(std::make_unique<DPadButton>(DPadButton::Direction::LEFT,
                                                                      event.value > 0,
                                                                      is_init));
                                    break;
                                case Button::DPAD_RIGHT:
                                    emit(std::make_unique<DPadButton>(DPadButton::Direction::RIGHT,
                                                                      event.value > 0,
                                                                      is_init));
                                    break;
                            }
                        }
                        else if (is_axis) {
                            float normalised = static_cast<float>(event.value)
                                               / static_cast<float>(std::numeric_limits<int16_t>::max());
                            switch (static_cast<Axis>(event.number)) {
                                case Axis::JOYSTICK_LEFT_HORIZONTAL:
                                    emit(std::make_unique<LeftJoystick>(LeftJoystick::Direction::HORIZONTAL,
                                                                        normalised,
                                                                        is_init));
                                    break;
                                case Axis::JOYSTICK_LEFT_VERTICAL:
                                    emit(std::make_unique<LeftJoystick>(LeftJoystick::Direction::VERTICAL,
                                                                        normalised,
                                                                        is_init));
                                    break;
                                case Axis::L2: emit(std::make_unique<L2Trigger>(normalised, is_init)); break;
                                case Axis::JOYSTICK_RIGHT_HORIZONTAL:
                                    emit(std::make_unique<RightJoystick>(RightJoystick::Direction::HORIZONTAL,
                                                                         normalised,
                                                                         is_init));
                                    break;
                                case Axis::JOYSTICK_RIGHT_VERTICAL:
                                    emit(std::make_unique<RightJoystick>(RightJoystick::Direction::VERTICAL,
                                                                         normalised,
                                                                         is_init));
                                    break;
                                case Axis::R2: emit(std::make_unique<R2Trigger>(normalised, is_init)); break;
                                case Axis::ACCELEROMETER_X: break;
                                case Axis::ACCELEROMETER_Y: break;
                                case Axis::ACCELEROMETER_Z: break;
                            }
                        }
                        else {
                            log<NUClear::WARN>("Unknown event on joystick. Ignoring.");
                            log<NUClear::WARN>(fmt::format("Value....: {}", event.value));
                            log<NUClear::WARN>(
                                fmt::format("Type.....: {}",
                                            is_init ? event.type | static_cast<uint8_t>(EventType::INIT) : event.type));
                            log<NUClear::WARN>(fmt::format("Number...: {}", static_cast<int>(event.number)));
                        }
                    }
                }
            });
        }

/*         if (accelerometer_fd > -1) {
            // Trigger when the joystick accelerometer has an event to read
            accelerometer_reaction =
                on<IO>(accelerometer_fd, IO::READ | IO::ERROR).then([this](const IO::Event& event) {
                    // Check for IO errors
                    if (event.events & IO::ERROR) {
                        disconnect();
                    }
                    // Accelerometer comes in here .... if you know which axis is which
                });
        } */
    }

    void PS3Controller::disconnect() {
        controller_reaction.unbind();
        // accelerometer_reaction.unbind();
        if (controller_fd != -1) {
            ::close(controller_fd);
            controller_fd = -1;
        }
        // if (accelerometer_fd != -1) {
        //     ::close(accelerometer_fd);
        //     accelerometer_fd = -1;
        // }
    }
}  // namespace module::input
