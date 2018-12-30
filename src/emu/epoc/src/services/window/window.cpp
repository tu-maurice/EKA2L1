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

#include <epoc/services/window/op.h>
#include <epoc/services/window/window.h>

#include <common/e32inc.h>
#include <common/algorithm.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/cvt.h>

#include <e32err.h>

#include <epoc/epoc.h>
#include <epoc/vfs.h>

#include <optional>
#include <string>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

namespace eka2l1::epoc {
    enum {
        base_handle = 0x40000000
    };

    window_client_obj::window_client_obj(window_server_client_ptr client)
        : client(client)
        , id(static_cast<std::uint32_t>(client->objects.size()) + base_handle + 1) {
    }
    
    void window_client_obj::execute_command(eka2l1::service::ipc_context ctx, eka2l1::ws_cmd cmd) {
        LOG_ERROR("Unimplemented command handler for object with handle: 0x{:x}", cmd.obj_handle);
    }

    screen_device::screen_device(window_server_client_ptr client, 
        int number, eka2l1::graphics_driver_client_ptr driver)
        : window_client_obj(client)
        , driver(driver)
        , screen(number)
    {
        scr_config = client->get_ws().get_screen_config(number);
        crr_mode = &scr_config.modes[0];   

        driver->set_screen_size(crr_mode->size);
    }

    graphics_orientation number_to_orientation(int rot) {
        switch (rot) {
        case 0: {
            return graphics_orientation::normal;
        }

        case 90: {
            return graphics_orientation::rotated90;
        }

        case 180: {
            return graphics_orientation::rotated180;
        }

        case 270: {
            return graphics_orientation::rotated270;
        }

        default: {
            break;
        }
        }

        assert(false && "UNREACHABLE");
        return graphics_orientation::normal;
    }

    static epoc::config::screen_mode *find_screen_mode(epoc::config::screen &scr, int mode_num) {
        for (std::size_t i = 0 ; i < scr.modes.size(); i++) {
            if (scr.modes[i].mode_number == mode_num) {
                return &scr.modes[i];
            }
        }

        return nullptr;
    }

    void screen_device::execute_command(eka2l1::service::ipc_context ctx, eka2l1::ws_cmd cmd) {
        TWsScreenDeviceOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsSdOpPixelSize: {
            // This doesn't take any arguments
            eka2l1::vec2 screen_size = crr_mode->size;
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, screen_size);
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpTwipsSize: {
            // This doesn't take any arguments
            eka2l1::vec2 screen_size = crr_mode->size;
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, screen_size * twips_mul);
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpGetNumScreenModes: {
            ctx.set_request_status(static_cast<TInt>(scr_config.modes.size()));
            break;
        }

        case EWsSdOpSetScreenMode: {
            LOG_TRACE("Set screen mode stubbed, why did you even use this");
            ctx.set_request_status(KErrNone);

            break;
        }
        
        case EWsSdOpSetScreenSizeAndRotation: {
            pixel_twips_and_rot *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
            bool found = true;

            for (int i = 0; i < scr_config.modes.size(); i++) {
                if (scr_config.modes[i].size == info->pixel_size &&
                    number_to_orientation(scr_config.modes[i].rotation) == info->orientation) {
                    crr_mode = &scr_config.modes[i];

                    ctx.set_request_status(KErrNone);
                    found = true;

                    break;
                }
            }

            if (!found) {
                LOG_ERROR("Unable to set size: mode not found!");
                ctx.set_request_status(KErrNotSupported);
            }
            
            break;
        }

        case EWsSdOpGetScreenSizeModeList: {
            std::vector<int> modes;
            
            for (int i = 0; i < scr_config.modes.size(); i++) {
                modes.push_back(scr_config.modes[i].mode_number);
            }

            ctx.write_arg_pkg(reply_slot, reinterpret_cast<std::uint8_t*>(&modes[0]),
                static_cast<std::uint32_t>(sizeof(int) * modes.size()));

            ctx.set_request_status(static_cast<TInt>(scr_config.modes.size()));

            break;
        }

        // This get the screen size in pixels + twips and orientation for the given mode
        case EWsSdOpGetScreenModeSizeAndRotation: {
            int mode = *reinterpret_cast<int*>(cmd.data_ptr);
            
            epoc::config::screen_mode *scr_mode = find_screen_mode(scr_config, mode);

            if (!scr_mode) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            pixel_twips_and_rot data;
            data.pixel_size = scr_mode->size;
            data.twips_size = scr_mode->size * twips_mul;
            data.orientation = number_to_orientation(scr_mode->rotation);

            ctx.write_arg_pkg(reply_slot, data);
            ctx.set_request_status(0);

            break;
        }

        // This get the screen size in pixels and orientation for the given mode
        case EWsSdOpGetScreenModeSizeAndRotation2: {
            int mode = *reinterpret_cast<int*>(cmd.data_ptr);
            
            epoc::config::screen_mode *scr_mode = find_screen_mode(scr_config, mode);

            if (!scr_mode) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            pixel_and_rot data;
            data.pixel_size = scr_mode->size;
            data.orientation = number_to_orientation(scr_mode->rotation);

            ctx.write_arg_pkg(reply_slot, data);
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpGetScreenModeDisplayMode: {
            int mode = *reinterpret_cast<int*>(cmd.data_ptr);

            LOG_TRACE("GetScreenModeDisplayMode stubbed with true color + alpha (color16ma)");
            ctx.write_arg_pkg(reply_slot, display_mode::color16ma);
            ctx.set_request_status(KErrNone);

            break;
        }

        // Get the current screen mode. AknCapServer uses this, compare with the saved screen mode
        // to trigger the layout change event for registered app.
        case EWsSdOpGetScreenMode: {
            ctx.set_request_status(crr_mode->mode_number);
            break;
        }

        case EWsSdOpFree: {
            ctx.set_request_status(KErrNone);
            
            // Detach the screen device
            for (epoc::window_ptr &win: windows) {
                win->dvc.reset();
            }

            windows.clear();

            // TODO: Remove reference in the client

            client->delete_object(id);
            break;
        }

        default: {
            LOG_WARN("Unimplemented IPC call for screen driver: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }

    void graphic_context::active(service::ipc_context context, ws_cmd cmd) {
        const std::uint32_t window_to_attach_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        attached_window = std::reinterpret_pointer_cast<epoc::window_user>(client->get_object(window_to_attach_handle));

        // Attach context with window
        attached_window->contexts.push_back(this);        

        // Afaik that the pointer to CWsScreenDevice is internal, so not so scared of general users touching
        // this.
        context.set_request_status(attached_window->dvc->id);
    }

    void graphic_context::do_command_draw_text(service::ipc_context ctx, eka2l1::vec2 top_left, eka2l1::vec2 bottom_right, std::u16string text) {
        LOG_TRACE("Attemp to draw text {}", common::ucs2_to_utf8(text));
        
        std::string buf;
        auto pos_to_screen = attached_window->pos + top_left;

        buf.append(reinterpret_cast<char*>(&pos_to_screen), sizeof(eka2l1::vec2));

        if (bottom_right == vec2(-1, -1)) {
            buf.append(reinterpret_cast<char*>(&bottom_right), sizeof(eka2l1::vec2));
        } else {
            pos_to_screen = bottom_right + attached_window->pos;
            buf.append(reinterpret_cast<char*>(&pos_to_screen), sizeof(eka2l1::vec2));
        }

        std::uint32_t text_len = text.length();

        LOG_TRACE("Drawing to window text {}", common::ucs2_to_utf8(text));

        buf.append(reinterpret_cast<char*>(&text_len), sizeof(std::uint32_t));
        buf.append(reinterpret_cast<char*>(&text[0]), text_len * sizeof(char16_t));

        draw_command command { EWsGcOpDrawTextPtr, buf };
        draw_queue.push(command);
        
        ctx.set_request_status(KErrNone);
    }

    void graphic_context::execute_command(service::ipc_context ctx, ws_cmd cmd) {
        TWsGcOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsGcOpActivate: {
            active(ctx, cmd);
            break;
        }

        // Brush is fill, pen is outline
        case EWsGcOpSetBrushColor: {
            std::string buf;
            buf.resize(sizeof(int));

            std::memcpy(&buf[0], cmd.data_ptr, sizeof(int));

            draw_command command { EWsGcOpSetBrushColor, buf };
            draw_queue.push(command);

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsGcOpSetBrushStyle: {
            LOG_ERROR("SetBrushStyle not supported, stub");
            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsGcOpSetPenStyle: {
            LOG_ERROR("Pen operation not supported yet (wait for ImGui support outline color)");
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsGcOpSetPenColor: {
            LOG_ERROR("Pen operation not supported yet (wait for ImGui support outline color)");
            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsGcOpDrawTextPtr: {
            ws_cmd_draw_text_ptr *draw_text_info = reinterpret_cast<decltype(draw_text_info)>(cmd.data_ptr);

            epoc::desc16 *text_des = draw_text_info->text.get(ctx.msg->own_thr->owning_process());
            std::u16string draw_text = text_des->to_std_string(ctx.msg->own_thr->owning_process());

            do_command_draw_text(ctx, draw_text_info->pos, vec2(-1, -1), draw_text);

            break;
        }

        case EWsGcOpDrawBoxTextPtr: {
            ws_cmd_draw_box_text_ptr *draw_text_info = reinterpret_cast<decltype(draw_text_info)>(cmd.data_ptr);

            epoc::desc16 *text_des = draw_text_info->text.get(ctx.msg->own_thr->owning_process());
            std::u16string draw_text = text_des->to_std_string(ctx.msg->own_thr->owning_process());

            // TODO: align
            do_command_draw_text(ctx, draw_text_info->left_top_pos, draw_text_info->right_bottom_pos, draw_text);

            break;
        }

        default: {
            LOG_WARN("Unimplemented opcode for graphics context operation: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }

    graphic_context::graphic_context(window_server_client_ptr client, screen_device_ptr scr,
        window_ptr win)
        : window_client_obj(client)
        , attached_window(std::reinterpret_pointer_cast<window_user>(win)) {
    }

    void sprite::execute_command(service::ipc_context context, ws_cmd cmd) {
    }

    sprite::sprite(window_server_client_ptr client, window_ptr attached_window,
        eka2l1::vec2 pos)
        : window_client_obj(client)
        , position(pos)
        , attached_window(attached_window) {
    }

    void anim_dll::execute_command(service::ipc_context ctx, ws_cmd cmd) {
        TWsAnimDllOpcode op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsAnimDllOpCreateInstance: {
            LOG_TRACE("AnimDll::CreateInstance stubbed with a anim handle (>= 0)");
            ctx.set_request_status(user_count++);

            break;
        }

        default: {
            LOG_ERROR("Unimplement AnimDll Opcode: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }
    
    void click_dll::execute_command(service::ipc_context ctx, ws_cmd cmd) {
        TWsClickOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsClickOpIsLoaded: {
            ctx.set_request_status(loaded ? 0 : 0x1);
            break;
        }

        case EWsClickOpLoad: {
            int dll_click_name_length = *reinterpret_cast<int*>(cmd.data_ptr);
            char16_t *dll_click_name_ptr = reinterpret_cast<char16_t*>(
                reinterpret_cast<std::uint8_t*>(cmd.data_ptr) + 4
            );

            std::u16string dll_click_name(dll_click_name_ptr, dll_click_name_length);
            LOG_TRACE("Stubbed EWsClickOpLoad (loading click DLL {})", common::ucs2_to_utf8(dll_click_name));

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClickOpCommandReply: {
            LOG_TRACE("ClickOpCommandReply stubbed with KErrNone");
            ctx.set_request_status(KErrNone);

            break;
        }

        default: {
            LOG_ERROR("Unimplement ClickDll Opcode: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }

    bool window::execute_command_for_general_node(eka2l1::service::ipc_context ctx, eka2l1::ws_cmd cmd) {
        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpEnableModifierChangedEvents: {
            epoc::event_mod_notifier_user nof;
            nof.notifier = *reinterpret_cast<event_mod_notifier*>(cmd.data_ptr);
            nof.user = this;

            client->add_event_mod_notifier_user(nof);
            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpSetOrdinalPosition: {
            secondary_priority = *reinterpret_cast<int*>(cmd.data_ptr);
            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpSetOrdinalPositionPri: {
            ws_cmd_ordinal_pos_pri *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
            priority = info->pri1;
            secondary_priority = info->pri2;

            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpSetExtent: {
            ws_cmd_set_extent *extent = reinterpret_cast<decltype(extent)>(cmd.data_ptr);
            
            pos = extent->pos;
            size = extent->size;

            ctx.set_request_status(KErrNone);

            return true;
        }

        case EWsWinOpIdentifier: {
            ctx.set_request_status(static_cast<int>(id));
            return true;
        }

        case EWsWinOpEnableErrorMessages: {
            epoc::event_control ctrl = *reinterpret_cast<epoc::event_control*>(cmd.data_ptr);
            epoc::event_error_msg_user nof;
            nof.when = ctrl;
            nof.user = this;

            client->add_event_error_msg_user(nof);
            ctx.set_request_status(KErrNone);

            return true;
        }

        default: {
            break;
        }
        }

        return false;
    }

    void window_group::execute_command(service::ipc_context ctx, ws_cmd cmd) {
        bool result = execute_command_for_general_node(ctx, cmd);

        if (result) {
            return;
        }
        
        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpEnableScreenChangeEvents: {
            epoc::event_screen_change_user evt;
            evt.user = this;

            client->add_event_screen_change_user(evt);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpSetName: {
            auto name_re = ctx.get_arg<std::u16string>(remote_slot);

            if (!name_re) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            name = std::move(*name_re);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpEnableOnEvents: {
            LOG_TRACE("Currently not support lock/unlock event for window server");
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpReceiveFocus: {
            accept_keyfocus = *reinterpret_cast<bool*>(cmd.data_ptr);

            if (accept_keyfocus) {
                LOG_TRACE("Request group {} to enable keyboard focus",
                    common::ucs2_to_utf8(name));
            } else {
                LOG_TRACE("Request group {} to disable keyboard focus",
                    common::ucs2_to_utf8(name));
            }

            ctx.set_request_status(KErrNone);
            break;
        }

        default: {
            LOG_ERROR("Unimplemented window group opcode 0x{:X}!", cmd.header.op);
            break;
        }
        }
    }
    
    void window_user::execute_command(service::ipc_context ctx, ws_cmd cmd) {
        bool result = execute_command_for_general_node(ctx, cmd);

        if (result) {
            return;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpSetShadowHeight: {
            shadow_height = *reinterpret_cast<int*>(cmd.data_ptr);
            ctx.set_request_status(KErrNone);
            
            break;
        }

        case EWsWinOpShadowDisabled: {
            shadow_disable = *reinterpret_cast<bool*>(cmd.data_ptr);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpSetBackgroundColor: {
            if (cmd.header.cmd_len == 0) {
                clear_color = -1;
                ctx.set_request_status(KErrNone);

                break;
            }

            clear_color = *reinterpret_cast<int*>(cmd.data_ptr);
            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsWinOpPointerFilter: {
            LOG_TRACE("Filtering pointer event type");

            ws_cmd_pointer_filter *filter_info = reinterpret_cast<ws_cmd_pointer_filter*>(cmd.data_ptr);
            filter &= ~filter_info->mask;
            filter |= filter_info->flags;

            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsWinOpSetPointerGrab: {
            allow_pointer_grab = *reinterpret_cast<bool*>(cmd.data_ptr);
            
            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsWinOpActivate: {
            LOG_TRACE("Window activated but redraw not yet implemented");
            activate = true;

            // TODO: Redraw
            ctx.set_request_status(KErrNone);

            break;
        }
        
        case EWsWinOpInvalidate: {
            LOG_INFO("Invalidate stubbed, currently we redraws all the screen");
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpBeginRedraw: {
            for (auto &context: contexts) {
                context->recording = true;

                while (!context->draw_queue.empty()) {
                    context->draw_queue.pop();
                }
            }

            LOG_TRACE("Begin redraw!");
            
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpEndRedraw: {
            for (auto &context: contexts) {
                context->recording = false;
                // context->flush_queue_to_driver();
            }

            LOG_TRACE("End redraw!");
            
            ctx.set_request_status(KErrNone);
            break;
        }

        default: {
            LOG_ERROR("Unimplemented window user opcode 0x{:X}!", cmd.header.op);
            break;
        }
        }
    }
    
    void window_server_client::add_event_mod_notifier_user(epoc::event_mod_notifier_user nof) {
        if (!std::any_of(mod_notifies.begin(), mod_notifies.end(), 
            [=](epoc::event_mod_notifier_user &denof) { return denof.user == nof.user; })) {
            mod_notifies.push_back(nof);
        }
    }
    
    void window_server_client::add_event_screen_change_user(epoc::event_screen_change_user nof) {
        if (!std::any_of(screen_changes.begin(), screen_changes.end(), 
            [=](epoc::event_screen_change_user &denof) { return denof.user == nof.user; })) {
            screen_changes.push_back(nof);
        }
    }

    void window_server_client::add_event_error_msg_user(epoc::event_error_msg_user nof) {
        if (!std::any_of(error_notifies.begin(), error_notifies.end(), 
            [=](epoc::event_error_msg_user &denof) { return denof.user == nof.user; })) {
            error_notifies.push_back(nof);
        }
    }
    
    void window_server_client::parse_command_buffer(service::ipc_context ctx) {
        std::optional<std::string> dat = ctx.get_arg<std::string>(cmd_slot);

        if (!dat) {
            return;
        }

        char *beg = dat->data();
        char *end = dat->data() + dat->size();

        std::vector<ws_cmd> cmds;

        while (beg < end) {
            ws_cmd cmd;

            cmd.header = *reinterpret_cast<ws_cmd_header *>(beg);

            if (cmd.header.op & 0x8000) {
                cmd.header.op &= ~0x8000;
                cmd.obj_handle = *reinterpret_cast<std::uint32_t *>(beg + sizeof(ws_cmd_header));

                beg += sizeof(ws_cmd_header) + sizeof(cmd.obj_handle);
            } else {
                beg += sizeof(ws_cmd_header);
            }

            cmd.data_ptr = reinterpret_cast<void *>(beg);
            beg += cmd.header.cmd_len;

            cmds.push_back(std::move(cmd));
        }

        execute_commands(ctx, std::move(cmds));
    }

    window_server_client::window_server_client(session_ptr guest_session, thread_ptr own_thread)
        : guest_session(guest_session),
          client_thread(own_thread) {
        add_object(std::make_shared<epoc::window>(this));
        root = std::reinterpret_pointer_cast<epoc::window>(objects.back());
    }

    void window_server_client::execute_commands(service::ipc_context ctx, std::vector<ws_cmd> cmds) {
        for (const auto &cmd : cmds) {
            if (cmd.obj_handle == guest_session->unique_id()) {
                execute_command(ctx, cmd);
            } else {
                if (auto obj = get_object(cmd.obj_handle)) {
                    obj->execute_command(ctx, cmd);
                }
            }
        }
    }

    std::uint32_t window_server_client::add_object(window_client_obj_ptr obj) {
        objects.push_back(std::move(obj));
        std::uint32_t de_id = static_cast<std::uint32_t>(base_handle + objects.size());
        objects.back()->id = de_id;

        return de_id;
    }

    window_client_obj_ptr window_server_client::get_object(const std::uint32_t handle) {
        if (handle <= base_handle || handle - base_handle > objects.size()) {
            LOG_WARN("Object handle is invalid {}", handle);
            return nullptr;
        }

        return objects[handle - 1 - base_handle];
    }

    bool window_server_client::delete_object(const std::uint32_t handle) {
        if (handle <= base_handle || handle - base_handle > objects.size()) {
            LOG_WARN("Object handle is invalid {}", handle);
            return false;
        }

        objects[handle - 1 - base_handle].reset();
        return true;
    }
    
    void window_server_client::create_screen_device(service::ipc_context ctx, ws_cmd cmd) {
        LOG_INFO("Create screen device.");

        ws_cmd_screen_device_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);

        epoc::screen_device_ptr device
            = std::make_shared<epoc::screen_device>(
                this, header->num_screen, ctx.sys->get_graphic_driver_client());

        if (!primary_device) {
            primary_device = device;
        }

        init_device(root);
        devices.push_back(device);

        ctx.set_request_status(add_object(device));
    }

    void window_server_client::init_device(epoc::window_ptr &win) {
        if (win->type == epoc::window_kind::group) {
            epoc::window_group_ptr group_win = std::reinterpret_pointer_cast<epoc::window_group>(
                win);

            if (!group_win->dvc) {
                group_win->dvc = primary_device;
            }
        }

        for (auto &child_win : win->childs) {
            init_device(child_win);
        }
    }

    void window_server_client::restore_hotkey(service::ipc_context ctx, ws_cmd cmd) {
        THotKey key = *reinterpret_cast<THotKey *>(cmd.data_ptr);

        LOG_WARN("Unknown restore key op.");
    }

    void window_server_client::create_window_group(service::ipc_context ctx, ws_cmd cmd) {
        ws_cmd_window_group_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);
        int device_handle = header->screen_device_handle;

        epoc::screen_device_ptr device_ptr;

        if (device_handle <= 0) {
            device_ptr = primary_device;
        } else {
            device_ptr = std::reinterpret_pointer_cast<epoc::screen_device>(get_object(device_handle));
        }

        epoc::window_ptr group = std::make_shared<epoc::window_group>(this, device_ptr);
        epoc::window_ptr parent_group = find_window_obj(root, header->parent_id);

        if (!parent_group) {
            LOG_WARN("Unable to find parent for new group with ID = 0x{:x}. Use root", header->parent_id);
            parent_group = root;
        }

        group->parent = parent_group;
        
        if (last_group) {
            last_group->next_sibling = 
                std::reinterpret_pointer_cast<epoc::window_group>(group);
        } 

        parent_group->childs.push(group);

        if (header->focus) {
            device_ptr->focus = std::reinterpret_pointer_cast<epoc::window_group>(group);
            get_ws().focus() = device_ptr->focus;
        }

        last_group = std::reinterpret_pointer_cast<epoc::window_group>(group);
        ctx.set_request_status(add_object(group));
    }
    
    void window_server_client::create_window_base(service::ipc_context ctx, ws_cmd cmd) {
        ws_cmd_window_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);
        
        epoc::window_ptr parent = find_window_obj(root, header->parent);

        if (!parent) {
            LOG_WARN("Unable to find parent for new window with ID = 0x{:x}. Use root", header->parent);
            parent = root;
        }

        epoc::window_ptr win = std::make_shared<epoc::window_user>(this, parent->dvc,
            header->win_type, header->dmode);

        win->parent = parent;
        parent->childs.push(win);

        ctx.set_request_status(add_object(win));
    }

    void window_server_client::create_graphic_context(service::ipc_context ctx, ws_cmd cmd) {
        std::shared_ptr<epoc::graphic_context> gcontext = std::make_shared<epoc::graphic_context>(this);

        ctx.set_request_status(add_object(gcontext));
    }

    void window_server_client::create_sprite(service::ipc_context ctx, ws_cmd cmd) {
        ws_cmd_create_sprite_header *sprite_header = reinterpret_cast<decltype(sprite_header)>(cmd.data_ptr);
        epoc::window_ptr win = nullptr;

        if (sprite_header->window_handle <= 0) {
            LOG_WARN("Window handle is invalid, use root");
            win = root;
        } else {
            win = std::reinterpret_pointer_cast<epoc::window>(get_object(sprite_header->window_handle));
        }

        std::shared_ptr<epoc::sprite> spr = std::make_shared<epoc::sprite>(this, std::move(win), sprite_header->base_pos);
        ctx.set_request_status(add_object(spr));
    }
    
    void window_server_client::create_anim_dll(service::ipc_context ctx, ws_cmd cmd) {
        int dll_name_length = *reinterpret_cast<int*>(cmd.data_ptr);
        char16_t *dll_name_ptr = reinterpret_cast<char16_t*>(
            reinterpret_cast<std::uint8_t*>(cmd.data_ptr) + sizeof(int));

        std::u16string dll_name(dll_name_ptr, dll_name_length);

        LOG_TRACE("Create ANIMDLL for {}, stubbed object", common::ucs2_to_utf8(dll_name));
        
        std::shared_ptr<epoc::anim_dll> animdll = std::make_shared<epoc::anim_dll>(this);
        ctx.set_request_status(add_object(animdll));
    }

    void window_server_client::create_click_dll(service::ipc_context ctx, ws_cmd cmd) {
        LOG_TRACE("Create CLICKDLL (button click sound plugin), stubbed object");
        
        std::shared_ptr<epoc::click_dll> clickdll = std::make_shared<epoc::click_dll>(this);
        ctx.set_request_status(add_object(clickdll));
    }
    
    epoc::window_ptr window_server_client::find_window_obj(epoc::window_ptr &root, std::uint32_t id) {
        if (root->id == id) {
            return root;
        }

        if (root->childs.size() == 0) {
            return nullptr;
        }

        for (auto &child_win : root->childs) {
            epoc::window_ptr obj = find_window_obj(child_win, id);

            if (obj) {
                return obj;
            }
        }

        return nullptr;
    }

    // This handle both sync and async
    void window_server_client::execute_command(service::ipc_context ctx, ws_cmd cmd) {
        switch (cmd.header.op) {
        case EWsClOpComputeMode: {
            LOG_TRACE("Setting compute mode not supported, instead stubbed");
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClOpSetPointerCursorMode: {
            // TODO: Check errors
            if (get_ws().focus() && get_ws().focus()->client == this) {
                get_ws().cursor_mode() = *reinterpret_cast<epoc::pointer_cursor_mode*>(cmd.data_ptr);
                ctx.set_request_status(KErrNone);

                break;
            }

            ctx.set_request_status(KErrPermissionDenied);

            break;
        }

        case EWsClOpGetWindowGroupClientThreadId: {
            std::uint32_t group_id = *reinterpret_cast<std::uint32_t*>(cmd.data_ptr);
            epoc::window_ptr win = find_window_obj(root, group_id);

            if (!win || win->type != window_kind::group) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            std::uint32_t thr_id = win->client->get_client()->unique_id();

            ctx.write_arg_pkg<std::uint32_t>(reply_slot, thr_id);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClOpCreateScreenDevice:
            create_screen_device(ctx, cmd);
            break;

        case EWsClOpCreateWindowGroup:
            create_window_group(ctx, cmd);
            break;

        case EWsClOpCreateWindow:
            create_window_base(ctx, cmd);
            break;
        
        case EWsClOpRestoreDefaultHotKey:
            restore_hotkey(ctx, cmd);
            break;

        case EWsClOpCreateGc:
            create_graphic_context(ctx, cmd);
            break;

        case EWsClOpCreateSprite:
            create_sprite(ctx, cmd);
            break;
            
        case EWsClOpCreateAnimDll:
            create_anim_dll(ctx, cmd);
            break;

        case EWsClOpCreateClick: 
            create_click_dll(ctx, cmd);
            break;

        case EWsClOpEventReady:
            break;

        case EWsClOpGetFocusWindowGroup: {
            // TODO: Epoc < 9
            if (cmd.header.cmd_len == 0) {
                ctx.set_request_status(primary_device->focus->id);
                break;
            }

            int screen_num = *reinterpret_cast<int*>(cmd.data_ptr);

            auto dvc_ite = std::find_if(devices.begin(), devices.end(), 
                [screen_num](const epoc::screen_device_ptr &dvc) { return dvc->screen == screen_num; });

            if (dvc_ite == devices.end()) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            ctx.set_request_status((*dvc_ite)->focus->id);
            break;
        }
        
        case EWsClOpFindWindowGroupIdentifier: {
            ws_cmd_find_window_group_identifier *find_info = 
                reinterpret_cast<decltype(find_info)>(cmd.data_ptr);

            epoc::window_group_ptr group = nullptr;

            if (find_info->parent_identifier) {
                group = std::reinterpret_pointer_cast<epoc::window_group>
                    (find_window_obj(root, find_info->parent_identifier));
            } else {
                LOG_TRACE("Parent identifier not specified, use root window group");
                group = std::reinterpret_pointer_cast<epoc::window_group>(
                   *root->childs.begin());
            }

            if (!group || group->type != window_kind::group) {
                ctx.set_request_status(KErrNotFound);
                break;
            }

            char16_t *win_group_name_ptr = reinterpret_cast<char16_t*>(find_info + 1);
            std::u16string win_group_name(win_group_name_ptr, find_info->length);

            bool found = false;

            for (; group; group = group->next_sibling) {
                if (common::compare_ignore_case(group->name.substr(find_info->offset),
                    win_group_name) == 0) {
                    ctx.set_request_status(group->id);
                    found = true;

                    break;
                }
            }

            if (!found) {
                ctx.set_request_status(KErrNotFound);
            }

            break;
        }

        default:
            LOG_INFO("Unimplemented ClOp: 0x{:x}", cmd.header.op);
        }
    }
}

namespace eka2l1 {
    void window_server::load_wsini() {
        io_system *io = sys->get_io_system();
        std::optional<eka2l1::drive> drv;
        drive_number dn = drive_z;

        for (; dn >= drive_a; dn = (drive_number)((int)dn - 1)) {
            drv = io->get_drive_entry(dn);

            if (drv && drv->media_type == drive_media::rom) {
                break;
            }
        }

        std::u16string wsini_path;
        wsini_path += static_cast<char16_t>((char)dn + 'A');
        wsini_path += u":\\system\\data\\wsini.ini";

        auto wsini_path_host = io->get_raw_path(wsini_path);

        if (!wsini_path_host) {
            LOG_ERROR("Can't find the window config file, app using window server will broken");
            return;
        }

        std::string wsini_path_host_utf8 = common::ucs2_to_utf8(*wsini_path_host);
        int err = ws_config.load(wsini_path_host_utf8.c_str());

        if (err != 0) {
            LOG_ERROR("Loading wsini file broke with code {}", err);
        }
    }
    
    void window_server::parse_wsini() {
        common::ini_node_ptr screen_node = nullptr;
        int total_screen = 0;

        do {
            std::string screen_key = "SCREEN";
            screen_key += std::to_string(total_screen);
            screen_node = ws_config.find(screen_key.c_str());

            if (!screen_node || !common::ini_section::is_my_type(screen_node)) {
                break;
            }

            total_screen++;
            epoc::config::screen scr;

            scr.screen_number = total_screen - 1;

            int total_mode = 0;

            do {
                std::string screen_mode_width_key = "SCR_WIDTH";
                screen_mode_width_key += std::to_string(total_mode + 1);

                common::ini_node_ptr mode_width = screen_node->
                    get_as<common::ini_section>().find(screen_mode_width_key.c_str());
                
                if (!mode_width) {
                    break;
                }

                std::string screen_mode_height_key = "SCR_HEIGHT";
                screen_mode_height_key += std::to_string(total_mode + 1);

                common::ini_node_ptr mode_height = screen_node->
                    get_as<common::ini_section>().find(screen_mode_height_key.c_str());

                total_mode++;

                epoc::config::screen_mode   scr_mode;
                scr_mode.screen_number = total_screen - 1;
                scr_mode.mode_number = total_mode;

                mode_width->get_as<common::ini_pair>().get(reinterpret_cast<std::uint32_t*>(&scr_mode.size.x),
                    1, 0);
                mode_height->get_as<common::ini_pair>().get(reinterpret_cast<std::uint32_t*>(&scr_mode.size.y),
                    1, 0);
                
                std::string screen_mode_rot_key = "SCR_ROTATION";
                screen_mode_rot_key += std::to_string(total_mode);

                common::ini_node_ptr mode_rot = screen_node->
                    get_as<common::ini_section>().find(screen_mode_rot_key.c_str());

                mode_rot->get_as<common::ini_pair>().get(reinterpret_cast<std::uint32_t*>(&scr_mode.rotation),
                    1, 0);

                scr.modes.push_back(scr_mode);
            } while (true);

            screens.push_back(scr);
        } while (screen_node != nullptr);
    }

    window_server::window_server(system *sys)
        : service::server(sys, "!Windowserver", true, true) {
        REGISTER_IPC(window_server, init, EWservMessInit,
            "Ws::Init");
        REGISTER_IPC(window_server, send_to_command_buffer, EWservMessCommandBuffer,
            "Ws::CommandBuffer");
        REGISTER_IPC(window_server, send_to_command_buffer, EWservMessSyncMsgBuf,
            "Ws::MessSyncBuf");
    }

    void window_server::init(service::ipc_context ctx) {
        if (!loaded) {
            load_wsini();
            parse_wsini();

            loaded = true;
        }

        clients.emplace(ctx.msg->msg_session->unique_id(), 
            std::make_shared<epoc::window_server_client>(ctx.msg->msg_session, ctx.msg->own_thr));

        ctx.set_request_status(ctx.msg->msg_session->unique_id());
    }

    void window_server::send_to_command_buffer(service::ipc_context ctx) {
        clients[ctx.msg->msg_session->unique_id()]->parse_command_buffer(ctx);
    }

    void window_server::add_to_notify(event_notify_info info) {
        statuses.push_back(info);
    }

    void window_server::on_unhandled_opcode(service::ipc_context ctx) {
        if (ctx.msg->function & EWservMessAsynchronousService) {
            switch (ctx.msg->function & ~EWservMessAsynchronousService) {
            case EWsClOpRedrawReady: {
                LOG_TRACE("Redraw ready");
                ctx.set_request_status(KErrNone);

                break;
            }

            // Notify when an event is ringing, means that whenever
            // is occured within an object that belongs to a client that
            // created by the same thread as the requester, that requester
            // will be notify
            case EWsClOpEventReady: {
                event_notify_info info;
                info.requester = ctx.msg->own_thr;
                info.sts = ctx.msg->request_sts;

                add_to_notify(info);

                break;
            }

            default: {
                LOG_TRACE("UNHANDLE ASYNC OPCODE: {}",
                    ctx.msg->function & ~EWservMessAsynchronousService);
    
                break;
            }
            }
        }
    }
}