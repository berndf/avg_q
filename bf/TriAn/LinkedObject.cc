// Copyright (C) 1996 Bernd Feige
// This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

#include "LinkedObject.hh"

LinkedObject::LinkedObject() {
 previous_member=Empty;
 next_member=Empty;
// printf("LinkedObject() called!\n");
}
LinkedObject::~LinkedObject() {
// printf("LinkedObject Destructor called, Position=%d %d\n", before_members(), after_members());
 unlink();
}
void LinkedObject::unlink() {
 if (previous_member!=Empty) previous_member->next_member=next_member;
 if (next_member!=Empty) next_member->previous_member=previous_member;
 previous_member=Empty;
 next_member=Empty;
}
LinkedObject* LinkedObject::previous() {
 return previous_member;
}
LinkedObject* LinkedObject::next() {
 return next_member;
}
LinkedObject* LinkedObject::first() {
 LinkedObject* inmember=this;
 while (inmember->previous_member!=Empty) inmember=inmember->previous_member;
 return inmember;
}
LinkedObject* LinkedObject::first(int index) {
 LinkedObject* inmember=first();
 int i;
 for (i=0; inmember!=Empty && i<index; i++) {
  inmember=inmember->next_member;
 }
 return inmember;
}
LinkedObject* LinkedObject::last() {
 LinkedObject* inmember=this;
 while (inmember->next_member!=Empty) inmember=inmember->next_member;
 return inmember;
}
LinkedObject* LinkedObject::ringprevious() {
 if (previous_member==Empty) return last();
 else return previous_member;
}
LinkedObject* LinkedObject::ringnext() {
 if (next_member==Empty) return first();
 else return next_member;
}
int LinkedObject::nr_of_members() {
 int nr_of_LinkedObject=1;
 LinkedObject* inmember=first();
 while (inmember->next_member!=Empty) {
  nr_of_LinkedObject++;
  inmember=inmember->next_member;
 }
 return nr_of_LinkedObject;
}
int LinkedObject::before_members() {
 int before=0;
 LinkedObject* inmember=this;
 while (inmember->previous_member!=Empty) {
  before++;
  inmember=inmember->previous_member;
 }
 return before;
}
int LinkedObject::after_members() {
 int after=0;
 LinkedObject* inmember=this;
 while (inmember->next_member!=Empty) {
  after++;
  inmember=inmember->next_member;
 }
 return after;
}
LinkedObject* LinkedObject::addmember(LinkedObject* n) {
 if (this!=Empty) {
  n->previous_member=this;
  if (next_member!=Empty) {
   n->next_member=next_member;
   n->next_member->previous_member=n;
  }
  next_member=n;
 }
 return n;
}
void LinkedObject::delall() {
 LinkedObject* inmember=first();
 while (inmember!=Empty) {
  LinkedObject* nextmember=inmember->next_member;
  delete inmember;
  inmember=nextmember;
 }
}
