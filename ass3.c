//------------------------------------------------------------------------------
// ass3.c
//
// QR - Code
//
// Group: Group C, study assistant Thomas Schwar
//
// Authors: Florian Klug 09830971
// Robin Edlinger 11804235
//------------------------------------------------------------------------------
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "qrc_ecc.h"

const uint8_t MAX_INPUT_STRING_SIZE = 106;
const uint8_t NUMBER_OF_QR_FLAVORS = 11;
const uint8_t NIBBLE_SIZE = 4;
const uint8_t MODULE_VALUE_BIT = 0;
const uint8_t MODULE_TAKEN_BIT = 1;
const uint8_t MODULE_DATA_FLAG_BIT = 2;
const uint8_t SYNC_PATTERN_POS = 6;
const uint8_t FORMAT_VERSION_POS = 8;
const uint8_t FORMAT_VERSION_LENGTH = 15;
const uint8_t MASK_PATTERN_ID = 6;


#define POS_PATTERN_SIZE 7
const uint8_t POS_PATTERN[POS_PATTERN_SIZE][POS_PATTERN_SIZE] =
{
  {3, 3, 3, 3, 3, 3, 3},
  {3, 2, 2, 2, 2, 2, 3},
  {3, 2, 3, 3, 3, 2, 3},
  {3, 2, 3, 3, 3, 2, 3},
  {3, 2, 3, 3, 3, 2, 3},
  {3, 2, 2, 2, 2, 2, 3},
  {3, 3, 3, 3, 3, 3, 3}
};

#define ALIGNMENT_PATTERN_SIZE 5
const uint8_t ALIGNMENT_PATTERN[ALIGNMENT_PATTERN_SIZE][ALIGNMENT_PATTERN_SIZE] 
=
{
  {3, 3, 3, 3, 3},
  {3, 2, 2, 2, 3},
  {3, 2, 3, 2, 3},
  {3, 2, 2, 2, 3},
  {3, 3, 3, 3, 3}
};

enum 
{
  ERR_NO_ERROR = 0,
  ERR_PARAMS = 1,
  ERR_ECC_OOM = 2,
  ERR_TEXT_SIZE = 3,
  ERR_ECC_PARAMS = 4,
  ERR_IO = 5
};

enum {
  UP = -1,
  DOWN = 1
};

enum {
  LEFT = -1,
  RIGHT = 1
};

const uint8_t QR_MODE = 0x04;



struct _MessageData_ 
{
  uint8_t mode_;
  uint8_t data_len_;
  unsigned char *data_;
};

struct _QRFlavor_ 
{
  uint8_t capacity_;
  uint8_t version_;
  unsigned char ec_level_;
  uint8_t ec_data_;
  uint8_t alignment_pattern_pos_;
}; 

const struct _QRFlavor_ QRFlavors[] = 
{
  {.capacity_ =   7, .version_ = 1, .ec_level_ = 'H', .ec_data_ = 17, 
    .alignment_pattern_pos_ = 0},
  {.capacity_ =  11, .version_ = 1, .ec_level_ = 'Q', .ec_data_ = 13, 
    .alignment_pattern_pos_ = 0},
  {.capacity_ =  14, .version_ = 1, .ec_level_ = 'M', .ec_data_ = 10, 
    .alignment_pattern_pos_ = 0},
  {.capacity_ =  17, .version_ = 1, .ec_level_ = 'L', .ec_data_ =  7, 
    .alignment_pattern_pos_ = 0},
  {.capacity_ =  20, .version_ = 2, .ec_level_ = 'Q', .ec_data_ = 22, 
    .alignment_pattern_pos_ = 18},
  {.capacity_ =  26, .version_ = 2, .ec_level_ = 'M', .ec_data_ = 16, 
    .alignment_pattern_pos_ = 18},
  {.capacity_ =  32, .version_ = 2, .ec_level_ = 'L', .ec_data_ = 10, 
    .alignment_pattern_pos_ = 18},
  {.capacity_ =  42, .version_ = 3, .ec_level_ = 'M', .ec_data_ = 26, 
    .alignment_pattern_pos_ = 22},
  {.capacity_ =  53, .version_ = 3, .ec_level_ = 'L', .ec_data_ = 15, 
    .alignment_pattern_pos_ = 22},
  {.capacity_ =  78, .version_ = 4, .ec_level_ = 'L', .ec_data_ = 20, 
    .alignment_pattern_pos_ = 26},
  {.capacity_ = 106, .version_ = 5, .ec_level_ = 'L', .ec_data_ = 26, 
    .alignment_pattern_pos_ = 30},
};

//------------------------------------------------------------------------------
///
/// @brief Set the module taken flag for \p module
/// 
/// @param module The module for which to set the flag
/// @param taken The taken flag
//
void setModuleTaken(uint8_t *module, uint8_t taken) 
{
  if (taken) *module |= (1 << MODULE_TAKEN_BIT);
  else *module &= (1 << MODULE_TAKEN_BIT);
}

//------------------------------------------------------------------------------
///
/// @brief Returns if the \p module is already in use
/// 
/// @param module The module for which to set the flag
///
/// @return uint8_t True if taken, else false
//
uint8_t isModuleTaken(uint8_t module) 
{
  return (module >> MODULE_TAKEN_BIT) & 1;
}

//------------------------------------------------------------------------------
///
/// @brief Set the \p module value
/// 
/// @param module The module for which to set the value
/// @param value 1 or zero (in fact, everything other than zero is treated as 1)
//
void setModuleValue(uint8_t *module, uint8_t value)
{
  if (value) *module |= (1 << MODULE_VALUE_BIT);
  else *module &= (0 << MODULE_VALUE_BIT);
  setModuleTaken(module, 1);
}

//------------------------------------------------------------------------------
///
/// @brief Get the \p module value
/// 
/// @param module The module for which to get the value
///
/// @return uint8_t 1 or 0
//
uint8_t getModuleValue(uint8_t module)
{
  return module & 1;
}

//------------------------------------------------------------------------------
///
/// @brief Set the \p module Data Flag
/// The data flag specifies wether a module is used for the payload data or not
/// 
/// @param module The module for which to set the flag
/// @param value The data flag
//
void setModuleDataFlag(uint8_t *module, uint8_t value)
{
  if (value) *module |= (1 << MODULE_DATA_FLAG_BIT);
  else *module &= (0 << MODULE_DATA_FLAG_BIT);
}

//------------------------------------------------------------------------------
///
/// @brief Returns wether a \p module is used for payload data
/// 
/// @param module The module to check
/// @return uint8_t True if module is used for payload, else false
//
uint8_t getModuleDataFlag(uint8_t module)
{
  return (module >> MODULE_DATA_FLAG_BIT) & 1;
}

//------------------------------------------------------------------------------
///
/// @brief Shortcut to set value and data flag at once
/// 
/// @param module The module to use
/// @param value The module value that should be set
//
void setModuleDataValue(uint8_t *module, uint8_t value)
{
  setModuleValue(module, value);
  setModuleDataFlag(module, true);
}

//------------------------------------------------------------------------------
///
/// @brief Writes the \p matrix to stdout
/// 
/// @param matrix The matrix to display
/// @param size The matrix size
//
void outputMatrix(uint8_t **matrix, uint8_t size)
{
  for (uint8_t row = 0; row < size; row++)
  {
    for (uint8_t column = 0; column < size; column++)
    {
      printf("%c", (getModuleValue(matrix[row][column]) == 1) ? '#' : ' ');
      if (column < size - 1) printf("%s", " ");
    }
    printf("%s", "\n");
  }
}

//------------------------------------------------------------------------------
///
/// @brief Outputs the IO error text and exits with corresponding error code
/// 
/// @param filename Name of the file which caused the error
//
static inline void exitWithIOError(char filename[])
{
  printf("[ERR] Could not write file %s.\n", filename);
  exit(ERR_IO);
}

//------------------------------------------------------------------------------
///
/// @brief Writes the matrix as SVG file
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
/// @param filename The filename under which the svg should be saved
//
void outputMatrixToSVGFile(uint8_t **matrix, uint8_t size, char filename[])
{
  const uint8_t module_size = 10;
  FILE *fp;
  int return_value;

  fp = fopen(filename, "w");
  if (!fp) exitWithIOError(filename);

  return_value = fputs("<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\" "
        "\"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\">",
        fp);
  if (return_value == EOF) exitWithIOError(filename);

  // border width 4x module size
  return_value = fprintf(fp, "<rect x=\"0\" y=\"0\" width=\"%i\" height=\"%i\" "
    "style=\"fill:%s\"/>\n",
    module_size * (8 + size), module_size * (8 + size), "white");
  if (return_value < 0) exitWithIOError(filename);

  for (uint8_t row = 0; row < size; row++)
  {
    for (uint8_t col = 0; col < size; col++)
    {
      return_value = fprintf(fp, "<rect x=\"%i\" y=\"%i\" width=\"%i\" height=\"%i\" "
        "style=\"fill:%s\"/>\n",
        (col + 4) * module_size, (row + 4) * module_size, module_size,
        module_size, (getModuleValue(matrix[row][col]) == 1) ? "black" : 
          "white");
      if (return_value < 0) exitWithIOError(filename);
    }
  }

  fputs("</svg>", fp);
  if (fclose(fp) == EOF) exitWithIOError(filename); 
}

//------------------------------------------------------------------------------
///
/// @brief Writes the matrix as CSV file
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
/// @param filename The filename under which the csv should be saved
//
void outputMatrixToCSVFile(uint8_t **matrix, uint8_t size, char filename[])
{
  FILE *fp;
  int return_value;

  fp = fopen(filename, "w");
  if (!fp) exitWithIOError(filename);

  for (uint8_t row = 0; row < size; row++)
  {
    for (uint8_t col = 0; col < size; col++)
    {
      return_value = fprintf(fp, "%i;", getModuleValue(matrix[row][col]));
      if (return_value < 0) exitWithIOError(filename);
    }
    return_value = fprintf(fp, "%s", "\n");
    if (return_value < 0) exitWithIOError(filename);
  }
  if (fclose(fp) == EOF) exitWithIOError(filename); 
}


//------------------------------------------------------------------------------
///
/// @brief Converts the _MessageData struct \p md to a byte stream considering
/// the QR-flavor
/// 
/// @param[out] md_stream A preallocated array to write the stream to
/// @param md The _MessageData struct to convert
/// @param flavor The QR-flavor to use
//
void generateMessageDataStream(uint8_t *md_stream, struct _MessageData_ *md, 
struct _QRFlavor_ flavor) 
{
  // qr mode and first byte of message
  md_stream[0] = 0 | (QR_MODE << NIBBLE_SIZE);
  md_stream[0] |= (md->data_len_ >> NIBBLE_SIZE);
  md_stream[1] = 0 | (md->data_len_ << NIBBLE_SIZE);

  // the remaining message bytes
  for (uint8_t counter = 1; counter <= md->data_len_; counter++) 
  {
    md_stream[counter] |= (md->data_[counter - 1] >> NIBBLE_SIZE);
    md_stream[counter + 1] = 0 | (md->data_[counter - 1] << NIBBLE_SIZE);
  }

  // terminate with zeros
  md_stream[md->data_len_+1] &= 0xF0;

  // padd with 0xEC11
  bool flag = true;
  for (uint8_t counter = md->data_len_ + 2; counter < flavor.capacity_ + 2; 
    counter++) 
  {
    md_stream[counter] = (flag) ? 0xEC : 0x11;
    flag = !flag;
  }

};

//------------------------------------------------------------------------------
///
/// @brief Creates the position patterns within the \p matrix
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
//
void mkPositionPattern(uint8_t **matrix, uint8_t size) 
{
  for (uint8_t row = 0; row < POS_PATTERN_SIZE; row++)
  {
    memcpy(&(matrix[row][0]), &(POS_PATTERN[row]), sizeof(uint8_t) * 
      POS_PATTERN_SIZE);
    memcpy(&(matrix[row][size - POS_PATTERN_SIZE]), &(POS_PATTERN[row]), 
      sizeof(uint8_t) * POS_PATTERN_SIZE);
    memcpy(&(matrix[size - POS_PATTERN_SIZE + row][0]), &(POS_PATTERN[row]), 
      sizeof(uint8_t) * POS_PATTERN_SIZE);
  }
}

//------------------------------------------------------------------------------
///
/// @brief Creates the separation patterns within the \p matrix
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
//
void mkSeparationPattern(uint8_t **matrix, uint8_t size)
{
  // horizontal patterns
  for (uint8_t col = 0; col < POS_PATTERN_SIZE + 1; col++)
  {
    setModuleValue(&(matrix[POS_PATTERN_SIZE][col]), 0);
    setModuleValue(&(matrix[POS_PATTERN_SIZE][size - POS_PATTERN_SIZE - 1 + 
      col]), 0);
    setModuleValue(&(matrix[size - POS_PATTERN_SIZE - 1][col]), 0);
  }

  // vertical patterns
  for (uint8_t row = 0; row < POS_PATTERN_SIZE; row++)
  {
    setModuleValue(&(matrix[row][POS_PATTERN_SIZE]), 0);
    setModuleValue(&(matrix[row][size - POS_PATTERN_SIZE - 1]), 0);
    setModuleValue(&(matrix[size - POS_PATTERN_SIZE + row][POS_PATTERN_SIZE]), 
      0);
  }
}

//------------------------------------------------------------------------------
///
/// @brief Creates the alignment pattern within the \p matrix
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
//
void mkAlignmentPattern(uint8_t **matrix, uint8_t pos)
{
  pos = pos - 2; // offset from pattern midpoint
  for (uint8_t row = 0; row < ALIGNMENT_PATTERN_SIZE; row++)
  {
    memcpy(&(matrix[pos + row][pos]), &(ALIGNMENT_PATTERN[row]), sizeof(uint8_t)
      * ALIGNMENT_PATTERN_SIZE);
  }
}

//------------------------------------------------------------------------------
///
/// @brief Creates the sync patterns within the \p matrix
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
//
void mkSyncPattern(uint8_t **matrix, uint8_t size)
{
  bool flag = 1;
  for (uint8_t row_col = POS_PATTERN_SIZE + 1; row_col < size - 1 - 
    POS_PATTERN_SIZE; row_col++)
  {
    // sync row
    setModuleValue(&(matrix[SYNC_PATTERN_POS][row_col]), flag);
    // sync column
    setModuleValue(&(matrix[row_col][SYNC_PATTERN_POS]), flag);
    flag = !flag;
  }
}

//------------------------------------------------------------------------------
///
/// @brief Marks the modules used for format- and version string as taken
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
//
void reserveFormatAndVersionModules(uint8_t **matrix, uint8_t size)
{
  uint8_t row = 0;
  uint8_t col = 0;
  for (uint8_t row_col = 0; row_col <= POS_PATTERN_SIZE; row_col++)
  {
    row = row_col;
    col = POS_PATTERN_SIZE + 1;
    // top left vertical
    if (!isModuleTaken(matrix[row][col])) 
    {
      setModuleTaken(&(matrix[row][col]), 1);
    }

    // top left horizontal
    row = POS_PATTERN_SIZE + 1;
    col = row_col;
    if (!isModuleTaken(matrix[row][col])) 
    {
      setModuleTaken(&(matrix[row][col]), 1);
    }

    // top right horizontal
    row = POS_PATTERN_SIZE + 1;
    col = size - 1 - POS_PATTERN_SIZE + row_col;
    setModuleTaken(&(matrix[row][col]), 1);


    // bottom left vertical
    if (row_col == POS_PATTERN_SIZE) continue;
    row = size - POS_PATTERN_SIZE + row_col;
    col = POS_PATTERN_SIZE + 1;
    setModuleTaken(&(matrix[row][col]), 1);
  }

  // top left corner module
  setModuleTaken(&(matrix)[POS_PATTERN_SIZE + 1][POS_PATTERN_SIZE + 1], 1);
}

//------------------------------------------------------------------------------
///
/// @brief Searches the next free module that can be used for payload data
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
///
/// @return Returns a pointer to the found module, NULL if none is found
//
uint8_t *getNextFreeModule(uint8_t **matrix, uint8_t size)
{
  static int16_t row = -1;
  static int16_t col = -1;
  static int8_t row_direction = UP;
  static bool next_row = false;


  if (col < 0) col = size - 1;
  if (row < 0) row = size - 1;

  while (col >= 0) {
    if (isModuleTaken(matrix[row][col])) 
    {
      if (next_row) 
      {
        row += row_direction;
        col++;
        // change vertical direction and go to the left
        if (row < 0 || row >= size) {
          if (row_direction == UP) row_direction = DOWN;
          else row_direction = UP;
          row += row_direction;
          col -= 2;
          if (col == 6) col--;
        }
      } 
      else 
      {
        col--;
      }
      next_row = !next_row;
      continue;
    }
    return &(matrix[row][col]);
  }
  return NULL;
}

//------------------------------------------------------------------------------
///
/// @brief Inserts the byte \p data_stream into the matrix
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
/// @param data_stream The byte stream that should be placed
/// @param data_size The length of the \p data_stream
//
void streamToPattern(uint8_t **matrix, uint8_t size, uint8_t *data_stream, 
uint8_t data_size)
{
  uint8_t *module = NULL;
  for (uint8_t counter = 0; counter < data_size; counter++)
  {
    for (int8_t bit_pos = 7; bit_pos >= 0; bit_pos--)
    {
      module = getNextFreeModule(matrix, size);
      if (module) {
        setModuleDataValue(module, (data_stream[counter] >> bit_pos) & 1);
      }
    }
  }
}

//------------------------------------------------------------------------------
///
/// @brief Inserts the data payload and ec-data into the matrix
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
/// @param message_data_stream The data byte stream
/// @param data_size The size of \p message_data_stream
/// @param ec_data_stream The error correction byte stream
/// @param ec_data_size The size of \p ec_data_stream
//
void mkDataPattern(uint8_t **matrix, uint8_t size, uint8_t *message_data_stream, 
uint8_t data_size, uint8_t *ec_data_stream, uint8_t ec_data_size)
{
  streamToPattern(matrix, size, message_data_stream, data_size);
  streamToPattern(matrix, size, ec_data_stream, ec_data_size);
}

//------------------------------------------------------------------------------
///
/// @brief Masks the data modules with the mask pattern
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
//
void maskData(uint8_t **matrix, uint8_t size)
{
  for (uint8_t row = 0; row < size; row++)
  {
    for (uint8_t col = 0; col < size; col++)
    {
      if (getModuleDataFlag(matrix[row][col]) || 
          !isModuleTaken(matrix[row][col]))
      {
        setModuleValue(&(matrix[row][col]), getModuleValue(matrix[row][col]) ^ 
          ((((col * row) % 2) + ((col * row) % 3)) % 2 == 0 ));
      }
    }
  }
}

//------------------------------------------------------------------------------
///
/// @brief Places the format and version info into the pre-reserved modules
/// 
/// @param matrix The matrix to use
/// @param size The matrix size
/// @param format_string The format and version data
//
void mkFormatVersionPattern(uint8_t **matrix, uint8_t size, uint32_t 
format_string)
{
  uint8_t col, row;
  for (uint8_t bit_pos = 0; bit_pos < FORMAT_VERSION_LENGTH; bit_pos++)
  {
    // first pattern
    if (bit_pos <= 7)
    {
      col = POS_PATTERN_SIZE + 1;
      row = bit_pos;
      if (bit_pos >= SYNC_PATTERN_POS) row++;
    }
    else 
    {
      col = POS_PATTERN_SIZE - (bit_pos - 8);
      if (col <= SYNC_PATTERN_POS) col--;
      row = POS_PATTERN_SIZE + 1;
    }
    setModuleValue(&(matrix[row][col]), (format_string >> bit_pos) & 1 );
    
    // second pattern
    if (bit_pos <= 7)
    {
      col = size - 1 - bit_pos;
      row = POS_PATTERN_SIZE + 1;
    }
    else 
    {
      col = POS_PATTERN_SIZE + 1;
      row = size - 1 - 6 + (bit_pos - 8);
    }
    setModuleValue(&(matrix[row][col]), (format_string >> bit_pos) & 1 );
  }
}

//------------------------------------------------------------------------------
/// @brief Checks the return codes of the EC-lib and handles the errors
/// 
/// @param return_value The return value the EC-lib has returned
//
void checkECCReturnValue(int return_value)
{
  if (return_value != ERROR_CORRECTION_RETURN_SUCCESSFUL) 
  {
    if (return_value == ERROR_CORRECTION_ERROR_OUT_OF_MEMORY)
    {
      printf("%s", "[ERR] Out of memory.\n");
      exit(ERR_ECC_OOM);
    } 
    else if (return_value == ERROR_CORRECTION_ERROR_INVALID_PARAMETER)
    {
      printf("%s", "[ERR] Function from errorcorrection library called with "
             "wrong parameters.\n"
      );
      exit(ERR_ECC_PARAMS);
    }
  }
}

//------------------------------------------------------------------------------
///
/// The main program.
/// Reads a string an generates a corresponding QR-code 
///
/// @param argc Could be used for bonus points - we don't use it
/// @param argv Could be used for bonus points - we don't use it
///
/// @return 0 on success, otherwise error code according to error codes enum and
/// the error that occured
//
int main(int argc, char** argv)
{
  unsigned char input_string[MAX_INPUT_STRING_SIZE + 1];
  int input;
  uint8_t len = 0;
  struct _QRFlavor_ flavor_to_use;
  struct _MessageData_ MessageData;
  uint8_t *message_data_stream;
  uint8_t *ec_data;
  uint8_t size;
  uint8_t **matrix;
  uint32_t format_string;
  uint8_t ec_level;
  int return_value;
  bool write_svg = false;
  bool write_csv = false;
  char filename[256];

  if (argc > 3) 
  {
    printf("%s", "Usage: ./ass3 [-b FILENAME]\n");
    exit(ERR_PARAMS);
  } 
  else if (argc == 3 && strcmp(argv[1], "-b") == 0)
  {
    write_svg = true;
    strcpy(filename, argv[2]);
  }
  else if (argc == 3 && strcmp(argv[1], "-c") == 0)
  {
    write_csv = true;
    strcpy(filename, argv[2]);
  }

  printf("--- QR-Code Encoder ---\n\nPlease enter a text:\n");

  do 
  {
    input = fgetc(stdin);
    if (input == '\n' || input == EOF) break;
    if (len == MAX_INPUT_STRING_SIZE)
    {
      printf("[ERR] Text to encode is too long, max. 106 bytes can be "
             "encoded.\n"
      );
      exit(ERR_TEXT_SIZE);
    }
    input_string[len] = input;
    len++;
  }
  while(true);
  input_string[len] = '\0';

  printf("\nMessage: %s\nLength: %i\n\n", input_string, len);

  for (uint8_t counter = 0; counter < NUMBER_OF_QR_FLAVORS; counter++) 
  {
      if (QRFlavors[counter].capacity_ < len) continue;
      flavor_to_use = QRFlavors[counter];
      break;
  }

  printf("QR-Code: %i-%c\n\n", flavor_to_use.version_, flavor_to_use.ec_level_);

  MessageData.mode_ = QR_MODE;
  MessageData.data_len_ = len;
  MessageData.data_ = malloc(sizeof(char)*len);

  for (uint8_t counter = 0; counter < len; counter++)
  {
    MessageData.data_[counter] = input_string[counter];
  }

  // data block
  message_data_stream = malloc(sizeof(uint8_t) * (flavor_to_use.capacity_ + 2));
  generateMessageDataStream(message_data_stream, &MessageData, flavor_to_use);

  printf("Message data codewords:\n");
  for (uint8_t counter = 0; counter < flavor_to_use.capacity_ + 2; counter++)
  {
    printf("0x%02X", message_data_stream[counter]);
    if (counter < flavor_to_use.capacity_ + 1) printf("%s", ", ");
  }
  printf("%s", "\n");


  // error correction
  ec_data = malloc(sizeof(uint8_t) * flavor_to_use.ec_data_);
  return_value = generateErrorCorrectionCodewords(ec_data, 
    flavor_to_use.ec_data_, message_data_stream, flavor_to_use.capacity_ + 2);
  checkECCReturnValue(return_value);

  printf("Error correction codewords:\n");
  for (uint8_t counter = 0; counter < flavor_to_use.ec_data_; counter++)
  {
    printf("0x%02X", ec_data[counter]);
    if (counter < flavor_to_use.ec_data_ - 1) printf("%s", ", ");
  }
  printf("%s", "\n");


  // message
  printf("Data codewords:\n");
  for (uint8_t counter = 0; counter < flavor_to_use.capacity_ + 2; counter++)
  {
    printf("0x%02X", message_data_stream[counter]);
    printf("%s", ", ");
  }
  
  for (uint8_t counter = 0; counter < flavor_to_use.ec_data_; counter++)
  {
    printf("0x%02X", ec_data[counter]);
    if (counter < flavor_to_use.ec_data_ - 1) printf("%s", ", ");
  }
  printf("%s", "\n");

  size = 21 + 4 * (flavor_to_use.version_ - 1);

  //printf("size: %i\n", size);

  // allocate memory for 2-dimensional matrix array
  matrix = malloc(sizeof(uint8_t*) * size);
  for (uint8_t row = 0; row < size; row++)
  {
    matrix[row] = malloc(sizeof(uint8_t*) * size);
    // initialize the newly allocated memory to zero
    memset((matrix[row]), 0, sizeof(uint8_t*) * size);
  }
  
  mkPositionPattern(matrix, size);

  mkSeparationPattern(matrix, size);

  if (flavor_to_use.alignment_pattern_pos_)
  {
    mkAlignmentPattern(matrix, flavor_to_use.alignment_pattern_pos_);
  }
  
  mkSyncPattern(matrix, size);

  // add fixed black module
  setModuleValue(&(matrix[4 * flavor_to_use.version_ + 9][8]), 1);

  reserveFormatAndVersionModules(matrix, size);

  mkDataPattern(matrix, size, message_data_stream, flavor_to_use.capacity_ + 2, 
    ec_data, flavor_to_use.ec_data_);

  printf("%s", "\nData matrix:\n");
  outputMatrix(matrix, size);

  maskData(matrix, size);

  switch (flavor_to_use.ec_level_) {
    case 'L':
      ec_level = 0;
      break;
    case 'M':
      ec_level = 1;
      break;
    case 'Q':
      ec_level = 2;
      break;
    case 'H':
      ec_level = 3;
      break;
    default:
      printf("%s", "Invalid EC Level");
      exit(ERR_ECC_PARAMS);
  }
  return_value = generateFormatString(&format_string, flavor_to_use.version_, 
    ec_level, MASK_PATTERN_ID);
  checkECCReturnValue(return_value);

  mkFormatVersionPattern(matrix, size, format_string);

  printf("\nMask id: %i\nFormat string: 0x%06X\n\nFinal matrix:\n", 
    MASK_PATTERN_ID, format_string);
  outputMatrix(matrix, size);

  if (write_svg) outputMatrixToSVGFile(matrix, size, filename);
  if (write_csv) outputMatrixToCSVFile(matrix, size, filename);


  free(MessageData.data_);
  free(message_data_stream);
  free(ec_data);
  for (uint8_t row = 0; row < size; row++)
  {
    free(matrix[row]);
  }
  free(matrix);

  return ERR_NO_ERROR;
}
