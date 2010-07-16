/* 
*  Copyright (c) 2010,
*  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
*  
*  This file is part of crtmpserver.
*  crtmpserver is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*  
*  crtmpserver is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef _ATOMFTYP_H
#define _ATOMFTYP_H

#include "mediaformats/mp4/baseatom.h"

class AtomFTYP
: public BaseAtom {
private:
    uint32_t _majorBrand;
    uint32_t _minorVersion;
    vector<uint32_t> _compatibleBrands;
public:
    AtomFTYP(MP4Document *pDocument, uint32_t type, uint64_t size, uint64_t start);
    virtual ~AtomFTYP();

    virtual bool Read();

    virtual string Hierarchy(uint32_t indent);
};

#endif	/* _ATOMFTYP_H */


