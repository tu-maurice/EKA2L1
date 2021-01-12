/*
 * Copyright (c) 2018 EKA2L1 Team / Citra Team
 * 
 * This file is part of EKA2L1 project / Citra Emulator Project
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

#include <common/cvt.h>
#include <common/log.h>

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <utils/err.h>

#include <vector>

namespace eka2l1 {
    namespace kernel {
        void timer_callback(uint64_t user, int ns_late);

        bool dumped = false;
        std::vector<std::string> timer_compl_v;

        timer::timer(kernel_system *kern, ntimer *timing, std::string name,
            kernel::access_type access)
            : kernel_obj(kern, name, nullptr, access)
            , timing(timing)
            , outstanding(false) {
            obj_type = object_type::timer;

            callback_type = timing->get_register_event("TimerCallback" + common::to_string(uid));

            if (callback_type == -1) {
                callback_type = timing->register_event("TimerCallback" + common::to_string(uid),
                    timer_callback);
            }
        }

        timer::~timer() {
            if (!dumped) {
                LOG_TRACE(KERNEL, "Peepee boo boo");
                for (auto &line : timer_compl_v) {
                    LOG_TRACE(KERNEL, "{}", line);
                }

                dumped = true;
            }

            timing->unschedule_event(callback_type, reinterpret_cast<std::uint64_t>(&info));
        }

        bool timer::after(kernel::thread *requester, eka2l1::ptr<epoc::request_status> sts,
            std::uint64_t us_signal) {
            if (outstanding) {
                return false;
            }

            outstanding = true;

            info.nof = { sts, requester };
            info.own_timer = this;

            timing->schedule_event(us_signal, callback_type, reinterpret_cast<std::uint64_t>(&info));

            return false;
        }

        bool timer::request_finish() {
            if (!outstanding) {
                return false;
            }

            outstanding = false;
            return true;
        }

        bool timer::cancel_request() {
            if (!outstanding) {
                // Do a signal so that the semaphore won't lock the thread up next time it waits
                // info.own_thread->signal_request();
                return false;
            }

            info.nof.complete(epoc::error_cancel, "TIMER CANCEL");

            // If the timer hasn't finished yet, please unschedule it.
            if (outstanding) {
                // Cancel
                timing->unschedule_event(callback_type, reinterpret_cast<std::uint64_t>(&info));
            }

            return request_finish();
        }

        void timer_callback(uint64_t user, int ns_late) {
            signal_info *info = reinterpret_cast<signal_info *>(user);

            if (!info) {
                return;
            }

            kernel_system *kern = info->own_timer->get_kernel_object_owner();
            kern->lock();
            
            if (!info->own_timer->request_finish()) {
                kern->unlock();
                return;
            }

            if (timer_compl_v.size() == 1200) {
                timer_compl_v.erase(timer_compl_v.begin());
            }

            if (info->nof.requester && info->nof.requester->name() == "Bounce") {
                timer_compl_v.push_back(fmt::format("0x{:X} to notify", info->nof.sts.ptr_address()));
            }

            info->nof.complete(epoc::error_none, "TIMER COMPL");
            kern->unlock();
        }
    }
}
