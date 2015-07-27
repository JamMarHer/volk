/* -*- c++ -*- */
/* 
 * Copyright 2015 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*!
 * \page volk_8u_x3_encodepolar_8u
 *
 * \b Overview
 *
 * encode given data for POLAR code
 *
 * <b>Dispatcher Prototype</b>
 * \code
 * void volk_8u_x3_encodepolar_8u(unsigned char* frame, const unsigned char* frozen_bit_mask, const unsigned char* frozen_bits,
 *                                  const unsigned char* info_bits, unsigned int frame_size, unsigned int info_bit_size)
 * \endcode
 *
 * \b Inputs
 * \li frame: buffer for encoded frame
 * \li frozen_bit_mask: bytes with 0xFF for frozen bit positions or 0x00 otherwise.
 * \li frozen_bits: values of frozen bits, 1 bit per byte
 * \li info_bits: info bit values, 1 bit per byte
 * \li frame_size: power of 2 value for frame size.
 * \li info_bit_size: number of info bits in a frame
 *
 * \b Outputs
 * \li frame: polar encoded frame.
 *
 * \b Example
 * \code
 * int N = 10000;
 *
 * volk_8u_x3_encodepolar_8u(unsigned char* frame, const unsigned char* frozen_bit_mask, const unsigned char* frozen_bits,
 *                                  const unsigned char* info_bits, unsigned int frame_size, unsigned int info_bit_size);
 *
 * volk_free(x);
 * \endcode
 */

#ifndef VOLK_KERNELS_VOLK_VOLK_8U_X3_ENCODEPOLAR_8U_X2__U_H_
#define VOLK_KERNELS_VOLK_VOLK_8U_X3_ENCODEPOLAR_8U_X2__U_H_
#include <stdio.h>

static inline unsigned int
log2_of_power_of_2(unsigned int val){
  // algorithm from: http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
  static const unsigned int b[] = {0xAAAAAAAA, 0xCCCCCCCC, 0xF0F0F0F0,
                                   0xFF00FF00, 0xFFFF0000};

  unsigned int res = (val & b[0]) != 0;
  res |= ((val & b[4]) != 0) << 4;
  res |= ((val & b[3]) != 0) << 3;
  res |= ((val & b[2]) != 0) << 2;
  res |= ((val & b[1]) != 0) << 1;
  return res;
}

static inline void
interleave_frozen_and_info_bits(unsigned char* target, const unsigned char* frozen_bit_mask,
                                const unsigned char* frozen_bits, const unsigned char* info_bits,
                                const unsigned int frame_size)
{
  unsigned int bit;
  for(bit = 0; bit < frame_size; ++bit){
    *target++ = *frozen_bit_mask++ ? *frozen_bits++ : *info_bits++;
  }
}

static inline void
encodepolar_single_stage(unsigned char* frame_ptr, const unsigned char* temp_ptr,
                         const unsigned int num_branches, const unsigned int frame_half)
{
  unsigned int branch, bit;
  for(branch = 0; branch < num_branches; ++branch){
    for(bit = 0; bit < frame_half; ++bit){
      *frame_ptr = *temp_ptr ^ *(temp_ptr + 1);
      *(frame_ptr + frame_half) = *(temp_ptr + 1);
      ++frame_ptr;
      temp_ptr += 2;
    }
    frame_ptr += frame_half;
  }
}

static inline void
print_vector_elements(const unsigned char* buf, int elems){
  buf += 64;
  int i, l;
//  printf("buffer: ");
  for(l = 0; l < 8; l++){
    printf("   ");
    for(i = 0; i < 8; i++){
      unsigned char b = buf[i];
      printf("%02x ", (int) b);
    }
    buf += 8;

  }
  printf("\n");

}

#ifdef LV_HAVE_GENERIC

static inline void
volk_8u_x3_encodepolar_8u_x2_generic(unsigned char* frame, unsigned char* temp, const unsigned char* frozen_bit_mask,
    const unsigned char* frozen_bits, const unsigned char* info_bits,
    unsigned int frame_size){
  unsigned int stage = log2_of_power_of_2(frame_size);
  unsigned int frame_half = frame_size >> 1;
  unsigned int num_branches = 1;

  // interleave
  interleave_frozen_and_info_bits(temp, frozen_bit_mask, frozen_bits, info_bits, frame_size);

  while(stage){
    // encode stage
    encodepolar_single_stage(frame, temp, num_branches, frame_half);
    memcpy(temp, frame, sizeof(unsigned char) * frame_size);

    // update all the parameters.
    num_branches = num_branches << 1;
    frame_half = frame_half >> 1;
    --stage;
  }
}
#endif /* LV_HAVE_GENERIC */


#ifdef LV_HAVE_SSSE3
#include <tmmintrin.h>

static inline void
volk_8u_x3_encodepolar_8u_x2_u_ssse3(unsigned char* frame, unsigned char* temp,
                                   const unsigned char* frozen_bit_mask,
                                   const unsigned char* frozen_bits, const unsigned char* info_bits,
                                   unsigned int frame_size)
{
  const unsigned int po2 = log2_of_power_of_2(frame_size);

  unsigned int stage = po2;
  unsigned char* frame_ptr = frame;
  unsigned char* temp_ptr = temp;

  unsigned int frame_half = frame_size >> 1;
  unsigned int num_branches = 1;
  unsigned int branch;
  unsigned int bit;

  // interleave
  interleave_frozen_and_info_bits(temp_ptr, frozen_bit_mask, frozen_bits, info_bits, frame_size);

  // prepare constants
  const __m128i mask_stage1 = _mm_set_epi8(0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF);

  // get some SIMD registers to play with.
  __m128i r_frame0, r_temp0, shifted;

  {
    __m128i r_frame1, r_temp1;
    const __m128i shuffle_separate = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);

    while(stage > 4){
      frame_ptr = frame;
      temp_ptr = temp;

      // for stage = 5 a branch has 32 elements. So upper stages are even bigger.
      for(branch = 0; branch < num_branches; ++branch){
        for(bit = 0; bit < frame_half; bit += 16){
          r_temp0 = _mm_loadu_si128((__m128i *) temp_ptr);
          temp_ptr += 16;
          r_temp1 = _mm_loadu_si128((__m128i *) temp_ptr);
          temp_ptr += 16;

          shifted = _mm_bsrli_si128(r_temp0, 1);
          shifted = _mm_and_si128(shifted, mask_stage1);
          r_temp0 = _mm_xor_si128(shifted, r_temp0);
          r_temp0 = _mm_shuffle_epi8(r_temp0, shuffle_separate);

          shifted = _mm_bsrli_si128(r_temp1, 1);
          shifted = _mm_and_si128(shifted, mask_stage1);
          r_temp1 = _mm_xor_si128(shifted, r_temp1);
          r_temp1 = _mm_shuffle_epi8(r_temp1, shuffle_separate);

          r_frame0 = _mm_unpacklo_epi64(r_temp0, r_temp1);
          _mm_storeu_si128((__m128i*) frame_ptr, r_frame0);

          r_frame1 = _mm_unpackhi_epi64(r_temp0, r_temp1);
          _mm_storeu_si128((__m128i*) (frame_ptr + frame_half), r_frame1);
          frame_ptr += 16;
        }

        frame_ptr += frame_half;
      }
      memcpy(temp, frame, sizeof(unsigned char) * frame_size);

      num_branches = num_branches << 1;
      frame_half = frame_half >> 1;
      stage--;
    }
  }

  // This last part requires at least 16-bit frames.
  // Smaller frames are useless for SIMD optimization anyways. Just choose GENERIC!

  // reset pointers to correct positions.
  frame_ptr = frame;
  temp_ptr = temp;

  // load first chunk.
  // Tests show a 1-2% gain compared to loading a new chunk and using it right after.
  r_temp0 = _mm_loadu_si128((__m128i*) temp_ptr);
  temp_ptr += 16;

  const __m128i shuffle_stage4 = _mm_setr_epi8(0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15);
  const __m128i mask_stage4 = _mm_set_epi8(0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
  const __m128i mask_stage3 = _mm_set_epi8(0x0, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0xFF);
  const __m128i mask_stage2 = _mm_set_epi8(0x0, 0x0, 0xFF, 0xFF, 0x0, 0x0, 0xFF, 0xFF, 0x0, 0x0, 0xFF, 0xFF, 0x0, 0x0, 0xFF, 0xFF);

  for(branch = 0; branch < num_branches; ++branch){
    // shuffle once for bit-reversal.
    r_temp0 = _mm_shuffle_epi8(r_temp0, shuffle_stage4);

    shifted = _mm_bsrli_si128(r_temp0, 8);
    shifted = _mm_and_si128(shifted, mask_stage4);
    r_frame0 = _mm_xor_si128(shifted, r_temp0);

    // start loading next chunk.
    r_temp0 = _mm_loadu_si128((__m128i*) temp_ptr);
    temp_ptr += 16;

    shifted = _mm_bsrli_si128(r_frame0, 4);
    shifted = _mm_and_si128(shifted, mask_stage3);
    r_frame0 = _mm_xor_si128(shifted, r_frame0);

    shifted = _mm_bsrli_si128(r_frame0, 2);
    shifted = _mm_and_si128(shifted, mask_stage2);
    r_frame0 = _mm_xor_si128(shifted, r_frame0);

    shifted = _mm_bsrli_si128(r_frame0, 1);
    shifted = _mm_and_si128(shifted, mask_stage1);
    r_frame0 = _mm_xor_si128(shifted, r_frame0);

    // store result of chunk.
    _mm_storeu_si128((__m128i*)frame_ptr, r_frame0);
    frame_ptr += 16;
  }
}

#endif /* LV_HAVE_SSSE3 */

#endif /* VOLK_KERNELS_VOLK_VOLK_8U_X3_ENCODEPOLAR_8U_X2__U_H_ */

#ifndef VOLK_KERNELS_VOLK_VOLK_8U_X3_ENCODEPOLAR_8U_X2__A_H_
#define VOLK_KERNELS_VOLK_VOLK_8U_X3_ENCODEPOLAR_8U_X2__A_H_

#ifdef LV_HAVE_SSSE3
#include <tmmintrin.h>
static inline void
volk_8u_x3_encodepolar_8u_x2_a_ssse3(unsigned char* frame, unsigned char* temp,
                                   const unsigned char* frozen_bit_mask,
                                   const unsigned char* frozen_bits, const unsigned char* info_bits,
                                   unsigned int frame_size)
{
  const unsigned int po2 = log2_of_power_of_2(frame_size);

  unsigned int stage = po2;
  unsigned char* frame_ptr = frame;
  unsigned char* temp_ptr = temp;

  unsigned int frame_half = frame_size >> 1;
  unsigned int num_branches = 1;
  unsigned int branch;
  unsigned int bit;

  // interleave
  interleave_frozen_and_info_bits(temp_ptr, frozen_bit_mask, frozen_bits, info_bits, frame_size);

  // prepare constants
  const __m128i mask_stage1 = _mm_set_epi8(0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF);

  // get some SIMD registers to play with.
  __m128i r_frame0, r_temp0, shifted;

  {
    __m128i r_frame1, r_temp1;
    const __m128i shuffle_separate = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);

    while(stage > 4){
      frame_ptr = frame;
      temp_ptr = temp;

      // for stage = 5 a branch has 32 elements. So upper stages are even bigger.
      for(branch = 0; branch < num_branches; ++branch){
        for(bit = 0; bit < frame_half; bit += 16){
          r_temp0 = _mm_load_si128((__m128i *) temp_ptr);
          temp_ptr += 16;
          r_temp1 = _mm_load_si128((__m128i *) temp_ptr);
          temp_ptr += 16;

          shifted = _mm_bsrli_si128(r_temp0, 1);
          shifted = _mm_and_si128(shifted, mask_stage1);
          r_temp0 = _mm_xor_si128(shifted, r_temp0);
          r_temp0 = _mm_shuffle_epi8(r_temp0, shuffle_separate);

          shifted = _mm_bsrli_si128(r_temp1, 1);
          shifted = _mm_and_si128(shifted, mask_stage1);
          r_temp1 = _mm_xor_si128(shifted, r_temp1);
          r_temp1 = _mm_shuffle_epi8(r_temp1, shuffle_separate);

          r_frame0 = _mm_unpacklo_epi64(r_temp0, r_temp1);
          _mm_store_si128((__m128i*) frame_ptr, r_frame0);

          r_frame1 = _mm_unpackhi_epi64(r_temp0, r_temp1);
          _mm_store_si128((__m128i*) (frame_ptr + frame_half), r_frame1);
          frame_ptr += 16;
        }

        frame_ptr += frame_half;
      }
      memcpy(temp, frame, sizeof(unsigned char) * frame_size);

      num_branches = num_branches << 1;
      frame_half = frame_half >> 1;
      stage--;
    }
  }

  // This last part requires at least 16-bit frames.
  // Smaller frames are useless for SIMD optimization anyways. Just choose GENERIC!

  // reset pointers to correct positions.
  frame_ptr = frame;
  temp_ptr = temp;

  // load first chunk.
  // Tests show a 1-2% gain compared to loading a new chunk and using it right after.
  r_temp0 = _mm_load_si128((__m128i*) temp_ptr);
  temp_ptr += 16;

  const __m128i shuffle_stage4 = _mm_setr_epi8(0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15);
  const __m128i mask_stage4 = _mm_set_epi8(0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
  const __m128i mask_stage3 = _mm_set_epi8(0x0, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0xFF);
  const __m128i mask_stage2 = _mm_set_epi8(0x0, 0x0, 0xFF, 0xFF, 0x0, 0x0, 0xFF, 0xFF, 0x0, 0x0, 0xFF, 0xFF, 0x0, 0x0, 0xFF, 0xFF);

  for(branch = 0; branch < num_branches; ++branch){
    // shuffle once for bit-reversal.
    r_temp0 = _mm_shuffle_epi8(r_temp0, shuffle_stage4);

    shifted = _mm_bsrli_si128(r_temp0, 8);
    shifted = _mm_and_si128(shifted, mask_stage4);
    r_frame0 = _mm_xor_si128(shifted, r_temp0);

    // start loading next chunk.
    r_temp0 = _mm_load_si128((__m128i*) temp_ptr);
    temp_ptr += 16;

    shifted = _mm_bsrli_si128(r_frame0, 4);
    shifted = _mm_and_si128(shifted, mask_stage3);
    r_frame0 = _mm_xor_si128(shifted, r_frame0);

    shifted = _mm_bsrli_si128(r_frame0, 2);
    shifted = _mm_and_si128(shifted, mask_stage2);
    r_frame0 = _mm_xor_si128(shifted, r_frame0);

    shifted = _mm_bsrli_si128(r_frame0, 1);
    shifted = _mm_and_si128(shifted, mask_stage1);
    r_frame0 = _mm_xor_si128(shifted, r_frame0);

    // store result of chunk.
    _mm_store_si128((__m128i*)frame_ptr, r_frame0);
    frame_ptr += 16;
  }
}
#endif /* LV_HAVE_SSSE3 */

#endif /* VOLK_KERNELS_VOLK_VOLK_8U_X3_ENCODEPOLAR_8U_X2__A_H_ */
