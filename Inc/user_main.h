//
// Created by MightyPork on 2.9.16.
//

#ifndef F107_FFT_USER_MAIN_H
#define F107_FFT_USER_MAIN_H

void user_main();

void user_Error_Handler();

void user_assert_failed(uint8_t* file, uint32_t line);

void user_error_file_line(const char *message, const char *file, uint32_t line);

#endif //F107_FFT_USER_MAIN_H
