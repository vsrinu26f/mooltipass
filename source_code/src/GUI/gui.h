/* CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at src/license_cddl-1.0.txt
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at src/license_cddl-1.0.txt
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*!  \file     gui.h
*    \brief    General user interface
*    Created:  22/6/2014
*    Author:   Mathieu Stephan
*/


#ifndef GUI_H_
#define GUI_H_

/* DEFINES */
// Wheel interface
#define WHEEL_TICK_INCREMENT            32
// Timers
#define SCREEN_TIMER_DEL                60000
#define LIGHT_TIMER_DEL                 16000
#define TAP_MAX_DEL                     300
#define TAP_MIN_DEL                     100
#define MIN_USER_INTER_DEL              7000
#define MAX_USER_INTER_DEL              25000
#define SCROLLING_DEL                   500
// Screen defines
#if defined(HARDWARE_OLIVIER_V1)
    #define SCREEN_DEFAULT_NINSERTED            0x00
    #define SCREEN_DEFAULT_INSERTED_LCK         0x10
    #define SCREEN_DEFAULT_INSERTED_NLCK        0x20
    #define SCREEN_DEFAULT_INSERTED_INVALID     0x30
    #define SCREEN_SETTINGS                     0x40
    #define SCREEN_MEMORY_MGMT                  0x50
    #define SCREEN_DEFAULT_INSERTED_UNKNOWN     0x60
// Change for mini version
#elif defined(MINI_VERSION)
    #define SCREEN_LOCK                         0
    #define SCREEN_LOGIN                        1
    #define SCREEN_FAVORITES                    2
    #define SCREEN_SETTINGS                     3
    #define SCREEN_SETTINGS_CHANGE_PIN          4
    #define SCREEN_SETTINGS_BACKUP              5
    #define SCREEN_SETTINGS_HOME                6
    #define SCREEN_SETTINGS_ERASE               7
    #define SCREEN_MEMORY_MGMT                  8
    #define SCREEN_DEFAULT_NINSERTED            9
    #define SCREEN_DEFAULT_INSERTED_LCK         10
    #define SCREEN_DEFAULT_INSERTED_NLCK        SCREEN_LOGIN
    #define SCREEN_DEFAULT_INSERTED_INVALID     11
    #define SCREEN_DEFAULT_INSERTED_UNKNOWN     12
#endif
// Truncate defines
#define INDEX_TRUNCATE_SERVICE_SEARCH   14
#define INDEX_TRUNCATE_LOGIN_FAV        15
#define INDEX_TRUNCATE_SERVICE_CENTER   14

/* STRUCTS */
typedef struct
{
    char* lines[4];
} confirmationText_t;

#endif /* GUI_H_ */