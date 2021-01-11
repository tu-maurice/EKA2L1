/*
 * Copyright (c) 2018 EKA2L1 Team
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
#include <kernel/thread.h>
#include <kernel/property.h>
#include <utils/err.h>

#include <common/log.h>

namespace eka2l1 {
    namespace service {
        property::property(kernel_system *kern)
            : kernel::kernel_obj(kern, "", nullptr, kernel::access_type::global_access)
            , data_len(0)
            , data_type(service::property_type::unk) {
            obj_type = kernel::object_type::prop;
            bindata.reserve(512);
        }

        bool property::is_defined() {
            return data_type != service::property_type::unk;
        }

        void property::add_data_change_callback(void *userdata, data_change_callback_handler handler) {
            data_change_callbacks.push_back({ userdata, handler });
        }

        void property::fire_data_change_callbacks() {
            for (auto &callback : data_change_callbacks) {
                callback.second(callback.first, this);
            }

            data_change_callbacks.clear();
        }

        void property::define(service::property_type pt, uint32_t pre_allocated) {
            data_type = pt;
            data_len = pre_allocated;

            if (pre_allocated > 512) {
                LOG_WARN(KERNEL, "Property trying to alloc more then 512 bytes, limited to 512 bytes");
                data_len = 512;
            }
        }

        bool property::set_int(int val) {
            if (data_type == service::property_type::int_data) {
                ndata = val;
                notify_request(epoc::error_none);

                fire_data_change_callbacks();

                return true;
            }

            return false;
        }

        bool property::set(uint8_t *bdata, uint32_t arr_length) {
            if (arr_length > data_len) {
                bindata.resize(arr_length);
            }

            memcpy(bindata.data(), bdata, arr_length);
            data_len = arr_length;

            notify_request(epoc::error_none);
            fire_data_change_callbacks();

            return true;
        }

        int property::get_int() {
            if (data_type != service::property_type::int_data) {
                return -1;
            }

            return ndata;
        }

        std::vector<uint8_t> property::get_bin() {
            std::vector<uint8_t> local;
            local.resize(data_len);

            memcpy(local.data(), bindata.data(), data_len);

            return local;
        }

        void property::subscribe(epoc::notify_info &info) {
            subscription_queue.push(&info);
        }

        bool property::cancel(const epoc::notify_info &info) {
            // Find the subscription
            auto subscription_iterator = std::find(subscription_queue.begin(), subscription_queue.end(),
                &info);

            if (subscription_iterator == subscription_queue.end()) {
                return false;
            }

            (*subscription_iterator)->complete(epoc::error_cancel, "PROP SUB CANCEL");
            return true;
        }

        void property::notify_request(const std::int32_t err) {
            while (auto subscription = subscription_queue.pop()) {
                subscription.value()->complete(err, "PROP SUB COMPL");
            }
        }

        property_reference::property_reference(kernel_system *kern, property *prop)
            : kernel::kernel_obj(kern, "", prop_)
            , prop_(prop) {
            obj_type = kernel::object_type::prop_ref;
        }

        bool property_reference::subscribe(const epoc::notify_info &info) {
            if (!nof_.empty()) {
                return false;
            }

            nof_ = info;
            prop_->subscribe(nof_);

            return true;
        }

        bool property_reference::cancel() {
            return prop_->cancel(nof_);
        }
    }
}
