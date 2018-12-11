//------------------------------------------------------------------------------
/// @file qrc_ecc.h
/// @author Loibl Philip
/// @date 1 Oct 2018
/// @brief This library procudes the error correction codewords used for
///        QR-Code ISO/IEC 18004:2006.
///
/// @details It is a header-only library that is built on the
///          c standard library only.
///          The function generateErrorCorrectionCodewords must be called to
///          generate the error correction codewords for a given message.
///          The function generateFormatString generates the format string bits
///          for a given matrix configuration.
///
/// @see https://palme.iicm.tugraz.at/wiki/ESP/Ass3_WS18
/// @see https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction
/// @copyright MIT in 2018 by Loibl Philip
//------------------------------------------------------------------------------
//

#ifndef QRC_ECC_H
#define QRC_ECC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#define GALOIS_FIELD_SIZE (1 << 8)


//------------------------------------------------------------------------------
///
/// Global variables which hold the Galois finite fields data
///
/// @note The use of any global variables is strictly forbidden in this course.
//
static uint8_t log_field[GALOIS_FIELD_SIZE];
static uint8_t exp_field[GALOIS_FIELD_SIZE];

//------------------------------------------------------------------------------
/// Return constants used for all functions in this library.
//
enum
{
  ERROR_CORRECTION_RETURN_SUCCESSFUL = 0,
  ERROR_CORRECTION_ERROR_OUT_OF_MEMORY = -1,
  ERROR_CORRECTION_ERROR_INVALID_PARAMETER = -2
};

//------------------------------------------------------------------------------
/// Range of supported QR versions
//
#define MIN_QR_VERSION 1
#define MAX_QR_VERSION 40

//------------------------------------------------------------------------------
/// Range of supported QR mask patterns
//
#define MIN_MASK_PATTERN_ID 0
#define MAX_MASK_PATTERN_ID 7

//------------------------------------------------------------------------------
/// Min version that requires special information encoding
//
#define MIN_LONG_INFO_VERSION 7

//------------------------------------------------------------------------------
/// Range of supported ecc length
//
#define MIN_ECC_LEN 7
#define MAX_ECC_LEN 254

//------------------------------------------------------------------------------
///
/// This function initializes the galois 256 finite fields which are declared
/// as global variables (log- and antilog field).
///
/// @param field_generator the reed solomon algorithm used in QR-codes use
///                        the value 285 for field generation
//
static void initializeGalois256Fields(const uint32_t field_generator)
{
  for(uint32_t alpha_value = 0; alpha_value < GALOIS_FIELD_SIZE; alpha_value++)
  {
    uint32_t integer_value = 1 << alpha_value;

    if(alpha_value > 7)
    {
      integer_value = exp_field[alpha_value - 1] * 2;
    }

    if(integer_value > UCHAR_MAX)
    {
      integer_value ^= field_generator;
    }

    exp_field[alpha_value] = integer_value;
    log_field[integer_value] = alpha_value;
  }
  log_field[0] = log_field[1] = 0;
}

//------------------------------------------------------------------------------
///
/// This function multiplies two alpha values by adding the exponents
///
/// @param alpha_exponent_1 the alpha exponent of term 1
/// @param alpha_exponent_2 the alpha exponent of term 2
///
/// @return the resulting exponent of the multiplication
//
static uint8_t multiplyAlphaValuesByExponents(const uint8_t alpha_exponent_1,
                                              const uint8_t alpha_exponent_2)
{
  // if sum is greater than 255, add 1
  if(alpha_exponent_1 > UCHAR_MAX - alpha_exponent_2)
  {
    return alpha_exponent_1 + alpha_exponent_2 + 1;
  }
  else
  {
    return alpha_exponent_1 + alpha_exponent_2;
  }
}

//------------------------------------------------------------------------------
///
/// This function generates the generator polynomial for a given size
///
/// @param generator_polynomial the array which stores the generator polynomial;
///                             must point to a valid memory with the size of
///                             generator_polynomial_size
/// @param generator_polynomial_size the size of the generator polynomial
///
/// @return ERROR_CORRECTION_RETURN_SUCCESSFUL if executes successfully,
///         ERROR_CORRECTION_ERROR_OUT_OF_MEMORY if memory allocation fails and
///         ERROR_CORRECTION_ERROR_INVALID_PARAMETER if this function is called
///         with invalid parameters
//
static int createGeneratorPolynomial(uint8_t *generator_polynomial,
                                     const size_t generator_polynomial_size)
{
  if(generator_polynomial == NULL ||
     generator_polynomial_size < 3 ||
     generator_polynomial_size > UCHAR_MAX)
  {
    return ERROR_CORRECTION_ERROR_INVALID_PARAMETER;
  }

  // initialize the starting generator polynomial
  generator_polynomial[0] = generator_polynomial[1] = 0;

  for(uint8_t iterator_polynomial = 1; iterator_polynomial <
      generator_polynomial_size - 1; iterator_polynomial++)
  {
    // insert term x^0
    generator_polynomial[iterator_polynomial + 1] =
        multiplyAlphaValuesByExponents(
            generator_polynomial[iterator_polynomial],
            iterator_polynomial);

    for(uint8_t gen_it = iterator_polynomial; gen_it > 0; gen_it--)
    {
      // combine the two x^gen_it terms
      uint8_t first_term = generator_polynomial[gen_it] + 0;
      uint8_t second_term = multiplyAlphaValuesByExponents(
          generator_polynomial[gen_it - 1], iterator_polynomial);

      // add the terms by xoring them in integer notation
      uint8_t xor_integers = exp_field[first_term] ^ exp_field[second_term];

      // convert the result back to alpha notation
      generator_polynomial[gen_it] = log_field[xor_integers];
    }
  }

  return ERROR_CORRECTION_RETURN_SUCCESSFUL;
}


//------------------------------------------------------------------------------
///
/// This function multiplies each term in a polynomial with a given term
///
/// @param polynomial the polynomial the term is multiplied with
/// @param polynomial_size the size of the polynomial (# of terms)
/// @param term the term each term in the polynomial is multiplied with
///
/// @return ERROR_CORRECTION_RETURN_SUCCESSFUL if executes successfully,
///         ERROR_CORRECTION_ERROR_OUT_OF_MEMORY if memory allocation fails and
///         ERROR_CORRECTION_ERROR_INVALID_PARAMETER if this function is called
///         with invalid parameters
//
static int multiplyPolynomialByTerm(uint8_t *polynomial,
                                    const size_t polynomial_size, uint8_t term)
{
  if(polynomial == NULL)
  {
    return ERROR_CORRECTION_ERROR_INVALID_PARAMETER;
  }

  // convert term to alpha notation
  term = log_field[term];

  // multiply the generator polynomial by the lead term of
  // the message polynomial
  for(size_t poly_it = 0; poly_it < polynomial_size; poly_it++)
  {
    const uint8_t alpha_result = multiplyAlphaValuesByExponents(
        polynomial[poly_it], term);

    // convert back to integer notation
    polynomial[poly_it] = exp_field[alpha_result];
  }

  return ERROR_CORRECTION_RETURN_SUCCESSFUL;
}


//------------------------------------------------------------------------------
///
/// This function performs an xor operation between two polynomials
///
/// @param rem_poly the first polynomial (worker polynomial); the result of
///                 the xor operation is stores in this array
/// @param rem_poly_size the size of the first polynomial (# of terms)
/// @param gen_poly the second polynomial for the xor operation
/// @param gen_poly_size the size of the second polynomial (# of terms)
///
/// @return ERROR_CORRECTION_RETURN_SUCCESSFUL if executes successfully,
///         ERROR_CORRECTION_ERROR_OUT_OF_MEMORY if memory allocation fails and
///         ERROR_CORRECTION_ERROR_INVALID_PARAMETER if this function is called
///         with invalid parameters
//
static int xorPolynomials(uint8_t *rem_poly, const size_t rem_poly_size,
                          uint8_t *gen_poly, const size_t gen_poly_size)
{
  if(rem_poly == NULL || gen_poly == NULL)
  {
    return ERROR_CORRECTION_ERROR_INVALID_PARAMETER;
  }

  // start at 1 to discard the lead 0 term
  for(size_t rem_it = 1, gen_it = 1;
      gen_it < gen_poly_size || rem_it < rem_poly_size; gen_it++, rem_it++)
  {
    const uint8_t remainder = rem_it < rem_poly_size ? rem_poly[rem_it] : 0;
    const uint8_t generator = gen_it < gen_poly_size ? gen_poly[gen_it] : 0;

    rem_poly[rem_it - 1] = remainder ^ generator;
  }
  rem_poly[rem_poly_size - 1] = 0;

  return ERROR_CORRECTION_RETURN_SUCCESSFUL;
}

//------------------------------------------------------------------------------
///
/// @brief The first core function of this library.
/// @details This function must be called to create the error correction
///          codewords for a given message. The message must include
///          mode indicator, character count indicator, text data with
///          terminator and 0xEC11 padding.
///
/// @param error_correction_code_words the result parameter; it must be a
///                                    preallocated array with the size of
///                                    number_of_error_correction_code_words
/// @param number_of_error_correction_code_words the size of the
///                                              error_correction_code_words
///                                              parameter
/// @param message the message the error correction words should be
///                calculated for
/// @param message_length the size of the message
///
/// @return ERROR_CORRECTION_RETURN_SUCCESSFUL if executes successfully,
///         ERROR_CORRECTION_ERROR_OUT_OF_MEMORY if memory allocation fails and
///         ERROR_CORRECTION_ERROR_INVALID_PARAMETER if this function is called
///         with invalid parameters
//
static int generateErrorCorrectionCodewords(
    uint8_t *error_correction_code_words,
    const size_t number_of_error_correction_code_words,
    const uint8_t *message, const size_t message_length)
{
  int ret = ERROR_CORRECTION_RETURN_SUCCESSFUL;

  if(error_correction_code_words == NULL ||
     number_of_error_correction_code_words < MIN_ECC_LEN ||
     number_of_error_correction_code_words > MAX_ECC_LEN)
  {
    return ERROR_CORRECTION_ERROR_INVALID_PARAMETER;
  }

  initializeGalois256Fields(0x11D);

  // ---------------------------------------------------------------------------
  const size_t generator_polynomial_size =
      number_of_error_correction_code_words + 1;

  uint8_t *generator_polynomial = (uint8_t*)malloc(generator_polynomial_size);
  if(generator_polynomial == NULL)
  {
    return ERROR_CORRECTION_ERROR_OUT_OF_MEMORY;
  }

  ret = createGeneratorPolynomial(generator_polynomial,
      generator_polynomial_size);

  if(ret != ERROR_CORRECTION_RETURN_SUCCESSFUL)
  {
    free(generator_polynomial);
    return ret;
  }

  // ---------------------------------------------------------------------------
  size_t remainder_polynomial_size =
      generator_polynomial_size > message_length ? generator_polynomial_size :
                                                   message_length;

  uint8_t *remainder_polynomial =
      (uint8_t*)calloc(sizeof(uint8_t), remainder_polynomial_size);
  if(remainder_polynomial == NULL)
  {
    free(generator_polynomial);
    return ERROR_CORRECTION_ERROR_OUT_OF_MEMORY;
  }
  memcpy(remainder_polynomial, message, message_length);

  // ---------------------------------------------------------------------------
  uint8_t *generator_polynomial_times_lead_term =
      (uint8_t*)malloc(generator_polynomial_size);
  if(generator_polynomial_times_lead_term == NULL)
  {
    free(remainder_polynomial);
    free(generator_polynomial);
    return ERROR_CORRECTION_ERROR_OUT_OF_MEMORY;
  }

  // ---------------------------------------------------------------------------
  // divide the message polynomial by the generator polynomial
  for(size_t division_step = 0; division_step < message_length; division_step++)
  {
    // multiply the generator polynomial by the lead term
    // of the message polynomial
    memcpy(generator_polynomial_times_lead_term,
           generator_polynomial, generator_polynomial_size);

    ret = multiplyPolynomialByTerm(generator_polynomial_times_lead_term,
        generator_polynomial_size, remainder_polynomial[0]);
    if(ret != ERROR_CORRECTION_RETURN_SUCCESSFUL)
    {
      free(generator_polynomial_times_lead_term);
      free(remainder_polynomial);
      free(generator_polynomial);
      return ret;
    }

    // XOR the result with the message polynomial
    ret = xorPolynomials(remainder_polynomial, remainder_polynomial_size,
                   generator_polynomial_times_lead_term,
                   generator_polynomial_size);
    if(ret != ERROR_CORRECTION_RETURN_SUCCESSFUL)
    {
      free(generator_polynomial_times_lead_term);
      free(remainder_polynomial);
      free(generator_polynomial);
      return ret;
    }

    // discard leading 0 terms
    for(size_t discard_it = remainder_polynomial_size;
        discard_it-- > 0 && remainder_polynomial[0] == 0;)
    {
      for(size_t rem_it = 0; rem_it < remainder_polynomial_size - 1; rem_it++)
      {
        remainder_polynomial[rem_it] = remainder_polynomial[rem_it + 1];
      }
      remainder_polynomial[remainder_polynomial_size - 1] = 0;
      division_step++;
    }
  }

  // the remainder polynomial is used for the error correction codewords
  memcpy(error_correction_code_words, remainder_polynomial,
         number_of_error_correction_code_words);

  free(generator_polynomial_times_lead_term);
  free(remainder_polynomial);
  free(generator_polynomial);
  return ret;
}


//------------------------------------------------------------------------------
///
/// This function performs a polynomial division with two binary polynomials
///
/// @param format_string the pointer to the current format string
/// @param format_string_size the pointer to the size of the format string
///                           in bits
/// @param generator_polynomial the generator polynomial
/// @param generator_polynomial_size the size of the generator polynomial
///                                  (# of bits)
///
/// @return ERROR_CORRECTION_RETURN_SUCCESSFUL if executes successfully,
///         ERROR_CORRECTION_ERROR_OUT_OF_MEMORY if memory allocation fails and
///         ERROR_CORRECTION_ERROR_INVALID_PARAMETER if this function is called
///         with invalid parameters
//
static int polynomialBinaryDivision(uint32_t *format_string,
                                    size_t *format_string_size,
                                    uint32_t generator_polynomial,
                                    const size_t generator_polynomial_size)
{
  if(format_string == NULL || format_string_size == NULL)
  {
    return ERROR_CORRECTION_ERROR_INVALID_PARAMETER;
  }

  // pad the generator polynomial
  if(generator_polynomial_size < *format_string_size)
  {
    generator_polynomial <<= (*format_string_size - generator_polynomial_size);
  }

  (*format_string) ^= generator_polynomial;

  // remove leading zeros
  while((*format_string_size) > 0 &&
        !((*format_string) >> (*format_string_size - 1)))
  {
    (*format_string_size)--;
  }

  return ERROR_CORRECTION_RETURN_SUCCESSFUL;
}


//------------------------------------------------------------------------------
///
/// @brief The second core function of this library.
/// @details This function must be called to create the format string bits.
///
///
/// @param format_string the resulting formatstring is stored on the location
///                      the parameter points to
/// @param version the QR-Code version the formatstring is generated for;
///                the parameter is valid for values between 1 and 40
/// @param error_correction_level the QR-Code error correction level
///                               value | ECL
///                                 0   |  L
///                                 1   |  M
///                                 2   |  Q
///                                 3   |  H
/// @param mask_pattern_id the mask pattern that is used for the QR-Code matrix
///                        the parameter is valid for values between 0 and 7
///
/// @return ERROR_CORRECTION_RETURN_SUCCESSFUL if executes successfully,
///         ERROR_CORRECTION_ERROR_OUT_OF_MEMORY if memory allocation fails and
///         ERROR_CORRECTION_ERROR_INVALID_PARAMETER if this function is called
///         with invalid parameters
//
static int generateFormatString(uint32_t *format_string, const int version,
                                int error_correction_level,
                                const int mask_pattern_id)
{
  // parameter check
  if(format_string == NULL ||
     version < MIN_QR_VERSION || version > MAX_QR_VERSION ||
     mask_pattern_id < MIN_MASK_PATTERN_ID ||
     mask_pattern_id > MAX_MASK_PATTERN_ID)
  {
    return ERROR_CORRECTION_ERROR_INVALID_PARAMETER;
  }

  if(error_correction_level < 0 || error_correction_level > 3)
  {
    return ERROR_CORRECTION_ERROR_INVALID_PARAMETER;
  }

  // change the error_correction_level to the appropriate value
  error_correction_level += (error_correction_level % 2) ? (-1) : (+1);

  uint32_t generator_polynomial = (version < MIN_LONG_INFO_VERSION) ?
      0b10100110111 : 0b1111100100101;

  size_t generator_polynomial_size = (version < MIN_LONG_INFO_VERSION) ?
      11 : 13;

  uint32_t error_correction_string = 0;
  size_t error_correction_string_size = (version < MIN_LONG_INFO_VERSION) ?
      15 : 18;

  if(version < MIN_LONG_INFO_VERSION)
  {
    error_correction_string |=
        error_correction_level << (error_correction_string_size - 2);
    error_correction_string |=
        mask_pattern_id << (error_correction_string_size - 5);
  }
  else
  {
    error_correction_string |= version << (error_correction_string_size - 6);
  }

  (*format_string) = error_correction_string;

  // perform polynomial division
  while(error_correction_string_size > ((version < MIN_LONG_INFO_VERSION) ?
      10 : 12))
  {
    int ret = polynomialBinaryDivision(&error_correction_string,
                                       &error_correction_string_size,
                                       generator_polynomial,
                                       generator_polynomial_size);

    if(ret != ERROR_CORRECTION_RETURN_SUCCESSFUL)
    {
      return ret;
    }
  }

  // add error correction to format string
  (*format_string) |= error_correction_string;

  // xor with mask string
  if(version < MIN_LONG_INFO_VERSION)
  {
    (*format_string) ^= 0b101010000010010;
  }

  return ERROR_CORRECTION_RETURN_SUCCESSFUL;
}

#endif //QRC_ECC_H
