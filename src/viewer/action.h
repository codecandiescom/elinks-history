/* $Id: action.h,v 1.12 2005/06/13 21:21:10 jonas Exp $ */

#ifndef EL__VIEWER_ACTION_H
#define EL__VIEWER_ACTION_H

#include "config/kbdbind.h"
#include "viewer/text/view.h"

struct session;

enum frame_event_status do_action(struct session *ses,
                                  enum main_action action_id, int verbose);

#endif
