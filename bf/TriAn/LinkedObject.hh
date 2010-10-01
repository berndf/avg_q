// Copyright (C) 1996 Bernd Feige
// 
// This file is part of avg_q.
// 
// avg_q is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// avg_q is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
#ifndef _LinkedObject_HH
#define _LinkedObject_HH

#include "General.hh"

class LinkedObject {
public:
 LinkedObject();
 virtual ~LinkedObject();
 void unlink();
 LinkedObject* previous();
 LinkedObject* next();
 LinkedObject* first();
 LinkedObject* first(int index);
 LinkedObject* last();
 LinkedObject* ringnext();
 LinkedObject* ringprevious();
 LinkedObject* addmember(LinkedObject* n);
 int nr_of_members();
 int before_members();
 int after_members();
 void delall();
 Bool is_unlinked;
 Bool is_first;
 Bool is_last;
private:
 LinkedObject* previous_member;
 LinkedObject* next_member;
};
#define Empty (LinkedObject*)0

#endif
