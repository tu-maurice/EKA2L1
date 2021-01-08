/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <cpu/12l1r/common.h>
#include <cpu/12l1r/arm_visitor.h>
#include <cpu/12l1r/thumb_visitor.h>
#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/visit_session.h>

namespace eka2l1::arm::r12l1 {
    bool arm_translate_visitor::arm_MOV_imm(common::cc_flags cond, bool S, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);

        if (dest_real == common::armgen::R15) {
            LOG_TRACE(CPU_12L1R, "Undefined behaviour mov to r15, todo!");
            emit_undefined_instruction_handler();

            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
                ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 imm_op(imm8, static_cast<std::uint8_t>(rotate));

        if (S) {
            big_block_->MOVS(dest_mapped, imm_op);
            cpsr_nzcv_changed();
        } else {
            big_block_->MOV(dest_mapped, imm_op);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MOV_reg(common::cc_flags cond, bool S, reg_index d, std::uint8_t imm5,
                     common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);
        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(source_mapped);
            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
                ALLOCATE_FLAG_DIRTY);

        if (source_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, expand_arm_shift(crr_block_->current_address() + 8,
                shift, imm5));

            return true;
        }

        common::armgen::operand2 imm_op(source_mapped, shift, imm5);

        if (S) {
            big_block_->MOVS(dest_mapped, imm_op);
            cpsr_nzcv_changed();
        } else {
            big_block_->MOV(dest_mapped, imm_op);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MVN_imm(common::cc_flags cond, bool S, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);

        if (dest_real == common::armgen::R15) {
            LOG_TRACE(CPU_12L1R, "Undefined behaviour mvn to r15, todo!");
            emit_undefined_instruction_handler();

            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
            ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 imm_op(imm8, static_cast<std::uint8_t>(rotate));

        if (S) {
            big_block_->MVNS(dest_mapped, imm_op);
            cpsr_nzcv_changed();
        } else {
            big_block_->MVN(dest_mapped, imm_op);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MVN_reg(common::cc_flags cond, bool S, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);
        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(source_mapped);
            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
            ALLOCATE_FLAG_DIRTY);

        if (source_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, ~expand_arm_shift(crr_block_->current_address() + 8,
                shift, imm5));

            return true;
        }

        common::armgen::operand2 imm_op(source_mapped, shift, imm5);

        if (S) {
            big_block_->MVNS(dest_mapped, imm_op);
            cpsr_nzcv_changed();
        } else {
            big_block_->MVN(dest_mapped, imm_op);
        }

        return true;
    }

    bool arm_translate_visitor::arm_ADD_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
        int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOV(dest_mapped, crr_block_->current_address() + 8 + expand_arm_imm(imm8, rotate));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->ADDS(dest_mapped, op1_mapped, op2);
                cpsr_nzcv_changed();
            } else {
                big_block_->ADD(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ADD_reg(common::cc_flags cond, bool S, reg_index n, reg_index d,
            std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm ADD op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->ADDS(dest_mapped, op1_mapped, op2);
            cpsr_nzcv_changed();
        } else {
            big_block_->ADD(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_SUB_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
        int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOV(dest_mapped, crr_block_->current_address() + 8 - expand_arm_imm(imm8, rotate));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->SUBS(dest_mapped, op1_mapped, op2);
                cpsr_nzcv_changed();
            } else {
                big_block_->SUB(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_SUB_reg(common::cc_flags cond, bool S, reg_index n, reg_index d,
            std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm SUB op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->SUBS(dest_mapped, op1_mapped, op2);
            cpsr_nzcv_changed();
        } else {
            big_block_->SUB(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_BIC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOV(dest_mapped, ((crr_block_->current_address() + 8) & ~(expand_arm_imm(imm8, rotate))));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->BICS(dest_mapped, op1_mapped, op2);
                cpsr_nzcv_changed();
            } else {
                big_block_->BIC(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_BIC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm BIC op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->BICS(dest_mapped, op1_mapped, op2);
            cpsr_nzcv_changed();
        } else {
            big_block_->BIC(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_BIC_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op_shift_real = reg_index_to_gpr(s);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm BIC op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        const common::armgen::arm_reg op_shift_mapped = reg_supplier_.map(op_shift_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, op_shift_mapped);

        if (S) {
            big_block_->BICS(dest_mapped, op1_mapped, op2);
            cpsr_nzcv_changed();
        } else {
            big_block_->BIC(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ORR_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOV(dest_mapped, ((crr_block_->current_address() + 8) | (expand_arm_imm(imm8, rotate))));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->ORRS(dest_mapped, op1_mapped, op2);
                cpsr_nzcv_changed();
            } else {
                big_block_->ORR(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ORR_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm ORR op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->ORRS(dest_mapped, op1_mapped, op2);
            cpsr_nzcv_changed();
        } else {
            big_block_->ORR(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ORR_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op_shift_real = reg_index_to_gpr(s);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm ORR op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        const common::armgen::arm_reg op_shift_mapped = reg_supplier_.map(op_shift_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, op_shift_mapped);

        if (S) {
            big_block_->ORRS(dest_mapped, op1_mapped, op2);
            cpsr_nzcv_changed();
        } else {
            big_block_->ORR(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_reg_link_exchange(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_CMP_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::operand2 rhs(imm8, static_cast<std::uint8_t>(rotate));

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);

        big_block_->CMP(lhs_mapped, rhs);
        cpsr_nzcv_changed();

        return true;
    }

    bool arm_translate_visitor::arm_CMP_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_base_mapped = reg_supplier_.map(rhs_base_real, 0);

        common::armgen::operand2 rhs(rhs_base_mapped, shift, imm5);

        big_block_->CMP(lhs_mapped, rhs);
        cpsr_nzcv_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_MOV_imm(reg_index d, std::uint8_t imm8) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 imm_op(imm8, 0);

        big_block_->MOVS(dest_mapped, imm_op);
        cpsr_nzcv_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_imm_t1(std::uint8_t imm3, reg_index n, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::operand2 op2(imm3, 0);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

        big_block_->ADDS(dest_mapped, op1_mapped, op2);
        cpsr_nzcv_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_imm_t2(reg_index d_n, std::uint8_t imm8) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::operand2 op2(imm8, 0);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);

        big_block_->ADDS(dest_and_op1_mapped, dest_and_op1_mapped, op2);
        cpsr_nzcv_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_sp_t1(reg_index d, std::uint8_t imm8) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);

        common::armgen::arm_reg sp_mapped = reg_supplier_.map(common::armgen::R13, 0);
        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        big_block_->ADDI2R(dest_mapped, sp_mapped, imm8 << 2, ALWAYS_SCRATCH1);
        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_sp_t2(std::uint8_t imm7) {
        const common::armgen::arm_reg sp_mapped = reg_supplier_.map(common::armgen::R13,
            ALLOCATE_FLAG_DIRTY);

        big_block_->ADDI2R(sp_mapped, sp_mapped, imm7 << 2, ALWAYS_SCRATCH1);
        return true;
    }

    bool thumb_translate_visitor::thumb16_SUB_imm_t1(std::uint8_t imm3, reg_index n, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::operand2 op2(imm3, 0);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

        big_block_->SUBS(dest_mapped, op1_mapped, op2);
        cpsr_nzcv_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_SUB_imm_t2(reg_index d_n, std::uint8_t imm8) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::operand2 op2(imm8, 0);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);

        big_block_->SUBS(dest_and_op1_mapped, dest_and_op1_mapped, op2);
        cpsr_nzcv_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_SUB_sp(std::uint8_t imm7) {
        const common::armgen::arm_reg sp_mapped = reg_supplier_.map(common::armgen::R13,
            ALLOCATE_FLAG_DIRTY);

        big_block_->SUBI2R(sp_mapped, sp_mapped, imm7 << 2, ALWAYS_SCRATCH1);
        return true;
    }

    bool thumb_translate_visitor::thumb16_CMP_imm(reg_index n, std::uint8_t imm8) {
        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::operand2 rhs(imm8, 0);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        big_block_->CMP(lhs_mapped, rhs);

        cpsr_nzcv_changed();
        return true;
    }
}