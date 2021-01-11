/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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

#include <kernel/kernel.h>
#include <mem/mem.h>
#include <mem/ptr.h>

#include <kernel/change_notifier.h>

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/random.h>

namespace eka2l1 {
    namespace kernel {
        change_notifier::change_notifier(kernel_system *kern)
            : eka2l1::kernel::kernel_obj(kern, "changenotifier" + common::to_string(eka2l1::random()),
                nullptr, kernel::access_type::local_access) {
            obj_type = object_type::change_notifier;
        }

        bool change_notifier::logon(eka2l1::ptr<epoc::request_status> request_sts) {
            // If this is already logon
            if (req_info_.sts) {
                return false;
            }

            req_info_.requester = kern->crr_thread();
            req_info_.sts = request_sts;

            return true;
        }

        bool change_notifier::logon_cancel() {
            if (!req_info_.sts) {
                return false;
            }

            req_info_.complete(-3, "CHANGE NOTIF CANCEL");

            return true;
        }

        void change_notifier::notify_change_requester() {
            req_info_.complete(0, "NOTIFY CHANGE REQUESTER");
        }

        void change_notifier::do_state(common::chunkyseri &seri) {
            req_info_.do_state(seri);
        }
    }
}
