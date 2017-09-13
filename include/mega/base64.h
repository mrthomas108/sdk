/**
 * @file mega/base64.h
 * @brief modified base64 encoding/decoding
 *
 * (c) 2013-2014 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#ifndef MEGA_BASE64_H
#define MEGA_BASE64_H 1

#include <string>

namespace mega {
// modified base64 encoding/decoding (unpadded, -_ instead of +/)
class Base64
{
    static unsigned char to64(unsigned char);
    static unsigned char from64(unsigned char);

public:
    static int btoa(const std::string&, std::string&);
    static int btoa(const unsigned char*, int, char*);
    static int atob(const std::string&, std::string&);
    static int atob(const char*, unsigned char*, int);

    static void itoa(long int, std::string *);
    static long int atoi(std::string *);

};

// lowercase base32 encoding
class Base32
{
    static unsigned char to32(unsigned char);
    static unsigned char from32(unsigned char);

public:
    static int btoa(const unsigned char*, int, char*);
    static int atob(const char*, unsigned char*, int);
};

class URLCodec
{
    static bool ishexdigit(char c);
    static bool issafe(char c);
    static char hexval(char c);


public:
    static void escape(std::string* plain, std::string* escaped);
    static void unescape(std::string* escaped, std::string* plain);
};

} // namespace

#endif
