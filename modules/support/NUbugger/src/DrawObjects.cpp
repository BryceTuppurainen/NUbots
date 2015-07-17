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

#include "NUbugger.h"

#include "messages/support/nubugger/proto/Message.pb.h"

#include "utility/time/time.h"

namespace modules {
namespace support {
    using messages::support::nubugger::proto::Message;
    using utility::time::getUtcTimestamp;

    using messages::support::nubugger::proto::DrawObjects;
    using messages::support::nubugger::proto::DrawObject;

    void NUbugger::provideDrawObjects() {

        handles[Message::DRAW_OBJECTS].push_back(on<Trigger<DrawObjects>>([this](const DrawObjects& drawObjects) {

            // TODO: potentially bad filter id
            Message message = createMessage(Message::DRAW_OBJECTS);
            *(message.mutable_draw_objects()) = drawObjects;
            send(message);
            
        }));

        handles[Message::DRAW_OBJECTS].push_back(on<Trigger<DrawObject>>([this](const DrawObject& drawObject) {

            Message message = createMessage(Message::DRAW_OBJECTS);
            *(message.mutable_draw_objects()->add_objects()) = drawObject;
            send(message);

        }));
    }
}
}
