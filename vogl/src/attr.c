#include <stdio.h>
#include "vogl.h"

static	Astack	*asfree = (Astack *)NULL;

/*
 * copyattributes
 *
 *	Copies attribute stack entries from b to a
 */
static	void
copyattributes(Attribute *a, Attribute *b)
{
	a->color = b->color;
	a->fontnum = b->fontnum;
	a->ls = b->ls;
	a->lw = b->lw;
	a->backface = b->backface;
}

/*
 * pushattributes
 *
 * save the current attributes on the matrix stack
 *
 */
void
pushattributes(void)
{
	Astack	*nattr;
	Token	*p;

	if (!vdevice.initialised)
		verror("pushattributes:  vogl not initialised");
	
	if (vdevice.inobject) {
		p = newtokens(1);

		p[0].i = PUSHATTRIBUTES;

		return;
	}

	if (asfree != (Astack *)NULL) {
		nattr = vdevice.attr;
		vdevice.attr = asfree;
		asfree = asfree->back;
		vdevice.attr->back = nattr;
		copyattributes(&vdevice.attr->a, &nattr->a);
	} else {	
		nattr = (Astack *)vallocate(sizeof(Astack));
		nattr->back = vdevice.attr;
		copyattributes(&nattr->a, &vdevice.attr->a);
		vdevice.attr = nattr;
	}
}

/*
 * popattributes
 *
 * pop the top entry on the attribute stack 
 *
 */
void
popattributes(void)
{
	Astack	*nattr;
	Token	*p;

	if (!vdevice.initialised)
		verror("popattributes: vogl not initialised");
	
	if (vdevice.inobject) {
		p = newtokens(1);

		p[0].i = POPATTRIBUTES;

		return;
	}

	if (vdevice.attr->back == (Astack *)NULL) 
		verror("popattributes: attribute stack is empty");
	else {
		font(vdevice.attr->back->a.fontnum);
		nattr = vdevice.attr;
		vdevice.attr = vdevice.attr->back;
		nattr->back = asfree;
		asfree = nattr;
	}

	if (vdevice.dev.Vsetls) (*vdevice.dev.Vsetls)(vdevice.attr->a.ls);
	if (vdevice.dev.Vsetlw) (*vdevice.dev.Vsetlw)(vdevice.attr->a.lw);

	color(vdevice.attr->a.color);
}
