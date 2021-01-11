/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <services/context.h>
#include <services/etel/line.h>
#include <services/etel/subsess.h>
#include <utils/err.h>

#include <common/log.h>

namespace eka2l1 {
    etel_line::etel_line(const epoc::etel_line_info &info, const std::string &name, const std::uint32_t caps)
        : info_(info)
        , caps_(caps)
        , name_(name) {
    }

    etel_line::~etel_line() {
    }

    etel_line_subsession::etel_line_subsession(etel_session *session, etel_line *line, bool oldarch)
        : etel_subsession(session)
        , line_(line)
        , oldarch_(oldarch) {
    }

    void etel_line_subsession::dispatch(service::ipc_context *ctx) {
        if (oldarch_) {
            switch (ctx->msg->function) {
            case epoc::etel_old_line_get_status:
                get_status(ctx);
                break;

            case epoc::etel_old_line_notify_status_change:
                notify_status_change(ctx);
                break;

            case epoc::etel_old_line_notify_status_change_cancel:
                cancel_notify_status_change(ctx);
                break;

            default:
                LOG_ERROR(SERVICE_ETEL, "Unimplemented etel line opcode {}", ctx->msg->function);
                break;
            }
        } else {
            switch (ctx->msg->function) {
            case epoc::etel_line_get_status:
            case epoc::etel_mobile_line_get_mobile_line_status: // Note: Not the same, just stub
                get_status(ctx);
                break;

            case epoc::etel_line_notify_incoming_call:
                notify_incoming_call(ctx);
                break;

            case epoc::etel_line_cancel_notify_incoming_call:
                cancel_notify_incoming_call(ctx);
                break;

            case epoc::etel_mobile_line_notify_status_change:
                notify_status_change(ctx);
                break;

            case epoc::etel_mobile_line_cancel_notify_status_change:
                cancel_notify_status_change(ctx);
                break;

            default:
                LOG_ERROR(SERVICE_ETEL, "Unimplemented etel line opcode {}", ctx->msg->function);
                break;
            }
        }
    }

    void etel_line_subsession::get_status(service::ipc_context *ctx) {
        if (ctx->msg->function == epoc::etel_mobile_line_get_mobile_line_status) {
            LOG_TRACE(SERVICE_ETEL, "Mobile line get status stubbed with normal get status");
        }

        ctx->write_data_to_descriptor_argument<epoc::etel_line_status>(0, line_->info_.sts_);
        ctx->complete(epoc::error_none);
    }

    void etel_line_subsession::notify_status_change(service::ipc_context *ctx) {
        status_change_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void etel_line_subsession::cancel_notify_status_change(service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
        status_change_nof_.complete(epoc::error_cancel, "CANCEL NOTIFY STS LINE");
    }

    void etel_line_subsession::notify_incoming_call(service::ipc_context *ctx) {
        incoming_call_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void etel_line_subsession::cancel_notify_incoming_call(service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
        incoming_call_nof_.complete(epoc::error_cancel, "CANCEL NOTIFY INCOMING CALL");
    }
}