// Copyright (C) 1996 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

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
