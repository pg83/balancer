/*
 *  Copyright 2001-2006 Adrian Thurston <thurston@cs.queensu.ca>
 *            2004 Erich Ocean <eric.ocean@ampede.com>
 *            2005 Alan West <alan@alanz.com>
 */

/*  This file is part of Ragel.
 *
 *  Ragel is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  Ragel is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with Ragel; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include "rlgen-cd.h"
#include "gotocodegen.h"
#include "redfsm.h"
#include "bstmap.h"
#include "gendata.h"

/* Emit the goto to take for a given transition. */
std::ostream &GotoCodeGen::TRANS_GOTO( RedTransAp *trans, int level )
{
	out << TABS(level) << "goto tr" << trans->id << ";";
	return out;
}

std::ostream &GotoCodeGen::TO_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( ActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numToStateRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\tcase " << act->actionId << ":\n";
			ACTION( out, act, 0, false );
			out << "\tbreak;\n";
		}
	}

	genLineDirective( out );
	return out;
}

std::ostream &GotoCodeGen::FROM_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( ActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numFromStateRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\tcase " << act->actionId << ":\n";
			ACTION( out, act, 0, false );
			out << "\tbreak;\n";
		}
	}

	genLineDirective( out );
	return out;
}

std::ostream &GotoCodeGen::EOF_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( ActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numEofRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\tcase " << act->actionId << ":\n";
			ACTION( out, act, 0, true );
			out << "\tbreak;\n";
		}
	}

	genLineDirective( out );
	return out;
}

std::ostream &GotoCodeGen::ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( ActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numTransRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\tcase " << act->actionId << ":\n";
			ACTION( out, act, 0, false );
			out << "\tbreak;\n";
		}
	}

	genLineDirective( out );
	return out;
}

void GotoCodeGen::GOTO_HEADER( RedStateAp *state )
{
	/* Label the state. */
	out << "case " << state->id << ":\n";
}


void GotoCodeGen::emitSingleSwitch( RedStateAp *state )
{
	/* Load up the singles. */
	int numSingles = state->outSingle.length();
	RedTransEl *data = state->outSingle.data;

	if ( numSingles == 1 ) {
		/* If there is a single single key then write it out as an if. */
		out << "\tif ( " << GET_WIDE_KEY(state) << " == " << 
				KEY(data[0].lowKey) << " )\n\t\t"; 

		/* Virtual function for writing the target of the transition. */
		TRANS_GOTO(data[0].value, 0) << "\n";
	}
	else if ( numSingles > 1 ) {
		/* Write out single keys in a switch if there is more than one. */
		out << "\tswitch( " << GET_WIDE_KEY(state) << " ) {\n";

		/* Write out the single indicies. */
		for ( int j = 0; j < numSingles; j++ ) {
			out << "\t\tcase " << KEY(data[j].lowKey) << ": ";
			TRANS_GOTO(data[j].value, 0) << "\n";
		}
		
		/* Emits a default case for D code. */
		SWITCH_DEFAULT();

		/* Close off the transition switch. */
		out << "\t}\n";
	}
}

void GotoCodeGen::emitRangeBSearch( RedStateAp *state, int level, int low, int high )
{
	/* Get the mid position, staying on the lower end of the range. */
	int mid = (low + high) >> 1;
	RedTransEl *data = state->outRange.data;

	/* Determine if we need to look higher or lower. */
	bool anyLower = mid > low;
	bool anyHigher = mid < high;

	/* Determine if the keys at mid are the limits of the alphabet. */
	bool limitLow = data[mid].lowKey == keyOps->minKey;
	bool limitHigh = data[mid].highKey == keyOps->maxKey;

	if ( anyLower && anyHigher ) {
		/* Can go lower and higher than mid. */
		out << TABS(level) << "if ( " << GET_WIDE_KEY(state) << " < " << 
				KEY(data[mid].lowKey) << " ) {\n";
		emitRangeBSearch( state, level+1, low, mid-1 );
		out << TABS(level) << "} else if ( " << GET_WIDE_KEY(state) << " > " << 
				KEY(data[mid].highKey) << " ) {\n";
		emitRangeBSearch( state, level+1, mid+1, high );
		out << TABS(level) << "} else\n";
		TRANS_GOTO(data[mid].value, level+1) << "\n";
	}
	else if ( anyLower && !anyHigher ) {
		/* Can go lower than mid but not higher. */
		out << TABS(level) << "if ( " << GET_WIDE_KEY(state) << " < " << 
				KEY(data[mid].lowKey) << " ) {\n";
		emitRangeBSearch( state, level+1, low, mid-1 );

		/* if the higher is the highest in the alphabet then there is no
		 * sense testing it. */
		if ( limitHigh ) {
			out << TABS(level) << "} else\n";
			TRANS_GOTO(data[mid].value, level+1) << "\n";
		}
		else {
			out << TABS(level) << "} else if ( " << GET_WIDE_KEY(state) << " <= " << 
					KEY(data[mid].highKey) << " )\n";
			TRANS_GOTO(data[mid].value, level+1) << "\n";
		}
	}
	else if ( !anyLower && anyHigher ) {
		/* Can go higher than mid but not lower. */
		out << TABS(level) << "if ( " << GET_WIDE_KEY(state) << " > " << 
				KEY(data[mid].highKey) << " ) {\n";
		emitRangeBSearch( state, level+1, mid+1, high );

		/* If the lower end is the lowest in the alphabet then there is no
		 * sense testing it. */
		if ( limitLow ) {
			out << TABS(level) << "} else\n";
			TRANS_GOTO(data[mid].value, level+1) << "\n";
		}
		else {
			out << TABS(level) << "} else if ( " << GET_WIDE_KEY(state) << " >= " << 
					KEY(data[mid].lowKey) << " )\n";
			TRANS_GOTO(data[mid].value, level+1) << "\n";
		}
	}
	else {
		/* Cannot go higher or lower than mid. It's mid or bust. What
		 * tests to do depends on limits of alphabet. */
		if ( !limitLow && !limitHigh ) {
			out << TABS(level) << "if ( " << KEY(data[mid].lowKey) << " <= " << 
					GET_WIDE_KEY(state) << " && " << GET_WIDE_KEY(state) << " <= " << 
					KEY(data[mid].highKey) << " )\n";
			TRANS_GOTO(data[mid].value, level+1) << "\n";
		}
		else if ( limitLow && !limitHigh ) {
			out << TABS(level) << "if ( " << GET_WIDE_KEY(state) << " <= " << 
					KEY(data[mid].highKey) << " )\n";
			TRANS_GOTO(data[mid].value, level+1) << "\n";
		}
		else if ( !limitLow && limitHigh ) {
			out << TABS(level) << "if ( " << KEY(data[mid].lowKey) << " <= " << 
					GET_WIDE_KEY(state) << " )\n";
			TRANS_GOTO(data[mid].value, level+1) << "\n";
		}
		else {
			/* Both high and low are at the limit. No tests to do. */
			TRANS_GOTO(data[mid].value, level+1) << "\n";
		}
	}
}

void GotoCodeGen::STATE_GOTO_ERROR()
{
	/* Label the state and bail immediately. */
	outLabelUsed = true;
	RedStateAp *state = redFsm->errState;
	out << "case " << state->id << ":\n";
	out << "	goto _out;\n";
}

void GotoCodeGen::COND_TRANSLATE( StateCond *stateCond, int level )
{
	CondSpace *condSpace = stateCond->condSpace;
	out << TABS(level) << "_widec = " << CAST(WIDE_ALPH_TYPE()) << "(" <<
			KEY(condSpace->baseKey) << " + (" << GET_KEY() << 
			" - " << KEY(keyOps->minKey) << "));\n";

	for ( CondSet::Iter csi = condSpace->condSet; csi.lte(); csi++ ) {
		out << TABS(level) << "if ( ";
		CONDITION( out, *csi );
		Size condValOffset = ((1 << csi.pos()) * keyOps->alphSize());
		out << " ) _widec += " << condValOffset << ";\n";
	}
}

void GotoCodeGen::emitCondBSearch( RedStateAp *state, int level, int low, int high )
{
	/* Get the mid position, staying on the lower end of the range. */
	int mid = (low + high) >> 1;
	StateCond **data = state->stateCondVect.data;

	/* Determine if we need to look higher or lower. */
	bool anyLower = mid > low;
	bool anyHigher = mid < high;

	/* Determine if the keys at mid are the limits of the alphabet. */
	bool limitLow = data[mid]->lowKey == keyOps->minKey;
	bool limitHigh = data[mid]->highKey == keyOps->maxKey;

	if ( anyLower && anyHigher ) {
		/* Can go lower and higher than mid. */
		out << TABS(level) << "if ( " << GET_KEY() << " < " << 
				KEY(data[mid]->lowKey) << " ) {\n";
		emitCondBSearch( state, level+1, low, mid-1 );
		out << TABS(level) << "} else if ( " << GET_KEY() << " > " << 
				KEY(data[mid]->highKey) << " ) {\n";
		emitCondBSearch( state, level+1, mid+1, high );
		out << TABS(level) << "} else {\n";
		COND_TRANSLATE(data[mid], level+1);
		out << TABS(level) << "}\n";
	}
	else if ( anyLower && !anyHigher ) {
		/* Can go lower than mid but not higher. */
		out << TABS(level) << "if ( " << GET_KEY() << " < " << 
				KEY(data[mid]->lowKey) << " ) {\n";
		emitCondBSearch( state, level+1, low, mid-1 );

		/* if the higher is the highest in the alphabet then there is no
		 * sense testing it. */
		if ( limitHigh ) {
			out << TABS(level) << "} else {\n";
			COND_TRANSLATE(data[mid], level+1);
			out << TABS(level) << "}\n";
		}
		else {
			out << TABS(level) << "} else if ( " << GET_KEY() << " <= " << 
					KEY(data[mid]->highKey) << " ) {\n";
			COND_TRANSLATE(data[mid], level+1);
			out << TABS(level) << "}\n";
		}
	}
	else if ( !anyLower && anyHigher ) {
		/* Can go higher than mid but not lower. */
		out << TABS(level) << "if ( " << GET_KEY() << " > " << 
				KEY(data[mid]->highKey) << " ) {\n";
		emitCondBSearch( state, level+1, mid+1, high );

		/* If the lower end is the lowest in the alphabet then there is no
		 * sense testing it. */
		if ( limitLow ) {
			out << TABS(level) << "} else {\n";
			COND_TRANSLATE(data[mid], level+1);
			out << TABS(level) << "}\n";
		}
		else {
			out << TABS(level) << "} else if ( " << GET_KEY() << " >= " << 
					KEY(data[mid]->lowKey) << " ) {\n";
			COND_TRANSLATE(data[mid], level+1);
			out << TABS(level) << "}\n";
		}
	}
	else {
		/* Cannot go higher or lower than mid. It's mid or bust. What
		 * tests to do depends on limits of alphabet. */
		if ( !limitLow && !limitHigh ) {
			out << TABS(level) << "if ( " << KEY(data[mid]->lowKey) << " <= " << 
					GET_KEY() << " && " << GET_KEY() << " <= " << 
					KEY(data[mid]->highKey) << " ) {\n";
			COND_TRANSLATE(data[mid], level+1);
			out << TABS(level) << "}\n";
		}
		else if ( limitLow && !limitHigh ) {
			out << TABS(level) << "if ( " << GET_KEY() << " <= " << 
					KEY(data[mid]->highKey) << " ) {\n";
			COND_TRANSLATE(data[mid], level+1);
			out << TABS(level) << "}\n";
		}
		else if ( !limitLow && limitHigh ) {
			out << TABS(level) << "if ( " << KEY(data[mid]->lowKey) << " <= " << 
					GET_KEY() << " )\n {";
			COND_TRANSLATE(data[mid], level+1);
			out << TABS(level) << "}\n";
		}
		else {
			/* Both high and low are at the limit. No tests to do. */
			COND_TRANSLATE(data[mid], level);
		}
	}
}

std::ostream &GotoCodeGen::STATE_GOTOS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		if ( st == redFsm->errState )
			STATE_GOTO_ERROR();
		else {
			/* Writing code above state gotos. */
			GOTO_HEADER( st );

			if ( st->stateCondVect.length() > 0 ) {
				out << "	_widec = " << GET_KEY() << ";\n";
				emitCondBSearch( st, 1, 0, st->stateCondVect.length() - 1 );
			}

			/* Try singles. */
			if ( st->outSingle.length() > 0 )
				emitSingleSwitch( st );

			/* Default case is to binary search for the ranges, if that fails then */
			if ( st->outRange.length() > 0 )
				emitRangeBSearch( st, 1, 0, st->outRange.length() - 1 );

			/* Write the default transition. */
			TRANS_GOTO( st->defTrans, 1 ) << "\n";
		}
	}
	return out;
}

std::ostream &GotoCodeGen::TRANSITIONS()
{
	/* Emit any transitions that have functions and that go to 
	 * this state. */
	for ( TransApSet::Iter trans = redFsm->transSet; trans.lte(); trans++ ) {
		/* Write the label for the transition so it can be jumped to. */
		out << "	tr" << trans->id << ": ";

		/* Destination state. */
		if ( trans->action != 0 && trans->action->anyCurStateRef() )
			out << "_ps = " << CS() << ";";
		out << CS() << " = " << trans->targ->id << "; ";

		if ( trans->action != 0 ) {
			/* Write out the transition func. */
			out << "goto f" << trans->action->actListId << ";\n";
		}
		else {
			/* No code to execute, just loop around. */
			out << "goto _again;\n";
		}
	}
	return out;
}

std::ostream &GotoCodeGen::EXEC_FUNCS()
{
	/* Make labels that set acts and jump to execFuncs. Loop func indicies. */
	for ( ActionTableMap::Iter redAct = redFsm->actionMap; redAct.lte(); redAct++ ) {
		if ( redAct->numTransRefs > 0 ) {
			out << "	f" << redAct->actListId << ": " <<
				"_acts = " << ARR_OFF(A(), itoa( redAct->location+1 ) ) << ";"
				" goto execFuncs;\n";
		}
	}

	out <<
		"\n"
		"execFuncs:\n"
		"	_nacts = *_acts++;\n"
		"	while ( _nacts-- > 0 ) {\n"
		"		switch ( *_acts++ ) {\n";
		ACTION_SWITCH();
		SWITCH_DEFAULT() <<
		"		}\n"
		"	}\n"
		"	goto _again;\n";
	return out;
}

unsigned int GotoCodeGen::TO_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->toStateAction != 0 )
		act = state->toStateAction->location+1;
	return act;
}

unsigned int GotoCodeGen::FROM_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->fromStateAction != 0 )
		act = state->fromStateAction->location+1;
	return act;
}

unsigned int GotoCodeGen::EOF_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->eofAction != 0 )
		act = state->eofAction->location+1;
	return act;
}

std::ostream &GotoCodeGen::TO_STATE_ACTIONS()
{
	/* Take one off for the psuedo start state. */
	int numStates = redFsm->stateList.length();
	unsigned int *vals = new unsigned int[numStates];
	memset( vals, 0, sizeof(unsigned int)*numStates );

	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
		vals[st->id] = TO_STATE_ACTION(st);

	out << "\t";
	for ( int st = 0; st < redFsm->nextStateId; st++ ) {
		/* Write any eof action. */
		out << vals[st];
		if ( st < numStates-1 ) {
			out << ", ";
			if ( (st+1) % IALL == 0 )
				out << "\n\t";
		}
	}
	out << "\n";
	delete[] vals;
	return out;
}

std::ostream &GotoCodeGen::FROM_STATE_ACTIONS()
{
	/* Take one off for the psuedo start state. */
	int numStates = redFsm->stateList.length();
	unsigned int *vals = new unsigned int[numStates];
	memset( vals, 0, sizeof(unsigned int)*numStates );

	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
		vals[st->id] = FROM_STATE_ACTION(st);

	out << "\t";
	for ( int st = 0; st < redFsm->nextStateId; st++ ) {
		/* Write any eof action. */
		out << vals[st];
		if ( st < numStates-1 ) {
			out << ", ";
			if ( (st+1) % IALL == 0 )
				out << "\n\t";
		}
	}
	out << "\n";
	delete[] vals;
	return out;
}

std::ostream &GotoCodeGen::EOF_ACTIONS()
{
	/* Take one off for the psuedo start state. */
	int numStates = redFsm->stateList.length();
	unsigned int *vals = new unsigned int[numStates];
	memset( vals, 0, sizeof(unsigned int)*numStates );

	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
		vals[st->id] = EOF_ACTION(st);

	out << "\t";
	for ( int st = 0; st < redFsm->nextStateId; st++ ) {
		/* Write any eof action. */
		out << vals[st];
		if ( st < numStates-1 ) {
			out << ", ";
			if ( (st+1) % IALL == 0 )
				out << "\n\t";
		}
	}
	out << "\n";
	delete[] vals;
	return out;
}

std::ostream &GotoCodeGen::FINISH_CASES()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* States that are final and have an out action need a case. */
		if ( st->eofAction != 0 ) {
			/* Write the case label. */
			out << "\t\tcase " << st->id << ": ";

			/* Write the goto func. */
			out << "goto f" << st->eofAction->actListId << ";\n";
		}
	}
	
	return out;
}

void GotoCodeGen::GOTO( ostream &ret, int gotoDest, bool inFinish )
{
	ret << "{" << CS() << " = " << gotoDest << "; " << 
			CTRL_FLOW() << "goto _again;}";
}

void GotoCodeGen::GOTO_EXPR( ostream &ret, InlineItem *ilItem, bool inFinish )
{
	ret << "{" << CS() << " = (";
	INLINE_LIST( ret, ilItem->children, 0, inFinish );
	ret << "); " << CTRL_FLOW() << "goto _again;}";
}

void GotoCodeGen::CURS( ostream &ret, bool inFinish )
{
	ret << "(_ps)";
}

void GotoCodeGen::TARGS( ostream &ret, bool inFinish, int targState )
{
	ret << "(" << CS() << ")";
}

void GotoCodeGen::NEXT( ostream &ret, int nextDest, bool inFinish )
{
	ret << CS() << " = " << nextDest << ";";
}

void GotoCodeGen::NEXT_EXPR( ostream &ret, InlineItem *ilItem, bool inFinish )
{
	ret << CS() << " = (";
	INLINE_LIST( ret, ilItem->children, 0, inFinish );
	ret << ");";
}

void GotoCodeGen::CALL( ostream &ret, int callDest, int targState, bool inFinish )
{
	ret << "{" << STACK() << "[" << TOP() << "++] = " << CS() << "; " << CS() << " = " << 
			callDest << "; " << CTRL_FLOW() << "goto _again;}";
}

void GotoCodeGen::CALL_EXPR( ostream &ret, InlineItem *ilItem, int targState, bool inFinish )
{
	ret << "{" << STACK() << "[" << TOP() << "++] = " << CS() << "; " << CS() << " = (";
	INLINE_LIST( ret, ilItem->children, targState, inFinish );
	ret << "); " << CTRL_FLOW() << "goto _again;}";
}

void GotoCodeGen::RET( ostream &ret, bool inFinish )
{
	ret << "{" << CS() << " = " << STACK() << "[--" << TOP() << "]; " << 
			CTRL_FLOW() << "goto _again;}";
}

void GotoCodeGen::BREAK( ostream &ret, int targState )
{
	outLabelUsed = true;
	ret << CTRL_FLOW() << "goto _out;";
}

void GotoCodeGen::writeData()
{
	if ( redFsm->anyActions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActArrItem), A() );
		ACTIONS_ARRAY();
		CLOSE_ARRAY() <<
		"\n";
	}

	if ( redFsm->anyToStateActions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActionLoc), TSA() );
		TO_STATE_ACTIONS();
		CLOSE_ARRAY() <<
		"\n";
	}

	if ( redFsm->anyFromStateActions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActionLoc), FSA() );
		FROM_STATE_ACTIONS();
		CLOSE_ARRAY() <<
		"\n";
	}

	if ( redFsm->anyEofActions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActionLoc), EA() );
		EOF_ACTIONS();
		CLOSE_ARRAY() <<
		"\n";
	}

	STATE_IDS();
}

void GotoCodeGen::writeExec()
{
	outLabelUsed = false;

	out << "	{\n";

	if ( redFsm->anyRegCurStateRef() )
		out << "	int _ps = 0;\n";

	if ( redFsm->anyToStateActions() || redFsm->anyRegActions() 
			|| redFsm->anyFromStateActions() )
	{
		out << 
			"	" << PTR_CONST() << ARRAY_TYPE(redFsm->maxActArrItem) << POINTER() << "_acts;\n"
			"	" << UINT() << " _nacts;\n";
	}

	if ( redFsm->anyConditions() )
		out << "	" << WIDE_ALPH_TYPE() << " _widec;\n";

	out << "\n";

	if ( hasEnd ) {
		outLabelUsed = true;
		out << 
			"	if ( " << P() << " == " << PE() << " )\n"
			"		goto _out;\n";
	}

	out << "_resume:\n";

	if ( redFsm->anyFromStateActions() ) {
		out <<
			"	_acts = " << ARR_OFF( A(), FSA() + "[" + CS() + "]" ) << ";\n"
			"	_nacts = " << CAST(UINT()) << " *_acts++;\n"
			"	while ( _nacts-- > 0 ) {\n"
			"		switch ( *_acts++ ) {\n";
			FROM_STATE_ACTION_SWITCH();
			SWITCH_DEFAULT() <<
			"		}\n"
			"	}\n"
			"\n";
	}

	out <<
		"	switch ( " << CS() << " ) {\n";
		STATE_GOTOS();
		SWITCH_DEFAULT() <<
		"	}\n"
		"\n";
		TRANSITIONS() <<
		"\n";

	if ( redFsm->anyRegActions() )
		EXEC_FUNCS() << "\n";

	out << "_again:\n";

	if ( redFsm->anyToStateActions() ) {
		out <<
			"	_acts = " << ARR_OFF( A(), TSA() + "[" + CS() + "]" ) << ";\n"
			"	_nacts = " << CAST(UINT()) << " *_acts++;\n"
			"	while ( _nacts-- > 0 ) {\n"
			"		switch ( *_acts++ ) {\n";
			TO_STATE_ACTION_SWITCH();
			SWITCH_DEFAULT() <<
			"		}\n"
			"	}\n"
			"\n";
	}

	if ( hasEnd ) {
		out << 
			"	if ( ++" << P() << " != " << PE() << " )\n"
			"		goto _resume;\n";
	}
	else {
		out << 
			"	" << P() << " += 1;\n"
			"	goto _resume;\n";
	}

	if ( outLabelUsed )
		out << "	_out: {}\n";

	out << "	}\n";
}

void GotoCodeGen::writeEOF()
{
	if ( redFsm->anyEofActions() ) {
		out << 
			"	{\n"
			"	" << PTR_CONST() << ARRAY_TYPE(redFsm->maxActArrItem) << POINTER() << "_acts = " << 
					ARR_OFF( A(), EA() + "[" + CS() + "]" ) << ";\n"
			"	" << UINT() << " _nacts = " << CAST(UINT()) << " *_acts++;\n"
			"	while ( _nacts-- > 0 ) {\n"
			"		switch ( *_acts++ ) {\n";
			EOF_ACTION_SWITCH();
			SWITCH_DEFAULT() <<
			"		}\n"
			"	}\n"
			"	}\n"
			"\n";
	}
}
