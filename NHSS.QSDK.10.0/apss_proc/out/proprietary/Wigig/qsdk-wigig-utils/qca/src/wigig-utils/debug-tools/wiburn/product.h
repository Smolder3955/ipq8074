/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PRODUCT_H_
#define _PRODUCT_H_

#include "wiburn.h"

#define MARLON_ID 612072
#define SPARROW_ID 632072
#define TALYN_ID 642072

class IProduct
{
public:
    IProduct(){}
    virtual ~IProduct(void){}
    virtual int Section2ID() = 0;
};

class talyn : public IProduct
{
public:
   typedef u_int32_t REG;
   typedef u_int32_t ADDRESS;
   static const u_int64_t id = TALYN_ID;

   virtual int Section2ID() {return 0;}
};

class sparrow : public IProduct
{
public:
    typedef u_int32_t REG;
    typedef u_int32_t ADDRESS;
    static const u_int64_t id = SPARROW_ID;

    virtual int Section2ID() {return 0;}
};

class marlon : public IProduct
{
public:
    typedef u_int32_t REG;
    typedef u_int32_t ADDRESS;
    static const u_int64_t id = MARLON_ID;

    virtual int Section2ID() {return 0;}
};

#endif //#ifndef _PRODUCT_H_
