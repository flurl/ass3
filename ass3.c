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
const uint8_t SYNC_PATTERN_POS = 6;
const uint8_t FORMAT_VERSION_POS = 8;

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
const uint8_t ALIGNMENT_PATTERN[ALIGNMENT_PATTERN_SIZE][ALIGNMENT_PATTERN_SIZE] =
{
  {3, 3, 3, 3, 3},
  {3, 2, 2, 2, 3},
  {3, 2, 3, 2, 3},
  {3, 2, 2, 2, 3},
  {3, 3, 3, 3, 3}
};

enum 
{
  ERR_PARAMS = 1,
  ERR_OOM = 2,
  ERR_TEXT_SIZE = 3,
  ERR_ECC = 4,
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
  {.capacity_ =   7, .version_ = 1, .ec_level_ = 'H', .ec_data_ = 17, .alignment_pattern_pos_ = 0},
  {.capacity_ =  11, .version_ = 1, .ec_level_ = 'Q', .ec_data_ = 13, .alignment_pattern_pos_ = 0},
  {.capacity_ =  14, .version_ = 1, .ec_level_ = 'M', .ec_data_ = 10, .alignment_pattern_pos_ = 0},
  {.capacity_ =  17, .version_ = 1, .ec_level_ = 'L', .ec_data_ =  7, .alignment_pattern_pos_ = 0},
  {.capacity_ =  20, .version_ = 2, .ec_level_ = 'Q', .ec_data_ = 22, .alignment_pattern_pos_ = 18},
  {.capacity_ =  26, .version_ = 2, .ec_level_ = 'M', .ec_data_ = 16, .alignment_pattern_pos_ = 18},
  {.capacity_ =  32, .version_ = 2, .ec_level_ = 'L', .ec_data_ = 10, .alignment_pattern_pos_ = 18},
  {.capacity_ =  42, .version_ = 3, .ec_level_ = 'M', .ec_data_ = 26, .alignment_pattern_pos_ = 22},
  {.capacity_ =  53, .version_ = 3, .ec_level_ = 'L', .ec_data_ = 15, .alignment_pattern_pos_ = 22},
  {.capacity_ =  78, .version_ = 4, .ec_level_ = 'L', .ec_data_ = 20, .alignment_pattern_pos_ = 26},
  {.capacity_ = 106, .version_ = 5, .ec_level_ = 'L', .ec_data_ = 26, .alignment_pattern_pos_ = 26},
};


void setModuleTaken(uint8_t *module, uint8_t taken) {
  if (taken) *module |= (1 << MODULE_TAKEN_BIT);
  else *module &= (1 << MODULE_TAKEN_BIT);
}

uint8_t isModuleTaken(uint8_t module) {
  return module & (1 << MODULE_TAKEN_BIT);
}

void setModuleValue(uint8_t *module, uint8_t value)
{
  if (value) *module |= (1 << MODULE_VALUE_BIT);
  else *module &= (0 << MODULE_VALUE_BIT);
  setModuleTaken(module, 1);
}

uint8_t getModuleValue(uint8_t module)
{
  return module & 1;
}


// shortcut function
/* void setModuleValueAndTaken(uint8_t *module, uint8_t flag) {
  setModuleValue(module, flag);
  setModuleTaken(module, flag);
} */


void outputMatrix(uint8_t **matrix, uint8_t size)
{
  for (uint8_t row = 0; row < size; row++)
  {
    for (uint8_t column = 0; column < size; column++)
    {
      printf("%c ", (getModuleValue(matrix[row][column]) == 1) ? '#' : (isModuleTaken(matrix[row][column])) ? '0' : ' ');
    }
    printf("%s", "\n");
  }
}

void generateMessageDataStream(uint8_t *md_stream, struct _MessageData_ *md, struct _QRFlavor_ flavor) 
{
  md_stream[0] = 0 | (QR_MODE << NIBBLE_SIZE);
  md_stream[0] |= (md->data_len_ >> NIBBLE_SIZE);
  md_stream[1] = 0 | (md->data_len_ << NIBBLE_SIZE);
  for (uint8_t counter = 1; counter <= md->data_len_; counter++) 
  {
    md_stream[counter] |= (md->data_[counter - 1] >> NIBBLE_SIZE);
    md_stream[counter + 1] = 0 | (md->data_[counter - 1] << NIBBLE_SIZE);
  }

  // terminate with zeros
  md_stream[md->data_len_+1] &= 0xF0;

  // padd with 0xEC11
  bool flag = true;
  for (uint8_t counter = md->data_len_ + 2; counter < flavor.capacity_ + 2; counter++) 
  {
    md_stream[counter] = (flag) ? 0xEC : 0x11;
    flag = !flag;
  }

};


void mkPositionPattern(uint8_t **matrix, uint8_t size) 
{
  for (uint8_t row = 0; row < POS_PATTERN_SIZE; row++)
  {
    memcpy(&(matrix[row][0]), &(POS_PATTERN[row]), sizeof(uint8_t) * POS_PATTERN_SIZE);
    memcpy(&(matrix[row][size - POS_PATTERN_SIZE]), &(POS_PATTERN[row]), sizeof(uint8_t) * POS_PATTERN_SIZE);
    memcpy(&(matrix[size - POS_PATTERN_SIZE + row][0]), &(POS_PATTERN[row]), sizeof(uint8_t) * POS_PATTERN_SIZE);
  }
}


void mkSeparationPattern(uint8_t **matrix, uint8_t size)
{
  for (uint8_t col = 0; col < POS_PATTERN_SIZE + 1; col++)
  {
    setModuleValue(&(matrix[POS_PATTERN_SIZE][col]), 0);
    setModuleValue(&(matrix[POS_PATTERN_SIZE][size - POS_PATTERN_SIZE - 1 + col]), 0);
    setModuleValue(&(matrix[size - POS_PATTERN_SIZE - 1][col]), 0);
  }

  for (uint8_t row = 0; row < POS_PATTERN_SIZE; row++)
  {
    setModuleValue(&(matrix[row][POS_PATTERN_SIZE]), 0);
    setModuleValue(&(matrix[row][size - POS_PATTERN_SIZE - 1]), 0);
    setModuleValue(&(matrix[size - POS_PATTERN_SIZE + row][POS_PATTERN_SIZE]), 0);
  }
}

void mkAlignmentPattern(uint8_t **matrix, uint8_t pos)
{
  pos = pos - 2; // offset from pattern midpoint
  for (uint8_t row = 0; row < ALIGNMENT_PATTERN_SIZE; row++)
  {
    memcpy(&(matrix[pos + row][pos]), &(ALIGNMENT_PATTERN[row]), sizeof(uint8_t) * ALIGNMENT_PATTERN_SIZE);
  }
}

void mkSyncPattern(uint8_t **matrix, uint8_t size)
{
  bool flag = 1;
  for (uint8_t row_col = POS_PATTERN_SIZE + 1; row_col < size - 1 - POS_PATTERN_SIZE; row_col++)
  {
    // sync row
    setModuleValue(&(matrix[SYNC_PATTERN_POS][row_col]), flag);
    setModuleTaken(&(matrix[SYNC_PATTERN_POS][row_col]), 1);

    // sync column
    setModuleValue(&(matrix[row_col][SYNC_PATTERN_POS]), flag);
    setModuleTaken(&(matrix[row_col][SYNC_PATTERN_POS]), 1);
    
    flag = !flag;
  }
}


void reserveFormatAndVersionModules(uint8_t **matrix, uint8_t size)
{
  uint8_t row, col;
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


uint8_t *getNextFreeModule(uint8_t **matrix, uint8_t size)
{
  static int16_t row = -1;
  static int16_t col = -1;
  static int8_t row_direction = UP;
  //static int8_t col_direction = LEFT;
  //uint8_t *found_module = NULL;
  static bool next_row = false;


  if (col < 0) col = size - 1;
  if (row < 0) row = size - 1;

  while (col >= 0) {
    
    //printf("%i %i %02x %i\n", row, col, matrix[row][col], isModuleTaken(matrix[row][col]));
    if (isModuleTaken(matrix[row][col])) 
    {
      if (next_row) 
      {
        row += row_direction;
        col++;
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
    //printf("%s", "free module found\n");
    return &(matrix[row][col]);
    
    /*if (!isModuleTaken(matrix[row][col])) return &(matrix[row][col]);
    col++;
    row += row_direction;

    

    if (col_direction == LEFT) col_direction = RIGHT;
    else col_direction = LEFT;

    
    row += row_direction;*/
    
  }

  return NULL;

}

void streamToPattern(uint8_t **matrix, uint8_t size, uint8_t *data_stream, uint8_t data_size)
{
  uint8_t *module;

  // the data
  for (uint8_t counter = 0; counter < data_size; counter++)
  {
    for (int8_t bit_pos = 7; bit_pos >= 0; bit_pos--)
    {
      //printf("%i %i", counter, bit_pos);
      //if (getNextFreeModule(matrix, size)) printf("%s\n", "freeModuleFound");
      module = getNextFreeModule(matrix, size);
      if (module) {
        setModuleValue(module, data_stream[counter] & (1 << bit_pos));
      }
    }
  }
}

void mkDataPattern(uint8_t **matrix, uint8_t size, uint8_t *message_data_stream, uint8_t data_size, uint8_t *ec_data_stream, uint8_t ec_data_size)
{
  streamToPattern(matrix, size, message_data_stream, data_size);
  streamToPattern(matrix, size, ec_data_stream, ec_data_size);
  // the ec information


}


int main(int argc, char** argv)
{

  unsigned char input_string[MAX_INPUT_STRING_SIZE + 1];
  unsigned char input;
  uint8_t len = 0;
  struct _QRFlavor_ flavor_to_use;
  struct _MessageData_ MessageData;
  uint8_t *message_data_stream;
  uint8_t *ec_data;
  uint8_t size;
  uint8_t **matrix; // TODO: find a better datatype?

  printf("--- QR-Code Encoder ---\n\nPlease enter a text:\n");

  do 
  {
    input = fgetc(stdin);
    if (input == '\n' || input == EOF) break;
    if (len == MAX_INPUT_STRING_SIZE)
    {
      printf("[ERR] Text to encode is too long, max. 106 bytes can be encoded.\n");
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
    if (counter < flavor_to_use.capacity_ + 2) printf("%s", ", ");
  }
  printf("%s", "\n");


  // error correction
  ec_data = malloc(sizeof(uint8_t) * flavor_to_use.ec_data_);
  generateErrorCorrectionCodewords(ec_data, flavor_to_use.ec_data_, message_data_stream, flavor_to_use.capacity_ + 2);

  printf("Error correction codewords:\n");
  for (uint8_t counter = 0; counter < flavor_to_use.ec_data_; counter++)
  {
    printf("0x%02X", ec_data[counter]);
    if (counter < flavor_to_use.ec_data_) printf("%s", ", ");
  }
  printf("%s", "\n");


  // message
  printf("Message data codewords:\n");
  for (uint8_t counter = 0; counter < flavor_to_use.capacity_ + 2; counter++)
  {
    printf("0x%02X", message_data_stream[counter]);
    printf("%s", ", ");
  }
  
  for (uint8_t counter = 0; counter < flavor_to_use.ec_data_; counter++)
  {
    printf("0x%02X", ec_data[counter]);
    if (counter < flavor_to_use.ec_data_) printf("%s", ", ");
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

  mkDataPattern(matrix, size, message_data_stream, flavor_to_use.capacity_ + 2, ec_data, flavor_to_use.ec_data_);

  //printf("%s", "BP1");

  outputMatrix(matrix, size);
}