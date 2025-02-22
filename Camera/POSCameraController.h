/* 
 * This file is part of the positron distribution (https://github.com/radredgreen/positron).
 * Copyright (c) 2024 RadRedGreen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef POSCAMERACONTROLLER_H
#define POSCAMERACONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"
#include "HAPBase.h"

//#include "App_Camera.h"
#include "App.h"
#include <pthread.h>



void StreamContextInitialize(AccessoryContext* context);
void StreamContextDeintialize(AccessoryContext* context); 
void posStartStream(AccessoryContext* context HAP_UNUSED);
void posStopStream(AccessoryContext* context HAP_UNUSED);
void posReconfigureStream(AccessoryContext* context HAP_UNUSED);
void posSuspendStream(AccessoryContext* context HAP_UNUSED) ;
void posResumeStream(AccessoryContext* context HAP_UNUSED) ;


#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
