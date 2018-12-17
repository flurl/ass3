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

enum 
{
  ERR_PARAMS = 1,
  ERR_OOM = 2,
  ERR_TEXT_SIZE = 3,
  ERR_ECC = 4,
  ERR_IO = 5
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
}; 

const struct _QRFlavor_ QRFlavors[] = 
{
  {.capacity_ =   7, .version_ = 1, .ec_level_ = 'H', .ec_data_ = 17},
  {.capacity_ =  11, .version_ = 1, .ec_level_ = 'Q', .ec_data_ = 13},
  {.capacity_ =  14, .version_ = 1, .ec_level_ = 'M', .ec_data_ = 10},
  {.capacity_ =  17, .version_ = 1, .ec_level_ = 'L', .ec_data_ =  7},
  {.capacity_ =  20, .version_ = 2, .ec_level_ = 'Q', .ec_data_ = 22},
  {.capacity_ =  26, .version_ = 2, .ec_level_ = 'M', .ec_data_ = 16},
  {.capacity_ =  32, .version_ = 2, .ec_level_ = 'L', .ec_data_ = 10},
  {.capacity_ =  42, .version_ = 3, .ec_level_ = 'M', .ec_data_ = 26},
  {.capacity_ =  53, .version_ = 3, .ec_level_ = 'L', .ec_data_ = 15},
  {.capacity_ =  78, .version_ = 4, .ec_level_ = 'L', .ec_data_ = 20},
  {.capacity_ = 106, .version_ = 5, .ec_level_ = 'L', .ec_data_ = 26},
};


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


int main(int argc, char** argv)
{

  unsigned char input_string[MAX_INPUT_STRING_SIZE + 1];
  unsigned char input;
  uint8_t len = 0;
  struct _QRFlavor_ flavor_to_use;
  struct _MessageData_ MessageData;
  uint8_t *message_data_stream;
  uint8_t *ec_data;

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

}