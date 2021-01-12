/*
 * Copyright (c) 2018 EKA2L1 Team / Citra Team
 * 
 * This file is part of EKA2L1 project / Citra Emulator Project.
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <kernel/kernel_obj.h>
#include <kernel/timing.h>

#include <utils/reqsts.h>

#include <memory>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    namespace kernel {
        class timer;

        struct signal_info {
            epoc::notify_info nof;
            timer *own_timer;
        };

        class timer : public kernel_obj {
            ntimer *timing;
            int callback_type;

            signal_info info;
            bool outstanding;

        public:
            timer(kernel_system *kern, ntimer *timing, std::string name,
                kernel::access_type access = access_type::local_access);
            ~timer();

            bool after(kernel::thread *requester, eka2l1::ptr<epoc::request_status> sts,
                std::uint64_t us_signal);
            
            bool request_finish();
            bool cancel_request();
        };
    }
}
