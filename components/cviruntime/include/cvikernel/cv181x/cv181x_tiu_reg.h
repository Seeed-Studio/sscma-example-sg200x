#ifndef CV181X_TIU_REG_H
#define CV181X_TIU_REG_H

/*
 * This file is generated by tools. Do not edit it manually.
 */

#include <stdint.h>
#include <stdio.h>

#define TIU_DESC_REG_BYTES (0x70)
#define TIU_ENGINE_DESCRIPTOR_NUM       (TIU_DESC_REG_BYTES >> 2)

// TIU operation data type
#define DCR_TYPE_CONV_FIX8B           0
#define DCR_TYPE_DEPTHWISE_POOL_FIX8B 1
#define DCR_TYPE_FC_FIX8B             2
#define DCR_TYPE_TENSOR_ARITH_FIX8B   3
#define NR_DCR_TYPES                  4

#define TENSOR_MUL_FIX8B              0
#define TENSOR_MAC_FIX8B              1
#define TENSOR_ADD_FIX8B              2
#define TENSOR_SUB_FIX8B              3
#define TENSOR_MAX_FIX8B              4
#define TENSOR_MIN_FIX8B              5
#define TENSOR_SHIFT_FIX8B            6
#define TENSOR_AND_FIX8B              7
#define TENSOR_OR_FIX8B               8
#define TENSOR_XOR_FIX8B              9
#define TENSOR_COPY_FIX8B             10
#define TENSOR_GE_FIX8B               11

typedef unsigned long long ullong;

typedef struct {
  uint32_t cmd_en;
  uint32_t cmd_end;
  uint32_t cmd_id_en;
  uint32_t cmd_keep;
  uint32_t cmd_intr_en;
  uint32_t tsk_typ;
  uint32_t tsk_eu_typ;
  uint32_t tsk_opd_num;
  uint32_t opt_res_shift;
  uint32_t opt_left_shift;
  uint32_t opt_shift_typ;
  uint32_t opt_rshift_typ;
  uint32_t dummy1;
  uint32_t opd_typ;
  uint32_t opt_chl_quan;
  uint32_t cmd_id_tpu;
  uint32_t cmd_id_gdma;
  uint32_t quan_m;
  uint32_t opt_res0_sign;
  uint32_t opt_opd0_sign;
  uint32_t opt_opd1_sign;
  uint32_t opt_opd2_sign;
  uint32_t opt_res0_seg;
  uint32_t opt_opd0_seg;
  uint32_t opt_opd1_seg;
  uint32_t opt_opd2_seg;
  uint32_t ps32_md;
  uint32_t double_conv;
  uint32_t opt_left_tran;
  uint32_t fp_round_typ;
  uint32_t opt_relu_typ;
  uint32_t opt_relu_value;
  uint32_t cmd_pre_exe_typ;
  uint32_t opt_res_add;
  uint32_t rsvd0;
  uint32_t conv_opd0_x_ins0;
  uint32_t conv_opd0_y_ins0;
  uint32_t conv_opd0_x_ins0_last;
  uint32_t conv_opd0_y_ins0_last;
  uint32_t conv_opd1_x_ins0;
  uint32_t conv_opd1_y_ins0;
  uint32_t dummy0;
  uint32_t opd0_ins_val;
  uint32_t conv_opd0_up_pad;
  uint32_t conv_opd0_dn_pad;
  uint32_t conv_opd0_lf_pad;
  uint32_t conv_opd0_rt_pad;
  uint32_t res0_n;
  uint32_t res0_c;
  uint32_t res0_h;
  uint32_t res0_w;
  uint32_t conv_op_x_str;
  uint32_t conv_op_y_str;
  uint32_t cmd_pre_exe;
  uint32_t rsvd1;
  uint32_t res0_addr;
  uint32_t opd0_addr;
  uint32_t opd1_addr;
  uint32_t opd2_addr;
  uint32_t opt_opd0_const;
  uint32_t opt_opd1_const;
  uint32_t opt_opd2_const;
  uint32_t short_nchwstr_same;
  uint32_t short_res0_str;
  uint32_t short_opd0_str;
  uint32_t short_opd1_str;
  uint32_t short_opd2_str;
  uint32_t dummy2;
  uint32_t opd0_n;
  uint32_t opd0_c;
  uint32_t dummy3;
  uint32_t rsvd2;
  uint32_t opd0_h;
  uint32_t opd0_w;
  uint32_t opd1_n;
  uint32_t opd1_c;
  uint32_t opd1_h;
  uint32_t opd1_w;
  uint32_t opd2_n;
  uint32_t opd2_c;
  uint32_t opd2_h;
  uint32_t opd2_w;
  uint32_t dummy4;
  uint32_t rsvd3;
  uint32_t layer_info;
  uint32_t res0_n_str;
  uint32_t res0_c_str;
  uint32_t res0_h_str;
  uint32_t res0_w_str;
  uint32_t res0_b_str;
  uint32_t opd0_n_str;
  uint32_t dummy5;
  uint32_t rsvd4;
  uint32_t opd0_c_str;
  uint32_t opd0_h_str;
  uint32_t opd0_w_str;
  uint32_t opd0_b_str;
  uint32_t opd1_n_str;
  uint32_t opd1_c_str;
  uint32_t opd1_h_str;
  uint32_t dummy6;
  uint32_t rsvd5;
  uint32_t opd1_w_str;
  uint32_t opd1_b_str;
  uint32_t opd2_n_str;
  uint32_t opd2_c_str;
  uint32_t opd2_h_str;
  uint32_t opd2_w_str;
  uint32_t opd2_b_str;
  uint32_t dummy7;
  uint32_t rsvd6;
} tiu_reg_t;

static inline void parse_tiu_reg(tiu_reg_t *r, const uint32_t *p)
{
  r->cmd_en = p[0] & 1;
  r->cmd_end = (p[0] >> 1) & 1;
  r->cmd_id_en = (p[0] >> 2) & 1;
  r->cmd_keep = (p[0] >> 3) & 1;
  r->cmd_intr_en = (p[0] >> 4) & 1;
  r->tsk_typ = (p[0] >> 5) & ((1u << 4) - 1);
  r->tsk_eu_typ = (p[0] >> 9) & ((1u << 5) - 1);
  r->tsk_opd_num = (p[0] >> 14) & ((1u << 2) - 1);
  r->opt_res_shift = (p[0] >> 16) & ((1u << 6) - 1);
  r->opt_left_shift = (p[0] >> 22) & ((1u << 5) - 1);
  r->opt_shift_typ = (p[0] >> 27) & 1;
  r->opt_rshift_typ = (p[0] >> 28) & 1;
  r->dummy1 = (p[0] >> 29) & 1;
  r->opd_typ = (p[0] >> 30) & 1;
  r->opt_chl_quan = (p[0] >> 31) & 1;
  r->cmd_id_tpu = p[1] & ((1u << 16) - 1);
  r->cmd_id_gdma = (p[1] >> 16) & ((1u << 16) - 1);
  r->quan_m = p[2];
  r->opt_res0_sign = p[3] & 1;
  r->opt_opd0_sign = (p[3] >> 1) & 1;
  r->opt_opd1_sign = (p[3] >> 2) & 1;
  r->opt_opd2_sign = (p[3] >> 3) & 1;
  r->opt_res0_seg = (p[3] >> 4) & ((1u << 2) - 1);
  r->opt_opd0_seg = (p[3] >> 6) & ((1u << 2) - 1);
  r->opt_opd1_seg = (p[3] >> 8) & ((1u << 2) - 1);
  r->opt_opd2_seg = (p[3] >> 10) & 1;
  r->ps32_md = (p[3] >> 11) & ((1u << 2) - 1);
  r->double_conv = (p[3] >> 13) & 1;
  r->opt_left_tran = (p[3] >> 14) & 1;
  r->fp_round_typ = (p[3] >> 15) & 1;
  r->opt_relu_typ = (p[3] >> 16) & ((1u << 2) - 1);
  r->opt_relu_value = (p[3] >> 18) & ((1u << 8) - 1);
  r->cmd_pre_exe_typ = (p[3] >> 26) & 1;
  r->opt_res_add = (p[3] >> 27) & 1;
  r->rsvd0 = (p[3] >> 28) & ((1u << 4) - 1);
  r->conv_opd0_x_ins0 = p[4] & ((1u << 4) - 1);
  r->conv_opd0_y_ins0 = (p[4] >> 4) & ((1u << 4) - 1);
  r->conv_opd0_x_ins0_last = (p[4] >> 8) & ((1u << 4) - 1);
  r->conv_opd0_y_ins0_last = (p[4] >> 12) & ((1u << 4) - 1);
  r->conv_opd1_x_ins0 = (p[4] >> 16) & ((1u << 4) - 1);
  r->conv_opd1_y_ins0 = (p[4] >> 20) & ((1u << 4) - 1);
  r->dummy0 = (p[4] >> 24) & ((1u << 8) - 1);
  r->opd0_ins_val = p[5] & ((1u << 16) - 1);
  r->conv_opd0_up_pad = (p[5] >> 16) & ((1u << 4) - 1);
  r->conv_opd0_dn_pad = (p[5] >> 20) & ((1u << 4) - 1);
  r->conv_opd0_lf_pad = (p[5] >> 24) & ((1u << 4) - 1);
  r->conv_opd0_rt_pad = (p[5] >> 28) & ((1u << 4) - 1);
  r->res0_n = p[6] & ((1u << 12) - 1);
  r->res0_c = (p[6] >> 12) & ((1u << 12) - 1);
  r->res0_h = (p[6] >> 24) & ((1u << 8) - 1);
  r->res0_h |= (uint64_t)(p[7] & ((1u << 4) - 1)) << 8;
  r->res0_w = (p[7] >> 4) & ((1u << 12) - 1);
  r->conv_op_x_str = (p[7] >> 16) & ((1u << 5) - 1);
  r->conv_op_y_str = (p[7] >> 21) & ((1u << 5) - 1);
  r->cmd_pre_exe = (p[7] >> 26) & ((1u << 2) - 1);
  r->rsvd1 = (p[7] >> 28) & ((1u << 4) - 1);
  r->res0_addr = p[8] & ((1u << 24) - 1);
  r->opd0_addr = (p[8] >> 24) & ((1u << 8) - 1);
  r->opd0_addr |= (uint64_t)(p[9] & ((1u << 16) - 1)) << 8;
  r->opd1_addr = (p[9] >> 16) & ((1u << 16) - 1);
  r->opd2_addr = p[10] & ((1u << 16) - 1);
  r->opt_opd0_const = (p[10] >> 16) & 1;
  r->opt_opd1_const = (p[10] >> 17) & 1;
  r->opt_opd2_const = (p[10] >> 18) & 1;
  r->short_nchwstr_same = (p[10] >> 19) & 1;
  r->short_res0_str = (p[10] >> 20) & ((1u << 2) - 1);
  r->short_opd0_str = (p[10] >> 22) & ((1u << 2) - 1);
  r->short_opd1_str = (p[10] >> 24) & ((1u << 2) - 1);
  r->short_opd2_str = (p[10] >> 26) & ((1u << 2) - 1);
  r->dummy2 = (p[10] >> 28) & ((1u << 4) - 1);
  r->opd0_n = p[11] & ((1u << 12) - 1);
  r->opd0_c = (p[11] >> 12) & ((1u << 12) - 1);
  r->dummy3 = (p[11] >> 24) & ((1u << 4) - 1);
  r->rsvd2 = (p[11] >> 28) & ((1u << 4) - 1);
  r->opd0_h = p[12] & ((1u << 12) - 1);
  r->opd0_w = (p[12] >> 12) & ((1u << 12) - 1);
  r->opd1_n = (p[12] >> 24) & ((1u << 8) - 1);
  r->opd1_n |= (uint64_t)(p[13] & ((1u << 4) - 1)) << 8;
  r->opd1_c = (p[13] >> 4) & ((1u << 12) - 1);
  r->opd1_h = (p[13] >> 16) & ((1u << 12) - 1);
  r->opd1_w = (p[13] >> 28) & ((1u << 4) - 1);
  r->opd1_w |= (uint64_t)(p[14] & ((1u << 8) - 1)) << 4;
  r->opd2_n = (p[14] >> 8) & ((1u << 12) - 1);
  r->opd2_c = (p[14] >> 20) & ((1u << 12) - 1);
  r->opd2_h = p[15] & ((1u << 12) - 1);
  r->opd2_w = (p[15] >> 12) & ((1u << 12) - 1);
  r->dummy4 = (p[15] >> 24) & ((1u << 4) - 1);
  r->rsvd3 = (p[15] >> 28) & ((1u << 4) - 1);
  r->layer_info = p[16] & ((1u << 16) - 1);
  r->res0_n_str = (p[16] >> 16) & ((1u << 16) - 1);
  r->res0_c_str = p[17] & ((1u << 16) - 1);
  r->res0_h_str = (p[17] >> 16) & ((1u << 16) - 1);
  r->res0_w_str = p[18] & ((1u << 16) - 1);
  r->res0_b_str = (p[18] >> 16) & ((1u << 16) - 1);
  r->opd0_n_str = p[19] & ((1u << 16) - 1);
  r->dummy5 = (p[19] >> 16) & ((1u << 12) - 1);
  r->rsvd4 = (p[19] >> 28) & ((1u << 4) - 1);
  r->opd0_c_str = p[20] & ((1u << 16) - 1);
  r->opd0_h_str = (p[20] >> 16) & ((1u << 16) - 1);
  r->opd0_w_str = p[21] & ((1u << 16) - 1);
  r->opd0_b_str = (p[21] >> 16) & ((1u << 16) - 1);
  r->opd1_n_str = p[22] & ((1u << 16) - 1);
  r->opd1_c_str = (p[22] >> 16) & ((1u << 16) - 1);
  r->opd1_h_str = p[23] & ((1u << 16) - 1);
  r->dummy6 = (p[23] >> 16) & ((1u << 12) - 1);
  r->rsvd5 = (p[23] >> 28) & ((1u << 4) - 1);
  r->opd1_w_str = p[24] & ((1u << 16) - 1);
  r->opd1_b_str = (p[24] >> 16) & ((1u << 16) - 1);
  r->opd2_n_str = p[25] & ((1u << 16) - 1);
  r->opd2_c_str = (p[25] >> 16) & ((1u << 16) - 1);
  r->opd2_h_str = p[26] & ((1u << 16) - 1);
  r->opd2_w_str = (p[26] >> 16) & ((1u << 16) - 1);
  r->opd2_b_str = p[27] & ((1u << 16) - 1);
  r->dummy7 = (p[27] >> 16) & ((1u << 12) - 1);
  r->rsvd6 = (p[27] >> 28) & ((1u << 4) - 1);
}

static inline void emit_tiu_reg(const tiu_reg_t *r, uint32_t *_p)
{
  volatile uint32_t *p = (typeof(p))_p;
  p[27] = (r->opd2_b_str & ((1u << 16) - 1)) |
          ((r->dummy7 & ((1u << 12) - 1)) << 16) |
          ((r->rsvd6 & ((1u << 4) - 1)) << 28);
  p[26] = (r->opd2_h_str & ((1u << 16) - 1)) |
          ((r->opd2_w_str & ((1u << 16) - 1)) << 16);
  p[25] = (r->opd2_n_str & ((1u << 16) - 1)) |
          ((r->opd2_c_str & ((1u << 16) - 1)) << 16);
  p[24] = (r->opd1_w_str & ((1u << 16) - 1)) |
          ((r->opd1_b_str & ((1u << 16) - 1)) << 16);
  p[23] = (r->opd1_h_str & ((1u << 16) - 1)) |
          ((r->dummy6 & ((1u << 12) - 1)) << 16) |
          ((r->rsvd5 & ((1u << 4) - 1)) << 28);
  p[22] = (r->opd1_n_str & ((1u << 16) - 1)) |
          ((r->opd1_c_str & ((1u << 16) - 1)) << 16);
  p[21] = (r->opd0_w_str & ((1u << 16) - 1)) |
          ((r->opd0_b_str & ((1u << 16) - 1)) << 16);
  p[20] = (r->opd0_c_str & ((1u << 16) - 1)) |
          ((r->opd0_h_str & ((1u << 16) - 1)) << 16);
  p[19] = (r->opd0_n_str & ((1u << 16) - 1)) |
          ((r->dummy5 & ((1u << 12) - 1)) << 16) |
          ((r->rsvd4 & ((1u << 4) - 1)) << 28);
  p[18] = (r->res0_w_str & ((1u << 16) - 1)) |
          ((r->res0_b_str & ((1u << 16) - 1)) << 16);
  p[17] = (r->res0_c_str & ((1u << 16) - 1)) |
          ((r->res0_h_str & ((1u << 16) - 1)) << 16);
  p[16] = (r->layer_info & ((1u << 16) - 1)) |
          ((r->res0_n_str & ((1u << 16) - 1)) << 16);
  p[15] = (r->opd2_h & ((1u << 12) - 1)) |
          ((r->opd2_w & ((1u << 12) - 1)) << 12) |
          ((r->dummy4 & ((1u << 4) - 1)) << 24) |
          ((r->rsvd3 & ((1u << 4) - 1)) << 28);
  p[14] = ((r->opd1_w >> 4) & ((1u << 8) - 1)) |
          ((r->opd2_n & ((1u << 12) - 1)) << 8) |
          ((r->opd2_c & ((1u << 12) - 1)) << 20);
  p[13] = ((r->opd1_n >> 8) & ((1u << 4) - 1)) |
          ((r->opd1_c & ((1u << 12) - 1)) << 4) |
          ((r->opd1_h & ((1u << 12) - 1)) << 16) |
          ((r->opd1_w & ((1u << 4) - 1)) << 28);
  p[12] = (r->opd0_h & ((1u << 12) - 1)) |
          ((r->opd0_w & ((1u << 12) - 1)) << 12) |
          ((r->opd1_n & ((1u << 8) - 1)) << 24);
  p[11] = (r->opd0_n & ((1u << 12) - 1)) |
          ((r->opd0_c & ((1u << 12) - 1)) << 12) |
          ((r->dummy3 & ((1u << 4) - 1)) << 24) |
          ((r->rsvd2 & ((1u << 4) - 1)) << 28);
  p[10] = (r->opd2_addr & ((1u << 16) - 1)) |
          ((r->opt_opd0_const & 1) << 16) |
          ((r->opt_opd1_const & 1) << 17) |
          ((r->opt_opd2_const & 1) << 18) |
          ((r->short_nchwstr_same & 1) << 19) |
          ((r->short_res0_str & ((1u << 2) - 1)) << 20) |
          ((r->short_opd0_str & ((1u << 2) - 1)) << 22) |
          ((r->short_opd1_str & ((1u << 2) - 1)) << 24) |
          ((r->short_opd2_str & ((1u << 2) - 1)) << 26) |
          ((r->dummy2 & ((1u << 4) - 1)) << 28);
  p[9] = ((r->opd0_addr >> 8) & ((1u << 16) - 1)) |
         ((r->opd1_addr & ((1u << 16) - 1)) << 16);
  p[8] = (r->res0_addr & ((1u << 24) - 1)) |
         ((r->opd0_addr & ((1u << 8) - 1)) << 24);
  p[7] = ((r->res0_h >> 8) & ((1u << 4) - 1)) |
         ((r->res0_w & ((1u << 12) - 1)) << 4) |
         ((r->conv_op_x_str & ((1u << 5) - 1)) << 16) |
         ((r->conv_op_y_str & ((1u << 5) - 1)) << 21) |
         ((r->cmd_pre_exe & ((1u << 2) - 1)) << 26) |
         ((r->rsvd1 & ((1u << 4) - 1)) << 28);
  p[6] = (r->res0_n & ((1u << 12) - 1)) |
         ((r->res0_c & ((1u << 12) - 1)) << 12) |
         ((r->res0_h & ((1u << 8) - 1)) << 24);
  p[5] = (r->opd0_ins_val & ((1u << 16) - 1)) |
         ((r->conv_opd0_up_pad & ((1u << 4) - 1)) << 16) |
         ((r->conv_opd0_dn_pad & ((1u << 4) - 1)) << 20) |
         ((r->conv_opd0_lf_pad & ((1u << 4) - 1)) << 24) |
         ((r->conv_opd0_rt_pad & ((1u << 4) - 1)) << 28);
  p[4] = (r->conv_opd0_x_ins0 & ((1u << 4) - 1)) |
         ((r->conv_opd0_y_ins0 & ((1u << 4) - 1)) << 4) |
         ((r->conv_opd0_x_ins0_last & ((1u << 4) - 1)) << 8) |
         ((r->conv_opd0_y_ins0_last & ((1u << 4) - 1)) << 12) |
         ((r->conv_opd1_x_ins0 & ((1u << 4) - 1)) << 16) |
         ((r->conv_opd1_y_ins0 & ((1u << 4) - 1)) << 20) |
         ((r->dummy0 & ((1u << 8) - 1)) << 24);
  p[3] = (r->opt_res0_sign & 1) |
         ((r->opt_opd0_sign & 1) << 1) |
         ((r->opt_opd1_sign & 1) << 2) |
         ((r->opt_opd2_sign & 1) << 3) |
         ((r->opt_res0_seg & ((1u << 2) - 1)) << 4) |
         ((r->opt_opd0_seg & ((1u << 2) - 1)) << 6) |
         ((r->opt_opd1_seg & ((1u << 2) - 1)) << 8) |
         ((r->opt_opd2_seg & 1) << 10) |
         ((r->ps32_md & ((1u << 2) - 1)) << 11) |
         ((r->double_conv & 1) << 13) |
         ((r->opt_left_tran & 1) << 14) |
         ((r->fp_round_typ & 1) << 15) |
         ((r->opt_relu_typ & ((1u << 2) - 1)) << 16) |
         ((r->opt_relu_value & ((1u << 8) - 1)) << 18) |
         ((r->cmd_pre_exe_typ & 1) << 26) |
         ((r->opt_res_add & 1) << 27) |
         ((r->rsvd0 & ((1u << 4) - 1)) << 28);
  p[2] = (r->quan_m & (((uint64_t)1 << 32) - 1));
  p[1] = (r->cmd_id_tpu & ((1u << 16) - 1)) |
         ((r->cmd_id_gdma & ((1u << 16) - 1)) << 16);
  p[0] = (r->cmd_en & 1) |
         ((r->cmd_end & 1) << 1) |
         ((r->cmd_id_en & 1) << 2) |
         ((r->cmd_keep & 1) << 3) |
         ((r->cmd_intr_en & 1) << 4) |
         ((r->tsk_typ & ((1u << 4) - 1)) << 5) |
         ((r->tsk_eu_typ & ((1u << 5) - 1)) << 9) |
         ((r->tsk_opd_num & ((1u << 2) - 1)) << 14) |
         ((r->opt_res_shift & ((1u << 6) - 1)) << 16) |
         ((r->opt_left_shift & ((1u << 5) - 1)) << 22) |
         ((r->opt_shift_typ & 1) << 27) |
         ((r->opt_rshift_typ & 1) << 28) |
         ((r->dummy1 & 1) << 29) |
         ((r->opd_typ & 1) << 30) |
         ((r->opt_chl_quan & 1) << 31);
}

static inline void reset_tiu_reg(tiu_reg_t *r)
{
  r->cmd_en = 0x0;
  r->cmd_end = 0x0;
  r->cmd_id_en = 0x0;
  r->cmd_keep = 0x0;
  r->cmd_intr_en = 0x0;
  r->tsk_typ = 0x0;
  r->tsk_eu_typ = 0x0;
  r->tsk_opd_num = 0x3;
  r->opt_res_shift = 0xa;
  r->opt_left_shift = 0x2;
  r->opt_shift_typ = 0x1;
  r->opt_rshift_typ = 0x1;
  r->dummy1 = 0x0;
  r->opd_typ = 0x0;
  r->opt_chl_quan = 0x0;
  r->cmd_id_tpu = 0x0;
  r->cmd_id_gdma = 0x0;
  r->quan_m = 0x0;
  r->opt_res0_sign = 0x0;
  r->opt_opd0_sign = 0x0;
  r->opt_opd1_sign = 0x1;
  r->opt_opd2_sign = 0x1;
  r->opt_res0_seg = 0x1;
  r->opt_opd0_seg = 0x1;
  r->opt_opd1_seg = 0x1;
  r->opt_opd2_seg = 0x0;
  r->ps32_md = 0x0;
  r->double_conv = 0x0;
  r->opt_left_tran = 0x0;
  r->fp_round_typ = 0x0;
  r->opt_relu_typ = 0x0;
  r->opt_relu_value = 0x0;
  r->cmd_pre_exe_typ = 0x0;
  r->opt_res_add = 0x0;
  r->rsvd0 = 0x0;
  r->conv_opd0_x_ins0 = 0x0;
  r->conv_opd0_y_ins0 = 0x0;
  r->conv_opd0_x_ins0_last = 0x0;
  r->conv_opd0_y_ins0_last = 0x0;
  r->conv_opd1_x_ins0 = 0x0;
  r->conv_opd1_y_ins0 = 0x0;
  r->dummy0 = 0x0;
  r->opd0_ins_val = 0x0;
  r->conv_opd0_up_pad = 0x0;
  r->conv_opd0_dn_pad = 0x0;
  r->conv_opd0_lf_pad = 0x0;
  r->conv_opd0_rt_pad = 0x0;
  r->res0_n = 0x1;
  r->res0_c = 0x1;
  r->res0_h = 0x1;
  r->res0_w = 0x10;
  r->conv_op_x_str = 0x1;
  r->conv_op_y_str = 0x1;
  r->cmd_pre_exe = 0x0;
  r->rsvd1 = 0x1;
  r->res0_addr = 0x0;
  r->opd0_addr = 0x0;
  r->opd1_addr = 0x0;
  r->opd2_addr = 0x0;
  r->opt_opd0_const = 0x0;
  r->opt_opd1_const = 0x0;
  r->opt_opd2_const = 0x0;
  r->short_nchwstr_same = 0x0;
  r->short_res0_str = 0x0;
  r->short_opd0_str = 0x0;
  r->short_opd1_str = 0x0;
  r->short_opd2_str = 0x0;
  r->dummy2 = 0x0;
  r->opd0_n = 0x1;
  r->opd0_c = 0x1;
  r->dummy3 = 0x0;
  r->rsvd2 = 0x2;
  r->opd0_h = 0x1;
  r->opd0_w = 0x10;
  r->opd1_n = 0x1;
  r->opd1_c = 0x1;
  r->opd1_h = 0x1;
  r->opd1_w = 0x10;
  r->opd2_n = 0x1;
  r->opd2_c = 0x1;
  r->opd2_h = 0x1;
  r->opd2_w = 0x10;
  r->dummy4 = 0x0;
  r->rsvd3 = 0x3;
  r->layer_info = 0x0;
  r->res0_n_str = 0x10;
  r->res0_c_str = 0x10;
  r->res0_h_str = 0x0;
  r->res0_w_str = 0x1;
  r->res0_b_str = 0x10;
  r->opd0_n_str = 0x10;
  r->dummy5 = 0x0;
  r->rsvd4 = 0x4;
  r->opd0_c_str = 0x10;
  r->opd0_h_str = 0x0;
  r->opd0_w_str = 0x1;
  r->opd0_b_str = 0x10;
  r->opd1_n_str = 0x10;
  r->opd1_c_str = 0x10;
  r->opd1_h_str = 0x0;
  r->dummy6 = 0x0;
  r->rsvd5 = 0x5;
  r->opd1_w_str = 0x1;
  r->opd1_b_str = 0x10;
  r->opd2_n_str = 0x10;
  r->opd2_c_str = 0x10;
  r->opd2_h_str = 0x0;
  r->opd2_w_str = 0x1;
  r->opd2_b_str = 0x10;
  r->dummy7 = 0x0;
  r->rsvd6 = 0x6;
}

static inline void trace_tiu_reg(tiu_reg_t *r, const char *tag)
{
#define trace_one_reg(name) \
  printf("  %s: 0x%llx\n", #name, (ullong)r->name)

  printf("--- %s ---\n", tag);
  trace_one_reg(cmd_en);
  trace_one_reg(cmd_end);
  trace_one_reg(cmd_id_en);
  trace_one_reg(cmd_keep);
  trace_one_reg(cmd_intr_en);
  trace_one_reg(tsk_typ);
  trace_one_reg(tsk_eu_typ);
  trace_one_reg(tsk_opd_num);
  trace_one_reg(opt_res_shift);
  trace_one_reg(opt_left_shift);
  trace_one_reg(opt_shift_typ);
  trace_one_reg(opt_rshift_typ);
  trace_one_reg(dummy1);
  trace_one_reg(opd_typ);
  trace_one_reg(opt_chl_quan);
  trace_one_reg(cmd_id_tpu);
  trace_one_reg(cmd_id_gdma);
  trace_one_reg(quan_m);
  trace_one_reg(opt_res0_sign);
  trace_one_reg(opt_opd0_sign);
  trace_one_reg(opt_opd1_sign);
  trace_one_reg(opt_opd2_sign);
  trace_one_reg(opt_res0_seg);
  trace_one_reg(opt_opd0_seg);
  trace_one_reg(opt_opd1_seg);
  trace_one_reg(opt_opd2_seg);
  trace_one_reg(ps32_md);
  trace_one_reg(double_conv);
  trace_one_reg(opt_left_tran);
  trace_one_reg(fp_round_typ);
  trace_one_reg(opt_relu_typ);
  trace_one_reg(opt_relu_value);
  trace_one_reg(cmd_pre_exe_typ);
  trace_one_reg(opt_res_add);
  trace_one_reg(rsvd0);
  trace_one_reg(conv_opd0_x_ins0);
  trace_one_reg(conv_opd0_y_ins0);
  trace_one_reg(conv_opd0_x_ins0_last);
  trace_one_reg(conv_opd0_y_ins0_last);
  trace_one_reg(conv_opd1_x_ins0);
  trace_one_reg(conv_opd1_y_ins0);
  trace_one_reg(dummy0);
  trace_one_reg(opd0_ins_val);
  trace_one_reg(conv_opd0_up_pad);
  trace_one_reg(conv_opd0_dn_pad);
  trace_one_reg(conv_opd0_lf_pad);
  trace_one_reg(conv_opd0_rt_pad);
  trace_one_reg(res0_n);
  trace_one_reg(res0_c);
  trace_one_reg(res0_h);
  trace_one_reg(res0_w);
  trace_one_reg(conv_op_x_str);
  trace_one_reg(conv_op_y_str);
  trace_one_reg(cmd_pre_exe);
  trace_one_reg(rsvd1);
  trace_one_reg(res0_addr);
  trace_one_reg(opd0_addr);
  trace_one_reg(opd1_addr);
  trace_one_reg(opd2_addr);
  trace_one_reg(opt_opd0_const);
  trace_one_reg(opt_opd1_const);
  trace_one_reg(opt_opd2_const);
  trace_one_reg(short_nchwstr_same);
  trace_one_reg(short_res0_str);
  trace_one_reg(short_opd0_str);
  trace_one_reg(short_opd1_str);
  trace_one_reg(short_opd2_str);
  trace_one_reg(dummy2);
  trace_one_reg(opd0_n);
  trace_one_reg(opd0_c);
  trace_one_reg(dummy3);
  trace_one_reg(rsvd2);
  trace_one_reg(opd0_h);
  trace_one_reg(opd0_w);
  trace_one_reg(opd1_n);
  trace_one_reg(opd1_c);
  trace_one_reg(opd1_h);
  trace_one_reg(opd1_w);
  trace_one_reg(opd2_n);
  trace_one_reg(opd2_c);
  trace_one_reg(opd2_h);
  trace_one_reg(opd2_w);
  trace_one_reg(dummy4);
  trace_one_reg(rsvd3);
  trace_one_reg(layer_info);
  trace_one_reg(res0_n_str);
  trace_one_reg(res0_c_str);
  trace_one_reg(res0_h_str);
  trace_one_reg(res0_w_str);
  trace_one_reg(res0_b_str);
  trace_one_reg(opd0_n_str);
  trace_one_reg(dummy5);
  trace_one_reg(rsvd4);
  trace_one_reg(opd0_c_str);
  trace_one_reg(opd0_h_str);
  trace_one_reg(opd0_w_str);
  trace_one_reg(opd0_b_str);
  trace_one_reg(opd1_n_str);
  trace_one_reg(opd1_c_str);
  trace_one_reg(opd1_h_str);
  trace_one_reg(dummy6);
  trace_one_reg(rsvd5);
  trace_one_reg(opd1_w_str);
  trace_one_reg(opd1_b_str);
  trace_one_reg(opd2_n_str);
  trace_one_reg(opd2_c_str);
  trace_one_reg(opd2_h_str);
  trace_one_reg(opd2_w_str);
  trace_one_reg(opd2_b_str);
  trace_one_reg(dummy7);
  trace_one_reg(rsvd6);
}
#endif /* CV181X_TIU_REG_H */
