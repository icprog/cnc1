/******************************************************************************/
/*Filename:    BGUI.h                                                         */
/*Project:     CNC 1                                                          */
/*Author:      New Rupture Systems                                            */
/*Description: Basic GUI library declarations.                                */
/******************************************************************************/
#ifndef BGUI_H
#define BGUI_H
#include <stdbool.h>

/*BGUI button object states*/
#define BGUI_BTN_DOWN (0x00)
#define BGUI_BTN_UP   (0x01)

//TODO:
//void BGUI_Create(void);
//void BGUI_Destroy(void);
bool BGUI_Open(void (*)(unsigned char, unsigned char));
void BGUI_Close(void);
bool BGUI_CreateButton(const char *restrict, unsigned int, unsigned char,
                       const unsigned int, const unsigned char,
                       const unsigned char);
void BGUI_DestroyButton(const unsigned char);
void BGUI_Hit(const unsigned int, const unsigned char);
void BGUI_Release(void);

#endif
